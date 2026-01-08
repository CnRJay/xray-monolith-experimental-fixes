#include "stdafx.h"
#include "../xrRender/DetailManager.h"

#include "../../xrEngine/igame_persistent.h"
#include "../../xrEngine/environment.h"

#include "../xrRenderDX10/dx10BufferUtils.h"

// Rolling structured buffer capacity
static const u32 GRASS_RING_BUFFER_CAPACITY = 32768;
// srv slot for grass instance data
static const u32 GRASS_SRV_SLOT = 15;

// Vars to store wind prev frame data ( Motion vectors )
static u32 prev_frame = -1;
static float prev_time = 0;
static Fvector4	prev_dir1 = { 0, 0, 0 }, prev_dir2 = { 0, 0, 0 };

const int quant = 16384;
const int c_hdr = 10;
const int c_size = 4;

static D3DVERTEXELEMENT9 dwDecl[] =
{
	{0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0}, // pos
	{0, 12, D3DDECLTYPE_SHORT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0}, // uv
	D3DDECL_END()
};

#pragma pack(push,1)
struct vertHW
{
	float x, y, z;
	short u, v, t, mid;
};
#pragma pack(pop)

short QC(float v);
//{
//	int t=iFloor(v*float(quant)); clamp(t,-32768,32767);
//	return short(t&0xffff);
//}

float GoToValue(float& current, float go_to)
{
	float diff = abs(current - go_to);

	float r_value = Device.fTimeDelta;

	if (diff - r_value <= 0)
	{
		current = go_to;
		return 0;
	}

	return current < go_to ? r_value : -r_value;
}

void CDetailManager::hw_Load_Shaders()
{
	// Create shader to access constant storage
	ref_shader S;
	S.create("details\\set");
	R_constant_table& T0 = *(S->E[0]->passes[0]->constants);
	R_constant_table& T1 = *(S->E[1]->passes[0]->constants);
	hwc_consts = T0.get("consts");
	hwc_wave = T0.get("wave");
	hwc_wind = T0.get("dir2D");
	hwc_array = T0.get("array");
	hwc_s_consts = T1.get("consts");
	hwc_s_xform = T1.get("xform");
	hwc_s_array = T1.get("array");
}

void CDetailManager::hw_Render()
{
	// Render-prepare
	//	Update timer
	//	Can't use Device.fTimeDelta since it is smoothed! Don't know why, but smoothed value looks more choppy!
	float fDelta = Device.fTimeGlobal - m_global_time_old;
	if ((fDelta < 0) || (fDelta > 1)) fDelta = 0.03;
	m_global_time_old = Device.fTimeGlobal;

	m_time_rot_1 += (PI_MUL_2 * fDelta / swing_current.rot1);
	m_time_rot_2 += (PI_MUL_2 * fDelta / swing_current.rot2);
	m_time_pos += fDelta * swing_current.speed;

	//float		tm_rot1		= (PI_MUL_2*Device.fTimeGlobal/swing_current.rot1);
	//float		tm_rot2		= (PI_MUL_2*Device.fTimeGlobal/swing_current.rot2);
	float tm_rot1 = m_time_rot_1;
	float tm_rot2 = m_time_rot_2;

	Fvector4 dir1, dir2;
	dir1.set(_sin(tm_rot1), 0, _cos(tm_rot1), 0).normalize().mul(swing_current.amp1);
	dir2.set(_sin(tm_rot2), 0, _cos(tm_rot2), 0).normalize().mul(swing_current.amp2);

	// Setup geometry and DMA
	RCache.set_Geometry(hw_Geom);

	// Wave0
	float scale = 1.f / float(quant);
	Fvector4 wave, prev_wave;
	Fvector4 consts;
	consts.set(scale, scale, ps_r__Detail_l_aniso, ps_r__Detail_l_ambient);
	//wave.set				(1.f/5.f,		1.f/7.f,	1.f/3.f,	Device.fTimeGlobal*swing_current.speed);
	wave.set(1.f / 5.f, 1.f / 7.f, 1.f / 3.f, m_time_pos);
	prev_wave.set(1.f / 5.f, 1.f / 7.f, 1.f / 3.f, prev_time);
	//RCache.set_c			(&*hwc_consts,	scale,		scale,		ps_r__Detail_l_aniso,	ps_r__Detail_l_ambient);				// consts
	//RCache.set_c			(&*hwc_wave,	wave.div(PI_MUL_2));	// wave
	//RCache.set_c			(&*hwc_wind,	dir1);																					// wind-dir
	//hw_Render_dump			(&*hwc_array,	1, 0, c_hdr );
	hw_Render_dump(consts, wave.div(PI_MUL_2), dir1, prev_wave.div(PI_MUL_2), prev_dir1, 1, 0);

	// Wave1
	//wave.set				(1.f/3.f,		1.f/7.f,	1.f/5.f,	Device.fTimeGlobal*swing_current.speed);
	wave.set(1.f / 3.f, 1.f / 7.f, 1.f / 5.f, m_time_pos);
	prev_wave.set(1.f / 3.f, 1.f / 7.f, 1.f / 5.f, prev_time);
	//RCache.set_c			(&*hwc_wave,	wave.div(PI_MUL_2));	// wave
	//RCache.set_c			(&*hwc_wind,	dir2);																					// wind-dir
	//hw_Render_dump			(&*hwc_array,	2, 0, c_hdr );
	hw_Render_dump(consts, wave.div(PI_MUL_2), dir2, prev_wave.div(PI_MUL_2), prev_dir2, 2, 0);

	// Still
	consts.set(scale, scale, scale, 1.f);
	//RCache.set_c			(&*hwc_s_consts,scale,		scale,		scale,				1.f);
	//RCache.set_c			(&*hwc_s_xform,	Device.mFullTransform);
	//hw_Render_dump			(&*hwc_s_array,	0, 1, c_hdr );
	hw_Render_dump(consts, wave.div(PI_MUL_2), dir2, prev_wave.div(PI_MUL_2), prev_dir2, 0, 1);

	if (prev_frame != Device.dwFrame) 
	{
		prev_frame = Device.dwFrame;
		
		// Prev Frame swing time
		prev_time = m_time_pos;

		// Prev frame dir
		prev_dir1.set(dir1);
		prev_dir2.set(dir2);
	}
}

