// LightShadows.cpp: implementation of the CLightShadows class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "LightShadows.h"
#include "../xrRender/LightTrack.h"
#include "../../xrEngine/xr_object.h"
#include "../xrRender/fbasicvisual.h"
#include "../../xrEngine/CustomHUD.h"
#include <xmmintrin.h>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#include "../../xrEngine/Fmesh.h"
#include "../xrRender/SkeletonCustom.h"
#include "../xrRender/SkeletonX.h"
#include "../xrRender/FSkinned.h"
#include "../../xrEngine/xrSkinning.h"

const float S_distance = 256;
const float S_distance2 = S_distance * S_distance;
const float S_ideal_size = 4.f; // ideal size for the object
const float S_fade = 4.5;
const float S_fade2 = S_fade * S_fade;

const float S_level = .05f; // clip by energy level
const int S_size = 85;
const int S_rt_size = 512;
const int batch_size = 256;
const float S_tess = .5f;
const int S_ambient = 32;
const int S_clip = 256 - 8;
const D3DFORMAT S_rtf = D3DFMT_A8R8G8B8;
const float S_blur_kernel = 0.75f;

const u32 cache_old = 30 * 1000; // 30 secs

// PLC constants
const float PLC_S_distance = 48;
const float PLC_S_distance2 = PLC_S_distance * PLC_S_distance;
const float PLC_S_fade = 4.5;
const float PLC_S_fade2 = PLC_S_fade * PLC_S_fade;

// Helper class for tessellation
class Tesselator {
public:
    xr_vector<CLightShadows::tess_tri> tris;

    void clear() { tris.clear(); }

    void tessellate(const Fvector* v, const Fvector& view, float tess_size) {
        // Simple pass through for now 
        // TODO: Implement recursive tessellation maybe?
        CLightShadows::tess_tri T;
        T.v[0] = v[0];
        T.v[1] = v[1];
        T.v[2] = v[2];
        // Calculate Normal (assuming CCW)
        T.N.mknormal(v[0], v[1], v[2]);
        tris.push_back(T);
    }
};

static Tesselator tesselator;

#include <emmintrin.h>

