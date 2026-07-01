#include "stdafx.h"
#include "../../xrEngine/igame_persistent.h"
#include "../xrRender/FBasicVisual.h"
#include "../../xrEngine/customhud.h"
#include "../../xrEngine/xr_object.h"
#include "../xrRender/SkeletonCustom.h"
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#include <tbb/parallel_sort.h>

#include "../xrRender/QueryHelper.h"
#include "../xrRender/r__dsgraph_build.cpp"
#include "../xrGame/debug_renderer.h"
#if USE_DX11
#include "../../gamedata/shaders/r3/scope_defines.h"
#endif

IC bool pred_sp_sort(ISpatial* _1, ISpatial* _2)
{
	float d1 = _1->spatial.sphere.P.distance_to_sqr(Device.frame_data.vCameraPosition);
	float d2 = _2->spatial.sphere.P.distance_to_sqr(Device.frame_data.vCameraPosition);
	return d1 < d2;
}

void CRender::render_main(Fmatrix& m_ViewProjection, bool _fportals)
{
	PIX_EVENT(render_main);
	//	Msg						("---begin");
	marker ++;

	// Calculate sector(s) and their objects
	if (pLastSector)
	{
		//!!!
		//!!! BECAUSE OF PARALLEL HOM RENDERING TRY TO DELAY ACCESS TO HOM AS MUCH AS POSSIBLE
		//!!!
		{
			// Traverse object database
			if (lstRenderables_frame != Device.dwFrame)
			{
				g_SpatialSpace->q_frustum
				(
					lstRenderablesMain,
					ISpatial_DB::O_ORDERED,
					STYPE_RENDERABLE + STYPE_LIGHTSOURCE,
					ViewBase
				);

				// (almost) Exact sorting order (front-to-back)
				tbb::parallel_sort(lstRenderablesMain.begin(), lstRenderablesMain.end(), pred_sp_sort);

				lstRenderables_frame = Device.dwFrame;
			}

			// Determine visibility for dynamic part of scene
			set_Object(0);
			u32 uID_LTRACK = 0xffffffff;
			if (phase == PHASE_NORMAL)
			{
				uLastLTRACK ++;
				if (lstRenderablesMain.size()) uID_LTRACK = uLastLTRACK % lstRenderablesMain.size();

				// update light-vis for current entity / actor
				CObject* O = g_pGameLevel->CurrentViewEntity();
				if (O)
				{
					CROS_impl* R = (CROS_impl*)O->ROS();
					if (R) R->update(O);
				}

				// update light-vis for selected entity
				// track lighting environment
				if (lstRenderablesMain.size())
				{
					IRenderable* renderable = lstRenderablesMain[uID_LTRACK]->dcast_Renderable();
					if (renderable)
					{
						CROS_impl* T = (CROS_impl*)renderable->renderable_ROS();
						if (T) T->update(renderable);
					}
				}
			}
		}

		// Traverse sector/portal structure
		PortalTraverser.traverse
		(
			pLastSector,
			ViewBase,
            Device.frame_data.vCameraPosition,
			m_ViewProjection,
			CPortalTraverser::VQ_HOM + CPortalTraverser::VQ_SSA + CPortalTraverser::VQ_FADE
			//. disabled scissoring (HW.Caps.bScissor?CPortalTraverser::VQ_SCISSOR:0)	// generate scissoring info
		);

		// Determine visibility for static geometry hierrarhy
		for (u32 s_it = 0; s_it < PortalTraverser.r_sectors.size(); s_it++)
		{
			CSector* sector = (CSector*)PortalTraverser.r_sectors[s_it];
			dxRender_Visual* root = sector->root();
			for (u32 v_it = 0; v_it < sector->r_frustums.size(); v_it++)
			{
				set_Frustum(&(sector->r_frustums[v_it]));
				add_Geometry(root);
			}
		}

		// skeleton pre calc before frustum
		{
			xr_vector<CKinematics*> skeletons_to_update;
			skeletons_to_update.reserve(lstRenderablesMain.size());

			for (u32 o_it = 0; o_it < lstRenderablesMain.size(); o_it++)
			{
				ISpatial* spatial = lstRenderablesMain[o_it];
				spatial->spatial_updatesector();
				CSector* sector = (CSector*)spatial->spatial.sector;
				if (!sector) continue;

				if (PortalTraverser.i_marker != sector->r_marker) continue;

				if (!(spatial->spatial.type & STYPE_RENDERABLE)) continue;

				// Check if it is a skeleton
				IRenderable* renderable = spatial->dcast_Renderable();
				if (!renderable) continue;

				dxRender_Visual* V = (dxRender_Visual*)renderable->renderable.visual;
				if (V->Type != MT_SKELETON_ANIM && V->Type != MT_SKELETON_RIGID) continue;

				// Check frustums
				bool visible_in_frustum = false;
				for (u32 v_it = 0; v_it < sector->r_frustums.size(); v_it++)
				{
					CFrustum& view = sector->r_frustums[v_it];
					if (view.testSphere_dirty(spatial->spatial.sphere.P, spatial->spatial.sphere.R)) {
						visible_in_frustum = true;
						break;
					}
				}
				if (!visible_in_frustum) continue;

				// Check HOM
				vis_data& v_orig = V->vis;
				if (Device.dwFrame < v_orig.hom_frame) {
					skeletons_to_update.push_back((CKinematics*)V);
				}
				else {
					vis_data v_copy = v_orig;
					v_copy.box.xform(renderable->renderable.xform);
					if (HOM.visible(v_copy.box)) {
						skeletons_to_update.push_back((CKinematics*)V);
					}
				}
			}

			tbb::parallel_for(tbb::blocked_range<size_t>(0, skeletons_to_update.size()),
				[&](const tbb::blocked_range<size_t>& range) {
					for (size_t i = range.begin(); i != range.end(); ++i) {
						skeletons_to_update[i]->CalculateBones(TRUE);
					}
				});
		}

		// Traverse frustums
		for (u32 o_it = 0; o_it < lstRenderablesMain.size(); o_it++)
		{
			ISpatial* spatial = lstRenderablesMain[o_it];
			spatial->spatial_updatesector();
			CSector* sector = (CSector*)spatial->spatial.sector;
			if (0 == sector) continue; // disassociated from S/P structure

			if (spatial->spatial.type & STYPE_LIGHTSOURCE)
			{
				// lightsource
				light* L = (light*)(spatial->dcast_Light());
				VERIFY(L);
				float lod = L->get_LOD();
				if (lod > EPS_L)
				{
					vis_data& vis = L->get_homdata();
					if (HOM.visible(vis)) Lights.add_light(L);
				}
				continue ;
			}

			if (PortalTraverser.i_marker != sector->r_marker) continue; // inactive (untouched) sector
			for (u32 v_it = 0; v_it < sector->r_frustums.size(); v_it++)
			{
				CFrustum& view = sector->r_frustums[v_it];
				if (!view.testSphere_dirty(spatial->spatial.sphere.P, spatial->spatial.sphere.R)) continue;

				if (spatial->spatial.type & STYPE_RENDERABLE)
				{
					// renderable
					IRenderable* renderable = spatial->dcast_Renderable();
					VERIFY(renderable);

					// Occlusion
					//	casting is faster then using getVis method
					vis_data& v_orig = ((dxRender_Visual*)renderable->renderable.visual)->vis;
					vis_data v_copy = v_orig;
					v_copy.box.xform(renderable->renderable.xform);
					BOOL bVisible = HOM.visible(v_copy);
					v_orig.marker = v_copy.marker;
					v_orig.accept_frame = v_copy.accept_frame;
					v_orig.hom_frame = v_copy.hom_frame;
					v_orig.hom_tested = v_copy.hom_tested;
					if (!bVisible) break; // exit loop on frustums

					// Rendering
					set_Object(renderable);
					renderable->renderable_Render();
					set_Object(0);
				}
				break; // exit loop on frustums
			}
		}
		if (g_pGameLevel && (phase == PHASE_NORMAL))
		{
			Target->bCaptureScopeLens = true;
			g_hud->Render_Last(); // HUD
			Target->bCaptureScopeLens = false;
			if (g_hud->RenderActiveItemUIQuery())
				r_dsgraph_render_hud_ui();
			if (g_hud->RenderCamAttachedUIQuery())
				r_dsgraph_render_cam_ui();
		}
	}
	else
	{
		set_Object(0);
		if (g_pGameLevel && (phase == PHASE_NORMAL))
		{
			Target->bCaptureScopeLens = true;
			g_hud->Render_Last(); // HUD
			Target->bCaptureScopeLens = false;
			if (g_hud->RenderActiveItemUIQuery())
				r_dsgraph_render_hud_ui();
			if (g_hud->RenderCamAttachedUIQuery())
				r_dsgraph_render_cam_ui();
		}
	}
}