void CDetailManager::hw_Render_dump(const Fvector4& consts, const Fvector4& wave, const Fvector4& wind, 
									const Fvector4& prev_wave, const Fvector4& prev_wind, u32 var_id, u32 lod_id)
{
	static shared_str strConsts("consts");
	static shared_str strWave("wave");
	static shared_str strDir2D("dir2D");
	static shared_str strArray("array");
	static shared_str strXForm("xform");

	// Vanilla grass/trees wind
	static shared_str strWavePrev("wave_prev");
	static shared_str strDir2DPrev("dir2D_prev");

	// Grass Benders
	static shared_str strPrevPos("benders_prevpos");
	static shared_str strPos("benders_pos");
	static shared_str strGrassSetup("benders_setup");

	static shared_str strExData("exdata");
	static shared_str strGrassAlign("grass_align");

	// Grass benders data
	IGame_Persistent::grass_data& GData = g_pGamePersistent->grass_shader_data;
	Fvector4 player_pos = { 0, 0, 0, 0 };
	int BendersQty = _min(16, ps_ssfx_grass_interactive.y + 1);

	// Add Player?
	if (ps_ssfx_grass_interactive.x > 0)
		player_pos.set(Device.vCameraPosition.x, Device.vCameraPosition.y, Device.vCameraPosition.z, -1);

	Device.Statistic->RenderDUMP_DT_Count = 0;

	// Matrices and offsets
	u32 vOffset = 0;
	u32 iOffset = 0;

	vis_list& list = m_visibles[var_id];

	CEnvDescriptor& desc = *g_pGamePersistent->Environment().CurrentEnv;
	Fvector c_sun, c_ambient, c_hemi;
	c_sun.set(desc.sun_color.x, desc.sun_color.y, desc.sun_color.z);
	c_sun.mul(.5f);
	c_ambient.set(desc.ambient.x, desc.ambient.y, desc.ambient.z);
	c_hemi.set(desc.hemi_color.x, desc.hemi_color.y, desc.hemi_color.z);

	// Iterate
	for (u32 O = 0; O < objects.size(); O++)
	{
		CDetail& Object = *objects[O];
		xr_vector<SlotItemVec*>& vis = list[O];
		if (!vis.empty())
		{
			for (u32 iPass = 0; iPass < Object.shader->E[lod_id]->passes.size(); ++iPass)
			{
				// Setup matrices + colors (and flush it as necessary)
				//RCache.set_Element				(Object.shader->E[lod_id]);
				RCache.set_Element(Object.shader->E[lod_id], iPass);
				RImplementation.apply_lmaterial();

				//	This could be cached in the corresponding consatant buffer
				//	as it is done for DX9
				RCache.set_c(strConsts, consts);
				RCache.set_c(strWave, wave);
				RCache.set_c(strDir2D, wind);
				RCache.set_c(strXForm, Device.mFullTransform);
				RCache.set_c(strGrassAlign, ps_ssfx_terrain_grass_align);

				RCache.set_c(strWavePrev, prev_wave);
				RCache.set_c(strDir2DPrev, prev_wind);

				if (ps_ssfx_grass_interactive.y > 0)
				{
					RCache.set_c(strGrassSetup, ps_ssfx_int_grass_params_1);

					Fvector4* c_grass;
					{
						void* GrassData;
						RCache.get_ConstantDirect(strPos, BendersQty * sizeof(Fvector4) * 2, &GrassData, 0, 0);
						c_grass = (Fvector4*)GrassData;
					}
					VERIFY(c_grass);

					if (c_grass)
					{
						c_grass[0].set(player_pos);
						c_grass[16].set(0.0f, -99.0f, 0.0f, 1.0f);

						for (int Bend = 1; Bend < BendersQty; Bend++)
						{
							c_grass[Bend].set(GData.pos[Bend].x, GData.pos[Bend].y, GData.pos[Bend].z, GData.radius_curr[Bend]);
							c_grass[Bend + 16].set(GData.dir[Bend].x, GData.dir[Bend].y, GData.dir[Bend].z, GData.str[Bend]);
						}
					}

					Fvector4* c_prev_grass;
					{
						void* prev_GrassData;
						RCache.get_ConstantDirect(strPrevPos, BendersQty * sizeof(Fvector4) * 2, &prev_GrassData, 0, 0);
						c_prev_grass = (Fvector4*)prev_GrassData;
					}
					VERIFY(c_prev_grass);

					if (c_prev_grass)
					{
						for (int Bend = 0; Bend < BendersQty; Bend++)
						{
							c_prev_grass[Bend].set(GData.prev_pos[Device.m_SecondViewport.IsSVPFrame()][Bend]);
							c_prev_grass[Bend + 16].set(GData.prev_dir[Device.m_SecondViewport.IsSVPFrame()][Bend]);
						}
					}
				}

				Fvector4* c_ExData = 0;
				{
					void* pExtraData;
					RCache.get_ConstantDirect(strExData, hw_BatchSize * sizeof(Fvector4), &pExtraData, 0, 0);
					c_ExData = (Fvector4*)pExtraData;
				}
				VERIFY(c_ExData);

				//ref_constant constArray = RCache.get_c(strArray);
				//VERIFY(constArray);

				//u32			c_base				= x_array->vs.index;
				//Fvector4*	c_storage			= RCache.get_ConstantCache_Vertex().get_array_f().access(c_base);
				Fvector4* c_storage = 0;
				//	Map constants to memory directly
				{
					void* pVData;
					RCache.get_ConstantDirect(strArray,
					                          hw_BatchSize * sizeof(Fvector4) * 4,
					                          &pVData, 0, 0);
					c_storage = (Fvector4*)pVData;
				}
				VERIFY(c_storage);

				u32 dwBatch = 0;

				xr_vector<SlotItemVec*>::iterator _vI = vis.begin();
				xr_vector<SlotItemVec*>::iterator _vE = vis.end();
				for (; _vI != _vE; _vI++)
				{
					SlotItemVec* items = *_vI;
					SlotItemVecIt _iI = items->begin();
					SlotItemVecIt _iE = items->end();
					for (; _iI != _iE; _iI++)
					{
						SlotItem& Instance = **_iI;
						u32 base = dwBatch * 4;

						Instance.alpha += GoToValue(Instance.alpha, Instance.alpha_target);

						float scale = Instance.scale_calculated;

						// Sort of fade using the scale
						// fade_distance == -1 use light_position to define "fade", anything else uses fade_distance
						if (fade_distance <= -1)
							scale *= 1.0f - Instance.position.distance_to_xz_sqr(light_position) * 0.005f;
						else if (Instance.distance > fade_distance)
							scale *= 1.0f - abs(Instance.distance - fade_distance) * 0.005f;

						if (scale <= 0 || Instance.alpha <= 0)
							break;

						// Build matrix ( 3x4 matrix, last row - color )
						//float scale = Instance.scale_calculated;
						Fmatrix& M = Instance.mRotY;
						c_storage[base + 0].set(M._11 * scale, M._21 * scale, M._31 * scale, M._41);
						c_storage[base + 1].set(M._12 * scale, M._22 * scale, M._32 * scale, M._42);
						c_storage[base + 2].set(M._13 * scale, M._23 * scale, M._33 * scale, M._43);
						//RCache.set_ca(&*constArray, base+0, M._11*scale,	M._21*scale,	M._31*scale,	M._41	);
						//RCache.set_ca(&*constArray, base+1, M._12*scale,	M._22*scale,	M._32*scale,	M._42	);
						//RCache.set_ca(&*constArray, base+2, M._13*scale,	M._23*scale,	M._33*scale,	M._43	);

						// Build color
						// R2 only needs hemisphere
						float h = Instance.c_hemi;
						float s = Instance.c_sun;
						c_storage[base + 3].set(s, s, s, h);

						if (c_ExData)
							c_ExData[dwBatch].set(Instance.normal.x, Instance.normal.y, Instance.normal.z, Instance.alpha);

						//RCache.set_ca(&*constArray, base+3, s,				s,				s,				h		);
						dwBatch ++;
						if (dwBatch == hw_BatchSize)
						{
							// flush
							Device.Statistic->RenderDUMP_DT_Count += dwBatch;
							u32 dwCNT_verts = dwBatch * Object.number_vertices;
							u32 dwCNT_prims = (dwBatch * Object.number_indices) / 3;
							//RCache.get_ConstantCache_Vertex().b_dirty				=	TRUE;
							//RCache.get_ConstantCache_Vertex().get_array_f().dirty	(c_base,c_base+dwBatch*4);
							RCache.Render(D3DPT_TRIANGLELIST, vOffset, 0, dwCNT_verts, iOffset, dwCNT_prims);
							RCache.stat.r.s_details.add(dwCNT_verts);

							// restart
							dwBatch = 0;

							//	Remap constants to memory directly (just in case anything goes wrong)
							{
								void* pVData;
								RCache.get_ConstantDirect(strArray,
								                          hw_BatchSize * sizeof(Fvector4) * 4,
								                          &pVData, 0, 0);
								c_storage = (Fvector4*)pVData;
							}
							VERIFY(c_storage);
						}
					}
				}
				// flush if nessecary
				if (dwBatch)
				{
					Device.Statistic->RenderDUMP_DT_Count += dwBatch;
					u32 dwCNT_verts = dwBatch * Object.number_vertices;
					u32 dwCNT_prims = (dwBatch * Object.number_indices) / 3;
					//RCache.get_ConstantCache_Vertex().b_dirty				=	TRUE;
					//RCache.get_ConstantCache_Vertex().get_array_f().dirty	(c_base,c_base+dwBatch*4);
					RCache.Render(D3DPT_TRIANGLELIST, vOffset, 0, dwCNT_verts, iOffset, dwCNT_prims);
					RCache.stat.r.s_details.add(dwCNT_verts);
				}
			}
		}
		vOffset += hw_BatchSize * Object.number_vertices;
		iOffset += hw_BatchSize * Object.number_indices;
	}
}
// the good shit -CnR
void CDetailManager::hw_Load_RingBuffer()
{
	m_RingBufferCapacity = GRASS_RING_BUFFER_CAPACITY;
	m_CurrentOffset = 0;
	m_LastFrameReset = 0;
	m_GrassRingBuffer = nullptr;
	m_GrassSRV = nullptr;

#if defined(USE_DX11)
	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = m_RingBufferCapacity * sizeof(GrassInstanceData);
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	desc.StructureByteStride = sizeof(GrassInstanceData);

	R_CHK(HW.pDevice->CreateBuffer(&desc, nullptr, &m_GrassRingBuffer));

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = m_RingBufferCapacity;

	R_CHK(HW.pDevice->CreateShaderResourceView(m_GrassRingBuffer, &srvDesc, &m_GrassSRV));
#else
	D3D10_BUFFER_DESC desc = {};
	desc.ByteWidth = m_RingBufferCapacity * sizeof(GrassInstanceData);
	desc.Usage = D3D10_USAGE_DYNAMIC;
	desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
	desc.MiscFlags = 0;

	R_CHK(HW.pDevice->CreateBuffer(&desc, nullptr, &m_GrassRingBuffer));

	D3D10_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	srvDesc.ViewDimension = D3D10_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.ElementOffset = 0;
	srvDesc.Buffer.ElementWidth = m_RingBufferCapacity * 5; // 5 float4 per instance

	R_CHK(HW.pDevice->CreateShaderResourceView(m_GrassRingBuffer, &srvDesc, &m_GrassSRV));
#endif

	Msg("* [DETAILS] Ring buffer created: %d instances, %dKB VRAM", 
		m_RingBufferCapacity, (m_RingBufferCapacity * sizeof(GrassInstanceData)) / 1024);

	hw_Load_InstancedGeom();
}