// Packed SSE helper for 3 vertices (replaces scalar PLC_energy_SSE and iCeil_SSE)
void PLC_calc3(int& c0, int& c1, int& c2, CRenderDevice& Device, Fvector* P, Fvector& N, light* L, float energy, Fvector& O)
{
	// Load P[0], P[1], P[2] into SoA registers
	// P is array of 3 Fvectors: x0 y0 z0, x1 y1 z1, x2 y2 z2
	// Layout in register (index 0 to 3): P0, P1, P2, 0
	__m128 Px = _mm_set_ps(0.0f, P[2].x, P[1].x, P[0].x);
	__m128 Py = _mm_set_ps(0.0f, P[2].y, P[1].y, P[0].y);
	__m128 Pz = _mm_set_ps(0.0f, P[2].z, P[1].z, P[0].z);

	__m128 Nx = _mm_set1_ps(N.x);
	__m128 Ny = _mm_set1_ps(N.y);
	__m128 Nz = _mm_set1_ps(N.z);

	__m128 zero = _mm_setzero_ps();
	__m128 one = _mm_set1_ps(1.0f);
	__m128 E_vec;

	if (L->flags.type == IRender_Light::DIRECT)
	{
		Fvector Ldir;
		Ldir.invert(L->direction);
		float D = Ldir.dotproduct(N);
		float E_val = (D <= 0) ? 0.0f : energy;
		E_vec = _mm_set1_ps(E_val);
	}
	else
	{
		// Point light
		__m128 Lx = _mm_set1_ps(L->position.x);
		__m128 Ly = _mm_set1_ps(L->position.y);
		__m128 Lz = _mm_set1_ps(L->position.z);
		__m128 Lrange_sq = _mm_set1_ps(L->range * L->range);

		// Ldir = L.position - P
		__m128 LdirX = _mm_sub_ps(Lx, Px);
		__m128 LdirY = _mm_sub_ps(Ly, Py);
		__m128 LdirZ = _mm_sub_ps(Lz, Pz);

		// sqD = Ldir.Ldir
		__m128 sqD = _mm_add_ps(_mm_add_ps(_mm_mul_ps(LdirX, LdirX), _mm_mul_ps(LdirY, LdirY)), _mm_mul_ps(LdirZ, LdirZ));

		// if (sqD > Lrange_sq) E = 0
		__m128 mask_range = _mm_cmple_ps(sqD, Lrange_sq);

		// Normalize Ldir: Ldir * rsqrt(sqD)
		__m128 rcpr = _mm_rsqrt_ps(sqD);
		
		LdirX = _mm_mul_ps(LdirX, rcpr);
		LdirY = _mm_mul_ps(LdirY, rcpr);
		LdirZ = _mm_mul_ps(LdirZ, rcpr);

		// Dot = Ldir . N
		__m128 D = _mm_add_ps(_mm_add_ps(_mm_mul_ps(LdirX, Nx), _mm_mul_ps(LdirY, Ny)), _mm_mul_ps(LdirZ, Nz));
		
		// if (D <= 0) E = 0
		__m128 mask_dot = _mm_cmpgt_ps(D, zero);

		// Attenuation
		// rcpr_plus_1 = rcpr + 1.0
		__m128 rcpr_plus_1 = _mm_add_ps(rcpr, one);
		// att = rcpr / rcpr_plus_1
		__m128 att = _mm_div_ps(rcpr, rcpr_plus_1);

		// Final E = energy * att
		E_vec = _mm_mul_ps(_mm_set1_ps(energy), att);
		
		// Apply masks
		E_vec = _mm_and_ps(E_vec, mask_range);
		E_vec = _mm_and_ps(E_vec, mask_dot);
	}

	// C1 = clampr(DistToCam / PLC_S_distance2, 0, 1)
	__m128 Cx = _mm_set1_ps(Device.vCameraPosition.x);
	__m128 Cy = _mm_set1_ps(Device.vCameraPosition.y);
	__m128 Cz = _mm_set1_ps(Device.vCameraPosition.z);
	
	__m128 Dx = _mm_sub_ps(Px, Cx);
	__m128 Dy = _mm_sub_ps(Py, Cy);
	__m128 Dz = _mm_sub_ps(Pz, Cz);
	
	__m128 dist_cam_sq = _mm_add_ps(_mm_add_ps(_mm_mul_ps(Dx, Dx), _mm_mul_ps(Dy, Dy)), _mm_mul_ps(Dz, Dz));
	__m128 C1 = _mm_mul_ps(dist_cam_sq, _mm_set1_ps(1.0f / PLC_S_distance2));
	C1 = _mm_min_ps(_mm_max_ps(C1, zero), one);

	// C2 = clampr(DistToObj / PLC_S_fade2, 0, 1)
	__m128 Ox_vec = _mm_set1_ps(O.x);
	__m128 Oy_vec = _mm_set1_ps(O.y);
	__m128 Oz_vec = _mm_set1_ps(O.z);
	
	Dx = _mm_sub_ps(Px, Ox_vec);
	Dy = _mm_sub_ps(Py, Oy_vec);
	Dz = _mm_sub_ps(Pz, Oz_vec);
	
	__m128 dist_obj_sq = _mm_add_ps(_mm_add_ps(_mm_mul_ps(Dx, Dx), _mm_mul_ps(Dy, Dy)), _mm_mul_ps(Dz, Dz));
	__m128 C2 = _mm_mul_ps(dist_obj_sq, _mm_set1_ps(1.0f / PLC_S_fade2));
	C2 = _mm_min_ps(_mm_max_ps(C2, zero), one);

	// A = 1 - 1.5 * E * (1 - C1) * (1 - C2)
	__m128 term1 = _mm_sub_ps(one, C1);
	__m128 term2 = _mm_sub_ps(one, C2);
	
	__m128 A = _mm_mul_ps(_mm_set1_ps(1.5f), E_vec);
	A = _mm_mul_ps(A, term1);
	A = _mm_mul_ps(A, term2);
	A = _mm_sub_ps(one, A);

	// c = iCeil(255 * clamp(A, 0, 1))
	A = _mm_min_ps(_mm_max_ps(A, zero), one);
	A = _mm_mul_ps(A, _mm_set1_ps(255.0f));
	
	// Convert to int (nearest)
	__m128i res = _mm_cvtps_epi32(A);
	
	// Extract
	alignas(16) int out[4];
	_mm_store_si128((__m128i*)out, res);
	c0 = out[0];
	c1 = out[1];
	c2 = out[2];
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CLightShadows::CLightShadows()
{
	current = 0;
	RT = 0;

	LPCSTR RTname = "$user$shadow";
	LPCSTR RTtemp = "$user$temp";
	string128 RTname2;
	strconcat(sizeof(RTname2), RTname2, RTname, ",", RTname);
	string128 RTtemp2;
	strconcat(sizeof(RTtemp2), RTtemp2, RTtemp, ",", RTtemp);

	// 
	RT.create(RTname, S_rt_size, S_rt_size, S_rtf);
	RT_temp.create(RTtemp, S_rt_size, S_rt_size, S_rtf);
	sh_World.create("effects\\shadow_world", RTname);
	geom_World.create(FVF::F_LIT, RCache.Vertex.Buffer(), NULL);
	sh_BlurTR.create("blur4", RTtemp2);
	sh_BlurRT.create("blur4", RTname2);
	geom_Blur.create(FVF::F_TL4uv, RCache.Vertex.Buffer(), RCache.QuadIB);

	// Debug
	sh_Screen.create("effects\\screen_set", RTname);
	geom_Screen.create(FVF::F_TL, RCache.Vertex.Buffer(), RCache.QuadIB);
}

CLightShadows::~CLightShadows()
{
	// Debug
	sh_Screen.destroy();
	geom_Screen.destroy();

	geom_Blur.destroy();
	geom_World.destroy();

	sh_BlurRT.destroy();
	sh_BlurTR.destroy();
	sh_World.destroy();
	RT_temp.destroy();
	RT.destroy();

	// casters
	for (u32 it = 0; it < casters_pool.size(); it++)
		xr_delete(casters_pool[it]);
	casters_pool.clear();

	// cache
	for (u32 it = 0; it < cache.size(); it++)
		xr_free(cache[it].tris);
	cache.clear();
}

void CLightShadows::set_object(IRenderable* O)
{
	if (0 == O) current = 0;
	else
	{
		if (!O->renderable_ShadowGenerate() || RImplementation.val_bHUD || ((CROS_impl*)O->renderable_ROS())->
			shadow_gen_frame == Device.dwFrame)
		{
			current = 0;
			return;
		}

		const vis_data& vis = O->renderable.visual->getVisData();
		Fvector C;
		O->renderable.xform.transform_tiny(C, vis.sphere.P);
		float R = vis.sphere.R;
		float D = C.distance_to(Device.vCameraPosition) + R;
		// D=0 -> P=0; 
		// R<S_ideal_size -> P=max, R>S_ideal_size -> P=min
		float _priority = (D / S_distance) * (S_ideal_size / (R + EPS));
		if (_priority < 1.f) current = O;
		else current = 0;

		if (current)
		{
			((CROS_impl*)O->renderable_ROS())->shadow_gen_frame = Device.dwFrame;

			// alloc
			caster* cs = NULL;
			if (casters_pool.empty()) cs = xr_new<caster>();
			else
			{
				cs = casters_pool.back();
				casters_pool.pop_back();
			}

			// 
			casters.push_back(cs);
			cs->O = current;
			cs->C = C;
			cs->D = D;
			cs->nodes.clear();
		}
	}
}

void CLightShadows::add_element(NODE& N)
{
	if (0 == current) return;
	VERIFY2(casters.back()->nodes.size()<24, "Object exceeds limit of 24 renderable parts/materials");
	if (0 == N.pVisual->shader->E[SE_R1_LMODELS]._get()) return;
	casters.back()->nodes.push_back(N);
}

//
void CLightShadows::calculate()
{
#ifdef _GPA_ENABLED
		TAL_SCOPED_TASK_NAMED( "CLightShadows::calculate()" );
#endif // _GPA_ENABLED

	if (casters.empty()) return;

	BOOL bRTS = FALSE;
	Device.Statistic->RenderDUMP_Scalc.Begin();
	HW.pDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);

	// iterate on objects
	int slot_id = 0;
	int slot_line = S_rt_size / S_size;
	int slot_max = slot_line * slot_line;
	const float eps = 2 * EPS_L;
	for (u32 o_it = 0; o_it < casters.size(); o_it++)
	{
		caster& C = *casters[o_it];
		if (C.nodes.empty()) continue;

		// Select lights and calc importance
		CROS_impl* LT = (CROS_impl*)C.O->renderable_ROS();
		xr_vector<CROS_impl::Light>& lights = LT->lights;

		// iterate on lights
		for (u32 l_it = 0; (l_it < lights.size()) && (slot_id < slot_max); l_it++)
		{
			CROS_impl::Light& L = lights[l_it];
			if (L.energy < S_level) continue;

			//Msg	("~ light: %d",l_it);

			// setup rt+state(s) for first use
			if (!bRTS)
			{
				bRTS = TRUE;
				RCache.set_RT(RT_temp->pRT);
				RCache.set_ZB(RImplementation.Target->pTempZB);
				HW.pDevice->Clear(0, 0,D3DCLEAR_TARGET,D3DCOLOR_XRGB(255, 255, 255), 1, 0);
			}

			// calculate light center
			Fvector Lpos = L.source->position;
			float Lrange = L.source->range;
			//Log	("* l-pos:",Lpos);
			//Msg	("* l-range: %f",Lrange);
			if (L.source->flags.type == IRender_Light::DIRECT)
			{
				// Msg		(" -direct- : %f",L.energy);
				Lpos.mul(L.source->direction, -100);
				Lpos.add(C.C);
				Lrange = 120;
			}
			else
			{
				// Msg		(" -point- : %f",L.energy);
			}

			// calculate "shadow"
			// calculate projection-matrix
			Fmatrix mProject, mModel;
			float s_d = C.C.distance_to(Lpos);
			float s_r = C.O->renderable.visual->getVisData().sphere.R;
			float s_a = 2 * asin(s_r / s_d);
			//float	s_a		=	deg2rad(30);
			mProject.build_projection_HAT(s_a, 1.f, s_d - s_r - eps, s_d + s_r + eps);
			//mProject.build_projection_hat	(s_a,1.f,0.1f,1000.f);
			RCache.set_xform_project(mProject);

			// calculate view-matrix
			Fvector v_from;
			Fvector v_to;
			Fvector v_up;
			v_from.set(Lpos);
			v_to.set(C.C);
			v_up.set(0, 1, 0);
			if (_abs(v_up.dotproduct(v_to)) > .99f) v_up.set(0, 0, 1);
			mModel.build_camera_dir(v_from, v_to, v_up);
			RCache.set_xform_view(mModel);

			// render object to temp-surface
			// 
			for (u32 n_it = 0; n_it < C.nodes.size(); n_it++)
				{
					NODE& N = C.nodes[n_it];
					RCache.set_xform_world(N.Matrix);
					RCache.set_Element(N.pVisual->shader->E[SE_R1_LMODELS]);
					RCache.set_CullMode(CULL_CW); // inverted?
					N.pVisual->Render(1.f);
				}
			RCache.set_CullMode(CULL_CCW);

			// t-stage 0
			// C.O->renderable.visual->Render	(1.f);

			// blur and merge with main
			// 1. setup matrices
			// 2. setup vb/ib/sw
			// 3. actual rendering

			// 1.
			//		per-light-view
			Fmatrix& m_View = mModel;
			//		per-light-proj
			Fmatrix& m_Proj = mProject;

			// 2.
			Fvector Le, TT_N, TT_O;
			Le.set(L.color.r, L.color.g, L.color.b);
			Le.mul(L.energy);
			TT_O.set(C.C);
			// TT_N.sub(C.C,Lpos);	TT_N.normalize();

			//
			u32 tri_count = 0;
			u32 v_offset;
			u32 i_offset;
			CLightShadows::cache_item* CI = 0;

			// Search cache
			for (u32 c_it = 0; c_it < cache.size(); c_it++)
			{
				if (cache[c_it].O == C.O)
				{
					CI = &cache[c_it];
					CI->time = Device.dwTimeGlobal;
					break;
				}
			}

			// Create if not found
			if (0 == CI)
			{
				if (cache.size() < cache_old)
				{
					cache.push_back(cache_item());
					CI = &cache.back();
					CI->tris = (tess_tri*)xr_malloc(S_clip * sizeof(tess_tri));
				}
				else
				{
					// search LRU
					u32 time = 0xffffffff;
					u32 who = 0xffffffff;
					for (u32 c_it = 0; c_it < cache.size(); c_it++)
					{
						if (cache[c_it].time < time)
						{
							time = cache[c_it].time;
							who = c_it;
						}
					}
					CI = &cache[who];
				}
				CI->O = C.O;
				CI->time = Device.dwTimeGlobal;
				CI->tcnt = 0;

				// Recalculate
				// 1. 
				Fvector O_view;
				m_View.transform_tiny(O_view, C.C);

				// 4.
				tesselator.clear();
				
				for (u32 n_it = 0; n_it < C.nodes.size(); n_it++)
				{
					NODE& N = C.nodes[n_it];
					dxRender_Visual* V = N.pVisual;
					if (V->Type == MT_SKELETON_ANIM || V->Type == MT_SKELETON_RIGID)
					{
						CKinematics* K = (CKinematics*)V;
						const SkeletonWMVec& wallmarks = K->GetWallmarks();
						for (SkeletonWMVec::const_iterator wm_it = wallmarks.begin(); wm_it != wallmarks.end(); ++wm_it)
						{
							CSkeletonWallmark* wm = &**wm_it;
							for (CSkeletonWallmark::WMFacesVecIt f_it = wm->m_Faces.begin(); f_it != wm->m_Faces.end(); ++f_it)
							{
								CSkeletonWallmark::WMFace& F = *f_it;
								Fvector v[3];
								// Skin vertices to World Space
								for (int k = 0; k < 3; k++)
								{
									u16 bone_id = F.bone_id[k][0];
									const Fmatrix& M = K->LL_GetBoneInstance(bone_id).mRenderTransform;
									M.transform_tiny(v[k], F.vert[k]);
								}

								// Transform to Light View Space
								m_View.transform_tiny(v[0]);
								m_View.transform_tiny(v[1]);
								m_View.transform_tiny(v[2]);

								if (v[0].z < 0.01f || v[1].z < 0.01f || v[2].z < 0.01f) continue;

								tesselator.tessellate(v, O_view, S_tess);
							}
						}
					}
				}

				// 5.
				for (u32 t_it = 0; t_it < tesselator.tris.size(); t_it++)
				{
					if (CI->tcnt >= S_clip) break;
					CI->tris[CI->tcnt] = tesselator.tris[t_it];
					CI->tcnt++;
				}
			}

			// Tesselate
			tri_count = CI->tcnt;
			if (tri_count)
			{
				FVF::LIT* v = (FVF::LIT*)RCache.Vertex.Lock(tri_count * 3, geom_World->vb_stride, v_offset);
				
				// TBB Parallel Loop
				tbb::parallel_for(tbb::blocked_range<u32>(0, tri_count),
					[&](const tbb::blocked_range<u32>& r) {
						for (u32 t_it = r.begin(); t_it != r.end(); ++t_it)
						{
							tess_tri& TT = CI->tris[t_it];
							int c0, c1, c2;
							// Note: Passing TT.v as P. TT.v is Fvector[3].
							PLC_calc3(c0, c1, c2, Device, TT.v, TT.N, L.source, L.energy, (Fvector&)C.C);
							
							// Access v array safely (disjoint access)
							FVF::LIT* current_v = v + t_it * 3;
							current_v[0].set(TT.v[0], c0, 0, 0); // u,v are 0?
							current_v[1].set(TT.v[1], c1, 0, 0);
							current_v[2].set(TT.v[2], c2, 0, 0);
						}
					}
				);

				RCache.Vertex.Unlock(tri_count * 3, geom_World->vb_stride);

				// set RT and States
				if (bRTS)
				{
					bRTS = FALSE;
					RCache.set_RT(RT->pRT);
					RCache.set_ZB(RImplementation.Target->pTempZB);
				}

				// set global offset
				float _w = float(S_rt_size);
				float _h = float(S_rt_size);
				int _c = slot_id % slot_line;
				int _r = slot_id / slot_line;
				float _cx = float(_c) * S_size;
				float _cy = float(_r) * S_size;

				// render
				// RCache.set_Shader			(sh_World);
				RCache.set_Element(sh_World->E[0]);
				RCache.set_Geometry(geom_World);
				Fmatrix m_Offset;
				m_Offset.translate(_cx, _cy, 0);
				Fmatrix m_World;
				m_World.mul(m_Offset, mProject);
				RCache.set_xform_world(m_World);
				RCache.set_xform_view(Fidentity);
				RCache.set_xform_project(Fidentity);
				RCache.set_c("m_head", m_World);
				RCache.Render(D3DPT_TRIANGLELIST, v_offset, 0, tri_count * 3, 0, tri_count);

				// increment slot
				slot_id++;
			}
		}
	}

	Device.Statistic->RenderDUMP_Scalc.End();
	HW.pDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
}