void CRender::render_menu()
{
	PIX_EVENT(render_menu);
	//	Globals
	RCache.set_CullMode(CULL_CCW);
	RCache.set_Stencil(FALSE);
	RCache.set_ColorWriteEnable();

	// Main Render
	{
		Target->u_setrt(Target->rt_Generic_0, 0, 0, HW.pBaseZB); // LDR RT
		g_pGamePersistent->OnRenderPPUI_main(); // PP-UI
	}

	// Distort
	{
		FLOAT ColorRGBA[4] = {127.0f / 255.0f, 127.0f / 255.0f, 0.0f, 127.0f / 255.0f};
		Target->u_setrt(Target->rt_Generic_1, 0, 0, HW.pBaseZB); // Now RT is a distortion mask
		HW.pContext->ClearRenderTargetView(Target->rt_Generic_1->pRT, ColorRGBA);
		g_pGamePersistent->OnRenderPPUI_PP(); // PP-UI
	}

	// Actual Display
	Target->u_setrt(Device.dwWidth, Device.dwHeight, HW.pBaseRT, nullptr, NULL, HW.pBaseZB);
	RCache.set_Shader(Target->s_menu);
	RCache.set_Geometry(Target->g_menu);

	Fvector2 p0, p1;
	u32 Offset;
	u32 C = color_rgba(255, 255, 255, 255);
	float _w = float(Device.dwWidth);
	float _h = float(Device.dwHeight);
	float d_Z = EPS_S;
	float d_W = 1.f;
	p0.set(.5f / _w, .5f / _h);
	p1.set((_w + .5f) / _w, (_h + .5f) / _h);

	FVF::TL* pv = (FVF::TL*)RCache.Vertex.Lock(4, Target->g_menu->vb_stride, Offset);
	pv->set(EPS, float(_h + EPS), d_Z, d_W, C, p0.x, p1.y);
	pv++;
	pv->set(EPS, EPS, d_Z, d_W, C, p0.x, p0.y);
	pv++;
	pv->set(float(_w + EPS), float(_h + EPS), d_Z, d_W, C, p1.x, p1.y);
	pv++;
	pv->set(float(_w + EPS), EPS, d_Z, d_W, C, p1.x, p0.y);
	pv++;
	RCache.Vertex.Unlock(4, Target->g_menu->vb_stride);
	RCache.Render(D3DPT_TRIANGLELIST, Offset, 0, 4, 0, 2);
}