// vertex format for instanced rendering
#pragma pack(push,1)
struct vertHW_Instanced
{
	float x, y, z;
	short u, v, t, pad;  // t = normalized height and pad for alignment
};
#pragma pack(pop)

void CDetailManager::hw_Load_InstancedGeom()
{
	static D3DVERTEXELEMENT9 instDecl[] =
	{
		{0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
		{0, 12, D3DDECLTYPE_SHORT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
		D3DDECL_END()
	};

	m_InstancedGeom.resize(objects.size());

	for (u32 o = 0; o < objects.size(); o++)
	{
		const CDetail& D = *objects[o];
		InstancedGeom& geom = m_InstancedGeom[o];

		geom.vertCount = D.number_vertices;
		geom.idxCount = D.number_indices;

		// create vertex buffer for single model
		u32 vbSize = D.number_vertices * sizeof(vertHW_Instanced);
		vertHW_Instanced* pVData = xr_alloc<vertHW_Instanced>(D.number_vertices);

		for (u32 v = 0; v < D.number_vertices; v++)
		{
			const Fvector& vP = D.vertices[v].P;
			pVData[v].x = vP.x;
			pVData[v].y = vP.y;
			pVData[v].z = vP.z;
			pVData[v].u = QC(D.vertices[v].u);
			pVData[v].v = QC(D.vertices[v].v);
			pVData[v].t = QC(vP.y / (D.bv_bb.max.y - D.bv_bb.min.y));
			pVData[v].pad = 0;
		}

		R_CHK(dx10BufferUtils::CreateVertexBuffer(&geom.VB, pVData, vbSize));
		xr_free(pVData);

		// Create index buffer for single model
		u32 ibSize = D.number_indices * sizeof(u16);
		R_CHK(dx10BufferUtils::CreateIndexBuffer(&geom.IB, D.indices, ibSize));
	}

	// Create declaration which is the same format as batched but without the matrix index
	m_InstancedDecl.create(instDecl, hw_VB, hw_IB);

	Msg("* [DETAILS] Instanced geometry created for %d models", objects.size());
}

void CDetailManager::hw_Unload_RingBuffer()
{
	// Unload the instanced geometry
	for (auto& geom : m_InstancedGeom)
	{
		_RELEASE(geom.IB);
		_RELEASE(geom.VB);
	}
	m_InstancedGeom.clear();
	m_InstancedDecl.destroy();

	_RELEASE(m_GrassSRV);
	_RELEASE(m_GrassRingBuffer);
	m_RingBufferCapacity = 0;
	m_CurrentOffset = 0;
}

void CDetailManager::hw_Render_dump_instanced(const Fvector4& consts, const Fvector4& wave, const Fvector4& wind,
    const Fvector4& prev_wave, const Fvector4& prev_wind, u32 var_id, u32 lod_id)
{
	static shared_str strConsts("consts");
	static shared_str strWave("wave");
	static shared_str strDir2D("dir2D");
	static shared_str strXForm("xform");
	static shared_str strBufferOffset("c_buffer_offset");

	// grass/trees wind
	static shared_str strWavePrev("wave_prev");
	static shared_str strDir2DPrev("dir2D_prev");

	// Grass Benders
	static shared_str strPrevPos("benders_prevpos");
	static shared_str strPos("benders_pos");
	static shared_str strGrassSetup("benders_setup");
	static shared_str strGrassAlign("grass_align");

	if (m_LastFrameReset != Device.dwFrame)
	{
		m_LastFrameReset = Device.dwFrame;
		m_CurrentOffset = 0;
	}

	// Grass benders data
	IGame_Persistent::grass_data& GData = g_pGamePersistent->grass_shader_data;
	Fvector4 player_pos = { 0, 0, 0, 0 };
	int BendersQty = _min(16, ps_ssfx_grass_interactive.y + 1);

	if (ps_ssfx_grass_interactive.x > 0)
		player_pos.set(Device.vCameraPosition.x, Device.vCameraPosition.y, Device.vCameraPosition.z, -1);

	Device.Statistic->RenderDUMP_DT_Count = 0;

	vis_list& list = m_visibles[var_id];

	// Bind the ring buffer srv once for all grass objects
#if defined(USE_DX11)
	ID3D11ShaderResourceView* srvs[] = { m_GrassSRV };
	HW.pContext->VSSetShaderResources(GRASS_SRV_SLOT, 1, srvs);
#else
	ID3D10ShaderResourceView* srvs[] = { m_GrassSRV };
	HW.pDevice->VSSetShaderResources(GRASS_SRV_SLOT, 1, srvs);
#endif

	// Iterate through detail objects
	for (u32 O = 0; O < objects.size(); O++)
	{
		CDetail& Object = *objects[O];
		xr_vector<SlotItemVec*>& vis = list[O];
		if (vis.empty())
			continue;

		u32 total_instances = 0;
		for (auto* items : vis)
		{
			if (items)
				total_instances += (u32)items->size();
		}

		if (total_instances == 0)
			continue;

		bool bReset = (m_CurrentOffset + total_instances) > m_RingBufferCapacity;
#if defined(USE_DX11)
		D3D11_MAP mapType = bReset ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE;
#else
		D3D10_MAP mapType = bReset ? D3D10_MAP_WRITE_DISCARD : D3D10_MAP_WRITE_NO_OVERWRITE;
#endif

		if (bReset)
			m_CurrentOffset = 0;

		// Map and fill buffer
#if defined(USE_DX11)
		D3D11_MAPPED_SUBRESOURCE mapped;
		HRESULT hr = HW.pContext->Map(m_GrassRingBuffer, 0, mapType, 0, &mapped);
		if (FAILED(hr))
		{
			Msg("! [DETAILS] Failed to map ring buffer");
			continue;
		}
		GrassInstanceData* gpu_ptr = (GrassInstanceData*)mapped.pData + m_CurrentOffset;
#else
		void* pData = nullptr;
		HRESULT hr = m_GrassRingBuffer->Map(mapType, 0, &pData);
		if (FAILED(hr))
		{
			Msg("! [DETAILS] Failed to map ring buffer");
			continue;
		}
		GrassInstanceData* gpu_ptr = (GrassInstanceData*)pData + m_CurrentOffset;
#endif

		u32 idx = 0;

		for (auto* items : vis)
		{
			if (!items)
				continue;

			for (SlotItem* pInstance : *items)
			{
				if (!pInstance)
					continue;

				SlotItem& Instance = *pInstance;

				Instance.alpha += GoToValue(Instance.alpha, Instance.alpha_target);

				float scale = Instance.scale_calculated;

				// Fade logic
				if (fade_distance <= -1)
					scale *= 1.0f - Instance.position.distance_to_xz_sqr(light_position) * 0.005f;
				else if (Instance.distance > fade_distance)
					scale *= 1.0f - abs(Instance.distance - fade_distance) * 0.005f;

				if (scale <= 0 || Instance.alpha <= 0)
					continue;

				// Build matrix rows (3x4 matrix)
				Fmatrix& M = Instance.mRotY;
				gpu_ptr[idx].mat0.set(M._11 * scale, M._21 * scale, M._31 * scale, M._41);
				gpu_ptr[idx].mat1.set(M._12 * scale, M._22 * scale, M._32 * scale, M._42);
				gpu_ptr[idx].mat2.set(M._13 * scale, M._23 * scale, M._33 * scale, M._43);

				// sun, sun, sun, hemi
				float h = Instance.c_hemi;
				float s = Instance.c_sun;
				gpu_ptr[idx].color.set(s, s, s, h);

				// normal.xyz, alpha
				gpu_ptr[idx].params.set(Instance.normal.x, Instance.normal.y, Instance.normal.z, Instance.alpha);

				idx++;
			}
		}

#if defined(USE_DX11)
		HW.pContext->Unmap(m_GrassRingBuffer, 0);
#else
		m_GrassRingBuffer->Unmap();
#endif
		u32 actual_instances = idx;
		if (actual_instances == 0)
			continue;

		// Render passes
		for (u32 iPass = 0; iPass < Object.shader->E[lod_id]->passes.size(); ++iPass)
		{
			RCache.set_Element(Object.shader->E[lod_id], iPass);
			RImplementation.apply_lmaterial();

			RCache.set_c(strConsts, consts);
			RCache.set_c(strWave, wave);
			RCache.set_c(strDir2D, wind);
			RCache.set_c(strXForm, Device.mFullTransform);
			RCache.set_c(strGrassAlign, ps_ssfx_terrain_grass_align);
			RCache.set_c(strWavePrev, prev_wave);
			RCache.set_c(strDir2DPrev, prev_wind);

			RCache.set_c(strBufferOffset, (float)m_CurrentOffset, 0.f, 0.f, 0.f);

			// Grass benders
			if (ps_ssfx_grass_interactive.y > 0)
			{
				RCache.set_c(strGrassSetup, ps_ssfx_int_grass_params_1);

				Fvector4* c_grass;
				{
					void* GrassData;
					RCache.get_ConstantDirect(strPos, BendersQty * sizeof(Fvector4) * 2, &GrassData, 0, 0);
					c_grass = (Fvector4*)GrassData;
				}

				if (c_grass)
				{
					c_grass[0].set(player_pos);
					c_grass[16].set(0.0f, -99.0f, 0.0f, 1.0f);

					for (int Bend = 1; Bend < BendersQty; Bend++)
					{
						c_grass[Bend].set(GData.pos[Bend].x, GData.pos[Bend].y, GData.pos[Bend].z, GData.radius_curr[Bend]);
						c_grass[Bend + 16].set(GData.dir[Bend].x, GData.dir[Bend].y, GData.dir[Bend].z, GData.str[Bend]);
					}
				}

				Fvector4* c_prev_grass;
				{
					void* prev_GrassData;
					RCache.get_ConstantDirect(strPrevPos, BendersQty * sizeof(Fvector4) * 2, &prev_GrassData, 0, 0);
					c_prev_grass = (Fvector4*)prev_GrassData;
				}

				if (c_prev_grass)
				{
					for (int Bend = 0; Bend < BendersQty; Bend++)
					{
						c_prev_grass[Bend].set(GData.prev_pos[Device.m_SecondViewport.IsSVPFrame()][Bend]);
						c_prev_grass[Bend + 16].set(GData.prev_dir[Device.m_SecondViewport.IsSVPFrame()][Bend]);
					}
				}
			}

			const InstancedGeom& geom = m_InstancedGeom[O];
			u32 stride = sizeof(vertHW_Instanced);
			u32 vb_offset = 0;
#if defined(USE_DX11)
			HW.pContext->IASetVertexBuffers(0, 1, &geom.VB, &stride, &vb_offset);
			HW.pContext->IASetIndexBuffer(geom.IB, DXGI_FORMAT_R16_UINT, 0);
			HW.pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			// Hardware instanced draw
			HW.pContext->DrawIndexedInstanced(
				geom.idxCount,           // Index count per instance
				actual_instances,        // Instance count
				0,                       // Start index
				0,                       // Base vertex 
				0                        // Start instance
			);
#else
			HW.pDevice->IASetVertexBuffers(0, 1, &geom.VB, &stride, &vb_offset);
			HW.pDevice->IASetIndexBuffer(geom.IB, DXGI_FORMAT_R16_UINT, 0);
			HW.pDevice->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			// Hardware instanced draw
			HW.pDevice->DrawIndexedInstanced(
				geom.idxCount,           // Index count per instance
				actual_instances,        // Instance count
				0,                       // Start index
				0,                       // Base vertex
				0                        // Start instance
			);
#endif

			RCache.stat.calls++;
			Device.Statistic->RenderDUMP_DT_Count += actual_instances;
			RCache.stat.r.s_details.add(actual_instances * geom.vertCount);
		}

		m_CurrentOffset += actual_instances;
	}

	// Unbind srv
#if defined(USE_DX11)
	ID3D11ShaderResourceView* nullSRV[] = { nullptr };
	HW.pContext->VSSetShaderResources(GRASS_SRV_SLOT, 1, nullSRV);
#else
	ID3D10ShaderResourceView* nullSRV[] = { nullptr };
	HW.pDevice->VSSetShaderResources(GRASS_SRV_SLOT, 1, nullSRV);
#endif
}