extern u32 g_r;

void debug_scope(Fmatrix scope_camera) {
	auto draw_circle = [](Fmatrix m, u32 color, bool bHud) -> void {
		int n = 100;
		Fvector v0, s;
		for (int i = 0; i < n + 1; i++) {
			float angle = float(i) / float(n) * PI * 2.0;

			Fvector v1 = { cos(angle), sin(angle), .0f };
			m.transform(v1);
			if (i > 0) CDebugRenderer().draw_line({}, v0, v1, color, bHud);
			v0 = v1;
		}
		};

	auto draw_lens = [draw_circle](CRenderDevice::CSecondVPParams::Lens lens, u32 color) -> void {
		draw_circle(Fmatrix(lens.m_W).mulB_43(Fmatrix().scale(lens.radius, lens.radius, 0.0)), color, true);

		Fvector v0 = { 0, 0, 0 };
		Fvector v1 = { 0, 0, 100 };
		lens.m_W.transform(v0);
		lens.m_W.transform(v1);

		CDebugRenderer().draw_line(Fmatrix().identity(), v0, v1, color, true);
	};

	auto draw_camera = [scope_camera, draw_circle](u32 color) -> void {
		auto cm = 1.0 / 100.0;
		draw_circle(Fmatrix(scope_camera).mulB_43(Fmatrix().scale(.25 * cm, .25 * cm, 0.0)), color, true);
		};

	auto p = Device.m_SecondViewport;

	draw_lens(p.eyepiece, 0xff0000ff);
	draw_lens(p.objective, 0xffffff00);
	draw_camera(0xffffffff);
}

void svpCamera() {
	float svp_fov = g_pGamePersistent->m_pGShaderConstants->hud_params.y * 0.75;
	float _, fov, fNearPlane, fFarPlane;
	Device.matrices[0].mProject.decompose_projection(fov, _, fNearPlane, fFarPlane);

	auto mm = Device.matrices[0];
	
	auto params = Device.m_SecondViewport;

	// Project into NDC space to determine the amount of extra magnification we need to correct scope camera size
	Fvector4 top, bot;
	Fmatrix m_WVP = Fmatrix().mul(mm.mProjectHud, Fmatrix().mul(mm.mView, params.eyepiece.m_W));
	m_WVP.transform(top, {0, params.eyepiece.radius,0, 1});
	m_WVP.transform(bot, {0,-params.eyepiece.radius,0, 1});
	top.div(top.w);
	bot.div(bot.w);
	float scope_height_NDC = abs(top.y - bot.y);
	float screen_height_NDC = 2.0;
	float ratio_magnification = screen_height_NDC / scope_height_NDC;

	// The magnficiation of the scope (1X 4X etc)
	float scope_magnification = fov / deg2rad(svp_fov);

	// The fov we need to render at in order to have correct zoom
	float vFov = 2.0 * atan(tan(fov*0.5) / (ratio_magnification * scope_magnification));

	// The fov we need for camera placement
	float vFovMagOnly = 2.0 * atan(tan(fov * 0.5) / scope_magnification);

	auto camera_offset_from_vfov_and_radius = [](float vFov, float radius) -> float {
		return radius / tan(vFov/2.0);
	};

	Fvector w_P_obj = {0,0,0};
	Fvector w_N_obj = {0,0,1};
	params.objective.m_W.transform(w_P_obj);
	params.objective.m_W.transform_dir(w_N_obj);

	auto near_plane = fNearPlane;
	auto m_W_svpcam = params.eyepiece.m_W;  // By default we use the eyepiece for camera placement
	if (scope_svp_enabled >= 2 && params.objective.radius > EPS) {
		// compute camera for objective lens placement
		// FIXME: This should be the min fov of the scope, not the current fov
		auto d = camera_offset_from_vfov_and_radius(vFovMagOnly, params.objective.radius);
		m_W_svpcam = Fmatrix().mul(params.objective.m_W, Fmatrix().translate(0, 0, -d));

		near_plane = d;
	}

	auto aspect = RImplementation.TargetSVP->Width /  RImplementation.TargetSVP->Height;


	float fNearPlane_hud, fFarPlane_hud;
	Device.matrices[0].mProject.decompose_projection(_, _, fNearPlane_hud, fFarPlane_hud);
	auto svp_proj = Fmatrix().build_projection(vFov, aspect, near_plane, fFarPlane);
	auto svp_proj_hud = Fmatrix().build_projection(vFov, aspect, near_plane, fFarPlane_hud);

	if (scope_debug >= 2)
		debug_scope(m_W_svpcam);

	Device.matrices[1].mView.invert(m_W_svpcam);
	Device.matrices[1].mProject = svp_proj;
	Device.matrices[1].mProjectHud = svp_proj_hud;

	auto& svp_frame = Device.frame_data.viewport[1];
	svp_frame.mView = Device.matrices[1].mView;
	svp_frame.mProject = Device.matrices[1].mProject;
	svp_frame.mProjectHud = Device.matrices[1].mProjectHud;
	svp_frame.mFullTransform.mul(svp_frame.mProject, svp_frame.mView);
	svp_frame.mFullTransformHud.mul(svp_frame.mProjectHud, svp_frame.mView);
}

void CRender::renderGBuffer() {
	PIX_EVENT(RENDER_GBUFFER);
	Device.dwViewport++;
	//******* Main calc - DEFERRER RENDERER
	// Main calc
	Device.Statistic->RenderCALC.Begin();
	r_pmask(true, false, true); // enable priority "0",+ capture wmarks
	if (bSUN && Target == TargetMain) set_Recorder(&main_coarse_structure);
	else set_Recorder(nullptr);
	phase = PHASE_NORMAL;

    //SVP HACK: Use main frame view matrix to prevent rendering the wrong sector
    auto main_ft = Device.frame_data.viewport[0].mFullTransform;
    ViewBase.CreateFromMatrix(main_ft, FRUSTUM_P_LRTB + FRUSTUM_P_FAR);
    View = 0;

    render_main(main_ft, true);
	
	set_Recorder(nullptr);
	r_pmask(true, false); // disable priority "1"
	Device.Statistic->RenderCALC.End();

	Target->phase_scene_prepare();
	/*if (RImplementation.o.ssfx_core) // SSS23: DEPRECATED
	{
		// HUD Masking rendering
		FLOAT ColorRGBA[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
		HW.pContext->ClearRenderTargetView(Target->rt_ssfx_hud->pRT, ColorRGBA);

		Target->u_setrt(Target->rt_ssfx_hud, nullptr, NULL, HW.pBaseZB);
		r_dsgraph_render_hud(true);

		// Reset Depth
		HW.pContext->ClearDepthStencilView(HW.pBaseZB, D3D_CLEAR_DEPTH, 1.0f, 0);
	}*/


	if (RImplementation.o.ssfx_motionvectors)
	{
		PIX_EVENT(ssfx_motionvectors);
		Target->u_setrt(Device.dwWidth, Device.dwHeight, 0, 0, Target->rt_ssfx_motion_vectors->pRT, 0);

		FLOAT ColorRGBA[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		HW.pContext->ClearRenderTargetView(Target->rt_ssfx_motion_vectors->pRT, ColorRGBA);

		RCache.set_Stencil(FALSE);
		g_pGamePersistent->Environment().RenderSky(true);

		RCache.Index.Flush();
		RCache.Vertex.Flush();

		RCache.set_xform_world(Fidentity);
	}

	if (ps_r2_ls_flags.test(R2FLAG_TERRAIN_PREPASS))
	{
		Target->u_setrt(Device.dwWidth, Device.dwHeight, nullptr, NULL, nullptr, !RImplementation.o.dx10_msaa ? Target->baseZB : Target->rt_MSAADepth->pZRT);
		r_dsgraph_render_landscape(0, false);
	}

	BOOL split_the_scene_to_minimize_wait = FALSE;
	if (ps_r2_ls_flags.test(R2FLAG_EXP_SPLIT_SCENE)) split_the_scene_to_minimize_wait = TRUE;

	//******* Main render :: PART-0	-- first
	if (!split_the_scene_to_minimize_wait)
	{
		PIX_EVENT(DEFER_PART0_NO_SPLIT);
		// level, DO NOT SPLIT
		r_dsgraph_render_hud();
		r_dsgraph_render_graph(0);
		r_dsgraph_render_lods(true, true);
		if (Details) {
			PIX_EVENT(CDETAILMANAGER_RENDER);
			Details->Render();
		}
		if (ps_r2_ls_flags.test(R2FLAG_TERRAIN_PREPASS)) r_dsgraph_render_landscape(1, true);
		Target->phase_scene_end();
	}
	else
	{
		PIX_EVENT(DEFER_PART0_SPLIT);
		Target->phase_scene_begin();

		// level, SPLIT

		{
			PIX_EVENT(RENDER_HUD_EARLY);

			auto fakescope = Target == TargetMain && !Device.m_SecondViewport.IsSVPActive();

			if (Target == TargetMain)
			{
				
				{
					PIX_EVENT(SCOPE_WRITE_LENS_DEPTH);
					// Write lens depth
					Target->draw_scope(Target->s_scope_depth_write, [&](auto N) -> void {
						// Mask the lens so we know what to push for svpscope 0
						// This must contain 0x01 to prevent the skybox from rendering into the diffuse buffer
						RCache.set_Stencil(TRUE, D3DCMP_ALWAYS, 0x3, 0x3, 0x3, D3DSTENCILOP_KEEP, D3DSTENCILOP_REPLACE, D3DSTENCILOP_KEEP);
						RCache.set_c("scope_phase", SCOPE_PHASE_GBUFFER); //GBUFFER
						RCache.set_c("scope_depth_value", 0.f);
					});
				}

				svpCamera();
			}

			{
				PIX_EVENT(RENDER_HUD);
				RCache.set_ZFunc(D3DCMP_LESS);
				r_dsgraph_render_hud();
				RCache.set_ZFunc(D3DCMP_LESSEQUAL);
			}

			if (Target == TargetMain && !Device.m_SecondViewport.IsSVPActive())
			{
				PIX_EVENT(SCOPE_HOLEPUNCH);
				// Clear depth anywhere the hud does not occlude the lens
				//   which allows g-buffer to populate for us to sample later.
				Target->draw_scope(Target->s_scope_depth_write, [&](auto _) -> void {
					RImplementation.rmNormal();
					// holepunch everything that is not occluded by closer objects, and is part of the lens
					//    making sure to clear the stencil to zero, so that the skybox can render.
					RCache.set_Stencil(TRUE, D3DCMP_EQUAL, 0x3, 0x3, 0x3, D3DSTENCILOP_KEEP, D3DSTENCILOP_ZERO, D3DSTENCILOP_KEEP);
					RCache.set_c("scope_phase", SCOPE_PHASE_DEPTHWRITE); //DEPTHWRITE
					RCache.set_c("scope_depth_value", 1.f);
				});
			}
		}
		Target->phase_scene_begin();
		r_dsgraph_render_graph(0);
		Target->disable_aniso();
	}

	bool locked = scope_debug == 4;

    if (!locked) {
        // Build light list
        // I doubt there is any point to occq checking non-shadowcasters

        if (Target == TargetMain) {
            auto LP = &Lights.package;
            LP_normal.clear();
            for (auto L : LP->v_shadowed) {
                L->vis_update();
                if (L->vis.visible)
                    LP_normal.v_shadowed.push_back(L);
                else if (scope_debug >= 3) CDebugRenderer().draw_line(Fmatrix(), L->position, Fvector(L->direction).mul(L->range).add(L->position), 0xff999999, false);
            }
            for (auto L : LP->v_point) LP_normal.v_point.push_back(L);
            for (auto L : LP->v_spot) LP_normal.v_spot.push_back(L);

            // stats
            stats.l_shadowed = LP_normal.v_shadowed.size();
            stats.l_unshadowed = LP_normal.v_point.size() + LP_normal.v_spot.size();
            stats.l_total = stats.l_shadowed + stats.l_unshadowed;
        }

        {
            PIX_EVENT(DEFER_TEST_LIGHT_VIS);
            Target->phase_occq();

            auto LP = &Lights.package;
            for (auto L : LP->v_shadowed)
                L->vis_prepare();
        }
    }

	//******* Main render :: PART-1 (second)
	if (split_the_scene_to_minimize_wait)
	{
		PIX_EVENT(DEFER_PART1_SPLIT);
		// skybox can be drawn here

		if (0)
		{
			if (!RImplementation.o.dx10_msaa)
				Target->u_setrt(Target->rt_Generic_0, Target->rt_Generic_1, 0, Target->baseZB);
			else
				Target->u_setrt(Target->rt_Generic_0_r, Target->rt_Generic_1, 0,
					RImplementation.Target->rt_MSAADepth->pZRT);
			RCache.set_CullMode(CULL_NONE);
			RCache.set_Stencil(FALSE);

			// draw skybox
			RCache.set_ColorWriteEnable();
			//CHK_DX(HW.pDevice->SetRenderState			( D3DRS_ZENABLE,	FALSE				));
			RCache.set_Z(FALSE);
			g_pGamePersistent->Environment().RenderSky();
			//CHK_DX(HW.pDevice->SetRenderState			( D3DRS_ZENABLE,	TRUE				));
			RCache.set_Z(TRUE);
		}

		// level
		Target->phase_scene_begin();
		r_dsgraph_render_lods(true, true);
		if (Details) {
			PIX_EVENT(CDETAILMANAGER_RENDER);
			Details->Render();
		}
		if (ps_r2_ls_flags.test(R2FLAG_TERRAIN_PREPASS)) r_dsgraph_render_landscape(1, true);
		Target->phase_scene_end();
	}

	// Wall marks
	if (Wallmarks)
	{
		PIX_EVENT(DEFER_WALLMARKS);
		Target->phase_wallmarks();

		Wallmarks->Render(); // wallmarks has priority as normal geometry
	}

	// full screen pass to mark msaa-edge pixels in highest stencil bit
	if (RImplementation.o.dx10_msaa)
	{
		PIX_EVENT(MARK_MSAA_EDGES);
		Target->mark_msaa_edges();
	}

	//	TODO: DX10: Implement DX10 rain.
	if (ps_r2_ls_flags.test(R3FLAG_DYN_WET_SURF))
	{
		PIX_EVENT(DEFER_RAIN);
		if (!Device.m_SecondViewport.IsSVPFrame())
			shadowmap_rain();
		render_rain();
	}

	// Save previus and current matrices
	{
		static Fmatrix mm_saved_viewproj[2];

		Target->GetPrevious()->Matrix_previous.mul(mm_saved_viewproj[Device.m_SecondViewport.IsSVPFrame()], Device.mInvView);
		Target->GetPrevious()->Matrix_current.set(Device.mProject);
		mm_saved_viewproj[Device.m_SecondViewport.IsSVPFrame()].set(Device.mFullTransform);
	}

		if (RImplementation.o.ssfx_sss)
	{
		static bool sss_rendered, sss_extended_rendered;

		// SSS Shadows
		if (ps_ssfx_sss_quality.z > 0)
		{
			Target->phase_ssfx_sss();
			sss_rendered = true;
		}
		else
		{
			if (sss_rendered) // Clear buffer
			{
				sss_rendered = false;
				FLOAT ColorRGBA[4] = { 1,1,1,1 };
				HW.pContext->ClearRenderTargetView(Target->rt_ssfx_sss->pRT, ColorRGBA);
			}
		}

		if (ps_ssfx_sss_quality.w > 0)
		{
			// Extra lights
			Target->phase_ssfx_sss_ext(Lights.package);
			sss_extended_rendered = true;
		}
		else
		{
			if (sss_extended_rendered) // Clear buffer
			{
				sss_extended_rendered = false;
				FLOAT ColorRGBA[4] = { 1,1,1,1 };
				HW.pContext->ClearRenderTargetView(Target->rt_ssfx_sss_tmp->pRT, ColorRGBA);
			}
		}
	}
}

void CRender::combineLightingAndBloom()
{
	// FIXME: SVP: Hack to force SRVSManager to unbind the first slot.
	//  Required until more robust state invalidation is in place.
	auto unbind_s_base = []() -> void {
		ID3D11ShaderResourceView* crv[1] = {nullptr};
		HW.pContext->PSSetShaderResources(0, 1, crv);
		SRVSManager.SetPSResource(0, nullptr);
	};

	{
		PIX_EVENT(DEFER_SELF_ILLUM);
		Target->phase_accumulator();
		// Render emissive geometry, stencil - write 0x0 at pixel pos
		RCache.set_xform_project(Device.mProject);
		RCache.set_xform_view(Device.mView);
		// Stencil - write 0x1 at pixel pos - 
		if (!RImplementation.o.dx10_msaa)
			RCache.set_Stencil(TRUE, D3DCMP_ALWAYS, 0x01, 0xff, 0xff, D3DSTENCILOP_KEEP, D3DSTENCILOP_REPLACE,
				D3DSTENCILOP_KEEP);
		else
			RCache.set_Stencil(TRUE, D3DCMP_ALWAYS, 0x01, 0xff, 0x7f, D3DSTENCILOP_KEEP, D3DSTENCILOP_REPLACE,
				D3DSTENCILOP_KEEP);
		//RCache.set_Stencil				(TRUE,D3DCMP_ALWAYS,0x00,0xff,0xff,D3DSTENCILOP_KEEP,D3DSTENCILOP_REPLACE,D3DSTENCILOP_KEEP);
		RCache.set_CullMode(CULL_CCW);
		RCache.set_ColorWriteEnable();

		unbind_s_base();

		RImplementation.r_dsgraph_render_emissive(RImplementation.o.ssfx_bloom ? false : true);
	}

	if (RImplementation.o.ssfx_bloom)
	{
		// Render Emissive on `rt_ssfx_bloom_emissive`
		FLOAT ColorRGBA[4] = { 0,0,0,0 };
		HW.pContext->ClearRenderTargetView(Target->rt_ssfx_bloom_emissive->pRT, ColorRGBA);
		Target->u_setrt(Target->rt_ssfx_bloom_emissive, nullptr, NULL, !RImplementation.o.dx10_msaa ? Target->baseZB : Target->rt_MSAADepth->pZRT);

		unbind_s_base();

		RImplementation.r_dsgraph_render_emissive(true, true);
	}
}

// Sun uses the entire depth texture, so we have to do an interleaved pass here
void CRender::renderSun() {
	// Directional light - fucking sun
	if (bSUN)
	{
		RImplementation.stats.l_visible++;
		render_sun_cascades();
	}	
}

void CRender::renderShadowmaps() {
	PIX_EVENT(RENDER_SHADOWMAPS);
	render_lights_shadowmaps(LP_normal);
}

// Combine runs in inverted order (svp -> main)
void CRender::combineGBuffer() {
	PIX_EVENT(COMBINE_GBUFFER);
	Device.dwViewport++;

	// Lighting, non dependant on OCCQ
	{
		PIX_EVENT(DEFERRED_LIGHTS);
		Target->phase_accumulator();
		HOM.Disable();
		render_lights(LP_normal);
	}

	{
		if (RImplementation.o.ssfx_volumetric)
			Target->phase_ssfx_volumetric_blur();
	}

	// Postprocess
	{
		PIX_EVENT(DEFER_LIGHT_COMBINE);
		Target->phase_combine();
	}
}

void CRender::Render()
{
	VERIFY(0 == mapDistort.size() + mapHUDDistort.size());

	rmNormal();

	TargetMain->SetActive();

	bool _menu_pp = g_pGamePersistent ? g_pGamePersistent->OnRenderPPUI_query() : false;
	if (_menu_pp)
	{
		render_menu();
		return;
	};

	IMainMenu* pMainMenu = g_pGamePersistent ? g_pGamePersistent->m_pMainMenu : 0;
	bool bMenu = pMainMenu ? pMainMenu->CanSkipSceneRendering() : false;

	if (!(g_pGameLevel && g_hud)
		|| bMenu)
	{
		Target->u_setrt(Device.dwWidth, Device.dwHeight, HW.pBaseRT, nullptr, NULL, HW.pBaseZB);
		return;
	}

	if (m_bFirstFrameAfterReset)
	{
		m_bFirstFrameAfterReset = false;
		return;
	}

	//.	VERIFY					(g_pGameLevel && g_pGameLevel->pHUD);

	// Configure
	RImplementation.o.distortion = FALSE; // disable distorion
	Fcolor sun_color = ((light*)Lights.sun_adapted._get())->color;
	bSUN = ps_r2_ls_flags.test(R2FLAG_SUN) && (u_diffuse2s(sun_color.r, sun_color.g, sun_color.b) > EPS) && !strstr(Core.Params, "-r4_dev");
	if (o.sunstatic) bSUN = FALSE;
	// Msg						("sstatic: %s, sun: %s",o.sunstatic?;"true":"false", bSUN?"true":"false");

	ViewBase.CreateFromMatrix(Device.mFullTransform, FRUSTUM_P_LRTB + FRUSTUM_P_FAR);
	View = 0;
	if (!ps_r2_ls_flags.test(R2FLAG_EXP_MT_CALC))
	{
		HOM.Enable();
		HOM.Render(ViewBase);
	}

	//*******
	// Sync point
	Device.Statistic->RenderDUMP_Wait_S.Begin();
	if (ps_r2_qsync)
	{
		CTimer T;
		T.Start();
		BOOL result = FALSE;
		HRESULT hr = S_FALSE;
		//while	((hr=q_sync_point[q_sync_count]->GetData	(&result,sizeof(result),D3DGETDATA_FLUSH))==S_FALSE) {
		while ((hr = GetData(q_sync_point[q_sync_count], &result, sizeof(result))) == S_FALSE)
		{
			if (!SwitchToThread()) Sleep(ps_r2_wait_sleep);
			if (T.GetElapsed_ms() > 500)
			{
				result = FALSE;
				break;
			}
		}
	}
	Device.Statistic->RenderDUMP_Wait_S.End();
	q_sync_count = (q_sync_count + 1) % HW.Caps.iGPUNum;
	//CHK_DX										(q_sync_point[q_sync_count]->Issue(D3DISSUE_END));
	CHK_DX(EndQuery(q_sync_point[q_sync_count]));

	// Clear the stored lenses
	RImplementation.mapScopeHUDSorted.clear();
  RImplementation.mapReflexHUDSorted.clear();
	mapEmissive.clear();
	mapHUDEmissive.clear();
  
	Device.m_SecondViewport.eyepiece.radius = 0;
	Device.m_SecondViewport.objective.radius = 0;

	auto mainCameraPos = Device.vCameraPosition;
	TargetMain->SetActive();
	{
		PIX_EVENT(DRAW_MAIN);
		renderGBuffer();
	}

	if (Device.m_SecondViewport.IsSVPActive()) {
		TargetSVP->SetActive();
		{
            PIX_EVENT(DRAW_SVP);
            //SVP HACK: Use main frame view matrix to prevent rendering the wrong sector
            Device.vCameraPosition = mainCameraPos;
            renderGBuffer();
		}
	}

	{   
		PIX_EVENT(RENDER_SUN);
		TargetMain->SetActive();
		renderSun();
	}

	{	
		PIX_EVENT(COMBINE_GBUFFER_CONT);
		TargetMain->SetActive();
		combineLightingAndBloom();
		if (Device.m_SecondViewport.IsSVPActive()) {
			TargetSVP->SetActive();
			combineLightingAndBloom();
		}
	}

	{
		PIX_EVENT(DRAW_SHADOWMAPS);
		TargetMain->SetActive();
		renderShadowmaps();
	}

	if (Device.m_SecondViewport.IsSVPActive()) {
		TargetSVP->SetActive();
		{
			PIX_EVENT(COMBINE_SVP);

			// NVG shader is required, but tube overlay is not
			//   so we increase the size to the max that doesn't break the shader
			auto nvg_tube_radius = ps_dev_param_7.y;
			ps_dev_param_7.y = 0.99;
			combineGBuffer();
			ps_dev_param_7.y = nvg_tube_radius;
		}		
	}

	TargetMain->SetActive();
	{
		PIX_EVENT(COMBINE_MAIN);
		combineGBuffer();
	}

	Target->phase_scope_debug();

	if (Details)
		Details->details_clear();

	VERIFY(0 == mapDistort.size() + mapHUDDistort.size());
}

void CRender::render_forward()
{
	VERIFY(0 == mapDistort.size() + mapHUDDistort.size());
	RImplementation.o.distortion = RImplementation.o.distortion_enabled; // enable distorion

	//******* Main render - second order geometry (the one, that doesn't support deffering)
	//.todo: should be done inside "combine" with estimation of of luminance, tone-mapping, etc.
	{
		// level
		r_pmask(false, true); // enable priority "1"
		phase = PHASE_NORMAL;
		render_main(Device.mFullTransform, false); //
		//	Igor: we don't want to render old lods on next frame.
		mapLOD.clear();
		r_dsgraph_render_graph(1); // normal level, secondary priority
		PortalTraverser.fade_render(); // faded-portals
		r_dsgraph_render_sorted(); // strict-sorted geoms
		//g_pGamePersistent->Environment().RenderLast(); // rain/thunder-bolts
	}

	RImplementation.o.distortion = FALSE; // disable distorion
}

void CRender::RenderToTarget(RRT target)
{
	ref_rt* RT = nullptr;

	switch (target)
	{
	case rtPDA:
		RT = &Target->rt_ui_pda;
		break;
	case rtSVP:
		RT = &Target->rt_secondVP;
		break;
	default:
		Debug.fatal(DEBUG_INFO, "None or wrong Target specified: %i", target);
		break;
	}

	ID3DTexture2D* pBuffer = nullptr;
	HW.m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBuffer);
	HW.pContext->CopyResource((*RT)->pSurface, pBuffer);
	pBuffer->Release();
}
