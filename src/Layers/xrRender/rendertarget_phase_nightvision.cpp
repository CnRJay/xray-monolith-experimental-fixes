#include "stdafx.h"
#include "../../xrGame/debug_renderer.h"
#include "FBasicVisual.h"
#include "xrRender_console.h"

#if USE_DX11
#include "../../gamedata/shaders/r3/scope_defines.h"
#endif

void CRenderTarget::phase_nightvision()
{
	//Constants
	u32 Offset = 0;
	u32 C = color_rgba(0, 0, 0, 255);

	float d_Z = EPS_S;
	float d_W = 1.0f;
	float w = float(Device.dwWidth);
	float h = float(Device.dwHeight);

	Fvector2 p0, p1;
#if defined(USE_DX10) || defined(USE_DX11)	
	p0.set(0.0f, 0.0f);
	p1.set(1.0f, 1.0f);
#else
	p0.set(0.5f / w, 0.5f / h);
	p1.set((w + 0.5f) / w, (h + 0.5f) / h);
#endif
	
	//////////////////////////////////////////////////////////////////////////
	//Set MSAA/NonMSAA rendertarget
#if defined(USE_DX10) || defined(USE_DX11)
	ref_rt& dest_rt = RImplementation.o.dx10_msaa ? rt_Generic : rt_Color;
	u_setrt(dest_rt, nullptr, nullptr, nullptr);
#else
	u_setrt(rt_Generic_0, nullptr, nullptr, nullptr);
#endif		

	RCache.set_CullMode(CULL_NONE);
	RCache.set_Stencil(FALSE);

	//Fill vertex buffer
	FVF::TL* pv = (FVF::TL*)RCache.Vertex.Lock(4, g_combine->vb_stride, Offset);
	pv->set(0, float(h), d_Z, d_W, C, p0.x, p1.y); pv++;
	pv->set(0, 0, d_Z, d_W, C, p0.x, p0.y); pv++;
	pv->set(float(w), float(h), d_Z, d_W, C, p1.x, p1.y); pv++;
	pv->set(float(w), 0, d_Z, d_W, C, p1.x, p0.y); pv++;
	RCache.Vertex.Unlock(4, g_combine->vb_stride);

	//Set pass
	RCache.set_Element(s_nightvision->E[ps_r2_nightvision]);

	//Set geometry
	RCache.set_Geometry(g_combine);
	RCache.Render(D3DPT_TRIANGLELIST, Offset, 0, 4, 0, 2);
	
#if defined(USE_DX10) || defined(USE_DX11)
	HW.pContext->CopyResource(rt_Generic_0->pTexture->surface_get(), dest_rt->pTexture->surface_get());
#endif
};


//crookr
void CRenderTarget::phase_fakescope()
{
	//Constants
	u32 Offset = 0;
	u32 C = color_rgba(0, 0, 0, 255);

	float d_Z = EPS_S;
	float d_W = 1.0f;
	float w = float(Device.dwWidth);
	float h = float(Device.dwHeight);

	Fvector2 p0, p1;
#if defined(USE_DX10) || defined(USE_DX11)	
	p0.set(0.0f, 0.0f);
	p1.set(1.0f, 1.0f);
#else
	p0.set(0.5f / w, 0.5f / h);
	p1.set((w + 0.5f) / w, (h + 0.5f) / h);
#endif

	//////////////////////////////////////////////////////////////////////////
	//Set MSAA/NonMSAA rendertarget
#if defined(USE_DX10) || defined(USE_DX11)
	ref_rt& dest_rt = RImplementation.o.dx10_msaa ? rt_Generic : rt_Color;
	u_setrt(dest_rt, nullptr, nullptr, nullptr);

	RCache.set_CullMode(CULL_NONE);
	RCache.set_Stencil(FALSE);

	//Fill vertex buffer
	FVF::TL* pv = (FVF::TL*)RCache.Vertex.Lock(4, g_combine->vb_stride, Offset);
	pv->set(0, float(h), d_Z, d_W, C, p0.x, p1.y); pv++;
	pv->set(0, 0, d_Z, d_W, C, p0.x, p0.y); pv++;
	pv->set(float(w), float(h), d_Z, d_W, C, p1.x, p1.y); pv++;
	pv->set(float(w), 0, d_Z, d_W, C, p1.x, p0.y); pv++;
	RCache.Vertex.Unlock(4, g_combine->vb_stride);

	//Set geometry
	RCache.set_Geometry(g_combine);

	//Set pass
	RCache.set_Element(s_fakescope->E[ps_r2_nightvision]);
	RCache.Render(D3DPT_TRIANGLELIST, Offset, 0, 4, 0, 2);

	HW.pContext->CopyResource(rt_Generic_0->pTexture->surface_get(), dest_rt->pTexture->surface_get());
#else
	//Main pass (we avoid write-read from the same buffer)
	u_setrt(rt_Generic_PingPong, nullptr, nullptr, nullptr);

	RCache.set_CullMode(CULL_NONE);
	RCache.set_Stencil(FALSE);

	//Fill vertex buffer
	FVF::TL* pv = (FVF::TL*)RCache.Vertex.Lock(4, g_combine->vb_stride, Offset);
	pv->set(0, float(h), d_Z, d_W, C, p0.x, p1.y); pv++;
	pv->set(0, 0, d_Z, d_W, C, p0.x, p0.y); pv++;
	pv->set(float(w), float(h), d_Z, d_W, C, p1.x, p1.y); pv++;
	pv->set(float(w), 0, d_Z, d_W, C, p1.x, p0.y); pv++;
	RCache.Vertex.Unlock(4, g_combine->vb_stride);

	//Set pass
	RCache.set_Element(s_fakescope->E[0]);

	//Set geometry
	RCache.set_Geometry(g_combine);
	RCache.Render(D3DPT_TRIANGLELIST, Offset, 0, 4, 0, 2);

	//Draw to rt_Generic_0
	u_setrt(rt_Generic_0, nullptr, nullptr, nullptr);

	RCache.set_CullMode(CULL_NONE);
	RCache.set_Stencil(FALSE);

	//Fill vertex buffer
	pv = (FVF::TL*)RCache.Vertex.Lock(4, g_combine->vb_stride, Offset);
	pv->set(0, float(h), d_Z, d_W, C, p0.x, p1.y); pv++;
	pv->set(0, 0, d_Z, d_W, C, p0.x, p0.y); pv++;
	pv->set(float(w), float(h), d_Z, d_W, C, p1.x, p1.y); pv++;
	pv->set(float(w), 0, d_Z, d_W, C, p1.x, p0.y); pv++;
	RCache.Vertex.Unlock(4, g_combine->vb_stride);

	//Set pass
	RCache.set_Element(s_fakescope->E[1]);

	//Set geometry
	RCache.set_Geometry(g_combine);
	RCache.Render(D3DPT_TRIANGLELIST, Offset, 0, 4, 0, 2);
#endif
};

//--DSR-- HeatVision_start
void CRenderTarget::phase_heatvision()
{
	//Constants
	u32 Offset = 0;
	u32 C = color_rgba(0, 0, 0, 255);

	float d_Z = EPS_S;
	float d_W = 1.0f;
	float w = float(Device.dwWidth);
	float h = float(Device.dwHeight);

	Fvector2 p0, p1;
#if defined(USE_DX10) || defined(USE_DX11)	
	p0.set(0.0f, 0.0f);
	p1.set(1.0f, 1.0f);
#else
	p0.set(0.5f / w, 0.5f / h);
	p1.set((w + 0.5f) / w, (h + 0.5f) / h);
#endif

	//////////////////////////////////////////////////////////////////////////
	//Set MSAA/NonMSAA rendertarget
#if defined(USE_DX10) || defined(USE_DX11)
	ref_rt& dest_rt = RImplementation.o.dx10_msaa ? rt_Generic : rt_Color;
	u_setrt(dest_rt, nullptr, nullptr, nullptr);
#else
	u_setrt(rt_Generic_0, nullptr, nullptr, nullptr);
#endif		

	RCache.set_CullMode(CULL_NONE);
	RCache.set_Stencil(FALSE);

	//Fill vertex buffer
	FVF::TL* pv = (FVF::TL*)RCache.Vertex.Lock(4, g_combine->vb_stride, Offset);
	pv->set(0, float(h), d_Z, d_W, C, p0.x, p1.y); pv++;
	pv->set(0, 0, d_Z, d_W, C, p0.x, p0.y); pv++;
	pv->set(float(w), float(h), d_Z, d_W, C, p1.x, p1.y); pv++;
	pv->set(float(w), 0, d_Z, d_W, C, p1.x, p0.y); pv++;
	RCache.Vertex.Unlock(4, g_combine->vb_stride);

	//Set pass
	RCache.set_Element(s_heatvision->E[ps_r2_heatvision]);

	//Set geometry
	RCache.set_Geometry(g_combine);
	RCache.Render(D3DPT_TRIANGLELIST, Offset, 0, 4, 0, 2);

#if defined(USE_DX10) || defined(USE_DX11)
	HW.pContext->CopyResource(rt_Generic_0->pTexture->surface_get(), dest_rt->pTexture->surface_get());
#endif
};
//--DSR-- HeatVision_start

#if defined(USE_DX11)	
void ffp_sfp(bool drawDebug) {
	auto e = Device.m_SecondViewport.eyepiece;
	auto o = Device.m_SecondViewport.objective;

	Fvector p_e = {0,0,0}; e.m_W.transform(p_e);
	Fvector p_o = {0,0,0}; o.m_W.transform(p_o);

	if (o.radius < EPS)
	{	// We have to have an objective lens or ffp/sfp wont work, so make one up
		float cm = 0.01;
		float distance = (10*cm);  // Something scopelike
		o.radius = e.radius;
		p_o = {0,0, distance};
		e.m_W.transform(p_o);
	}

	{	// Not all scopes have eyepiece and objective inline, and we sure aren't going to simulate prisms
		// So we will reproject the objective lens directly in front of the eyepiece lens and pretend
		
		float distance = p_o.distance_to(p_e) ;
			
		Fvector dir = {0,0,1};

		o.m_W.transform_dir(dir);

		p_o.set(dir.mul(distance).add(p_e));
	}

	Fvector p_d =  Fvector(p_o).sub(p_e);

	Fvector p_c1 = Fvector(p_d).mul(0.4).add(p_e);
	Fvector p_c2 = Fvector(p_d).mul(0.6).add(p_e);

	if (drawDebug) {
		Fvector u_e = {0,e.radius,0}; e.m_W.transform_dir(u_e);
		Fvector u_o = {0,o.radius,0}; e.m_W.transform_dir(u_o);
		Fvector u_c = {0,(o.radius + e.radius)*0.3,0}; e.m_W.transform_dir(u_c);


		auto d = CDebugRenderer();
		d.draw_line(Fmatrix(), Fvector(p_e).add(u_e), Fvector(p_c1).sub(u_c), 0xffffffff, true);
		d.draw_line(Fmatrix(), Fvector(p_e).sub(u_e), Fvector(p_c1).add(u_c), 0xffffffff, true);

		d.draw_line(Fmatrix(), Fvector(p_c1).add(u_c), Fvector(p_c2).add(u_c), 0xffffffff, true);
		d.draw_line(Fmatrix(), Fvector(p_c1).sub(u_c), Fvector(p_c2).sub(u_c), 0xffffffff, true);

		d.draw_line(Fmatrix(), Fvector(p_c2).add(u_c), Fvector(p_o).sub(u_o), 0xffffffff, true);
		d.draw_line(Fmatrix(), Fvector(p_c2).sub(u_c), Fvector(p_o).add(u_o), 0xffffffff, true);
	}
	
	Device.m_SecondViewport.w_ffp = Fvector(p_c1).add(p_e).mul(0.5);
	Device.m_SecondViewport.w_sfp = Fvector(p_c2).add(p_o).mul(0.5);
}

void CRenderTarget::draw_reflex() {
	PIX_EVENT(RENDER_REFLEX_SIGHTS);

	
	Fmatrix FTold = Device.mFullTransform;

	Device.mFullTransform = Device.mFullTransformHud;
	RCache.set_xform_project(Device.mProjectHud);

	// Rendering
	RImplementation.rmNear();
	for (auto N : RImplementation.mapReflexHUDSorted) {
		
		RCache.set_Element(N.val.se);
		
		RCache.set_xform_world(N.val.Matrix);
		RImplementation.apply_object(N.val.pObject);
		RImplementation.apply_lmaterial();
		
		N.val.pVisual->Render(0);
	}
	
	RImplementation.rmNormal();

	// Restore projection
	Device.mFullTransform = FTold;
	RCache.set_xform_project(Device.mProject);
}

void CRenderTarget::draw_scope(ref_shader se, std::function<void(R_dsgraph::mapSorted_Node *N)> bind)
{
	auto elem = se ? se->E[0] : nullptr;
	if (!elem)
		return;
	Fmatrix FTold = Device.mFullTransform;

	Device.mFullTransform = Device.mFullTransformHud;
	RCache.set_xform_project(Device.mProjectHud);

	// Rendering
	RImplementation.rmNear();

	for (auto N : RImplementation.mapScopeHUDSorted) {
		dxRender_Visual* V = N.val.pVisual;

		auto tex = V->GetTexture();
		if (tex)
			t_reticle->surface_set(tex->surface_get());
		RCache.set_Element(elem);

		RCache.set_xform_world(N.val.Matrix);
		RImplementation.apply_object(N.val.pObject);
		RImplementation.apply_lmaterial();

		auto set_v3 = [](LPCSTR id, Fvector3 v) -> void {
			RCache.set_c(id, v.x, v.y, v.z, 1.0);
		};

		RCache.set_c("scope_svp", Device.m_SecondViewport.IsSVPActive());
		RCache.set_c("scope_debug", (int)scope_debug);
		set_v3("scope_w_ffp", Device.m_SecondViewport.w_ffp);
		set_v3("scope_w_sfp", Device.m_SecondViewport.w_sfp);
		Fvector pt = {0,0,0};
		Device.m_SecondViewport.eyepiece.m_W.transform(pt);
		set_v3("scope_w_eyepiece", pt);

		bind(&N);
		V->Render(0);

		auto p = &Device.m_SecondViewport;
		// Clear so that we don't have invalid data for no lense being found

		static u32 dwFrame = 0;
		if (Device.dwFrame > dwFrame)
		{   // Compute the lens information
			dwFrame = Device.dwFrame;

			p->eyepiece.radius = 0.f;
			p->objective.radius = 0.f;

			auto S = N.val.pVisual->getVisData().sphere;
			auto m_W = RCache.get_xform_world();
			m_W.mulB_43(Fmatrix().translate(S.P));

			p->eyepiece.m_W = m_W;
			p->eyepiece.radius = S.R;

			if (p->eyepiece.radius > EPS) {
				// Many guns have had their mesh directly scaled, so the only reliable unit of
				//    measurement is based off the only reliable mesh in the file. The lens.
				Fvector4 o = Fvector4(scope_objective_lens_offset).mul(p->eyepiece.radius);

				// FIXME: I think we need to use the coordinate system of the scope, with the look vector of the gun
				p->objective.m_W.mul(p->eyepiece.m_W, Fmatrix().translate({ o.x, o.y, o.z }));
				p->objective.radius = o.w;
			
			
				ffp_sfp(scope_debug >= 3);
			}
		}
	}

	RImplementation.rmNormal();

	// Restore projection
	Device.mFullTransform = FTold;
	RCache.set_xform_project(Device.mProject);

	RImplementation.o.distortion = FALSE;
}

//  Redotix99: for 3D Shader Based Scopes 		(sorry for using the nightvision phase file)
void CRenderTarget::phase_3DSSReticle()
{
	PIX_EVENT(PHASE_SCOPE_RETICLE);
	HW.pContext->CopyResource(rt_Generic_2->pTexture->surface_get(), RImplementation.Target->rt_Position->pTexture->surface_get());

	if (!Device.m_SecondViewport.IsSVPActive())
		HW.pContext->CopyResource(rt_secondVP->pTexture->surface_get(), rt_Generic_0->pTexture->surface_get());

	u_setrt(RImplementation.Target->rt_Generic_0, nullptr, RImplementation.Target->rt_Position, RImplementation.Target->baseZB);


	RCache.set_CullMode(CULL_CCW);
	RCache.set_Stencil(FALSE);
	RCache.set_ColorWriteEnable();

	draw_reflex();

	{   PIX_EVENT(SCOPE_PHASE_JITTERFIX);

		// Write a dark color to the jittered geometry position to ensure
		//  no light/sky bleed
		draw_scope(s_scope_color_write, [](auto N) -> void {
			RCache.set_c("scope_phase", SCOPE_PHASE_JITTERFIX);
		});
	}

	{   // Remap gbuffer bindings to where the scope was rendered
		auto f = [&](LPCSTR name, ref_rt& target) -> void {
			ref_texture t;
			t.create(name);
			RCache.override_Texture(t->cName, target->pTexture);
		};

		auto svp = Device.m_SecondViewport.IsSVPActive();
		
		f(r2_RT_secondVP, svp ? RImplementation.TargetSVP->rt_secondVP : RImplementation.TargetMain->rt_secondVP);		
		f(r2_RT_generic2, svp ? RImplementation.TargetSVP->rt_Position : RImplementation.TargetMain->rt_Generic_2);
		f(r2_RT_heat,     svp ? RImplementation.TargetSVP->rt_Heat : RImplementation.TargetMain->rt_Heat);

		u_setrt(RImplementation.TargetMain->rt_Generic_0, nullptr, RImplementation.TargetMain->rt_Position, RImplementation.TargetMain->baseZB);
	}

	{   PIX_EVENT(SCOPE_PHASE_IMAGE);
		draw_scope(s_scope_color_write, [](auto N) -> void {
			RCache.set_c("scope_phase", SCOPE_PHASE_IMAGE);

			{	// Set screen_res to the gbuffer size
				auto t = Device.m_SecondViewport.IsSVPActive() ? RImplementation.TargetSVP : RImplementation.TargetMain;
				RCache.set_c("screen_res", Fvector4({(float)t->Width, (float)t->Height, 1.0f/(float)t->Width, 1.0f/(float)t->Height}));
			}

			{	// Resolution of the view we are rendering into
				auto t = RImplementation.TargetMain;
				RCache.set_c("output_res", Fvector4({(float)t->Width, (float)t->Height, 1.0f/(float)t->Width, 1.0f/(float)t->Height}));
			}

			auto P = Device.m_SecondViewport;
			Fvector up = {0,1,0};
			P.objective.m_W.transform_dir(up);
			Device.mView.transform_dir(up);

			up.z = 0.0;
			up.normalize();
			auto angle = acos(up.dotproduct({0,1,0})) * (up.x > 0 ? 1 : -1);

			RCache.set_c("hack_tex_angle", angle);
		});
	}
	
	RImplementation.TargetMain->SetActive(true); // Force set main target to fully invalidate rcache
	u_setrt(RImplementation.TargetMain->rt_Generic_0, nullptr, RImplementation.TargetMain->rt_Position, RImplementation.TargetMain->baseZB);

	{   PIX_EVENT(SCOPE_PHASE_RETICLE);
		draw_scope(s_scope_color_write, [](auto N) -> void {
			RCache.set_c("scope_phase", SCOPE_PHASE_RETICLE);
		});
	}

	{   PIX_EVENT(SCOPE_PHASE_SHADOW);
		draw_scope(s_scope_color_write, [](auto N) -> void {
			RCache.set_c("scope_phase", SCOPE_PHASE_SHADOW);
		});
	}

	{   PIX_EVENT(SCOPE_PHASE_LENS);
		draw_scope(s_scope_color_write, [](auto N) -> void {
			RCache.set_c("scope_phase", SCOPE_PHASE_LENS);
		});
	}

	{   PIX_EVENT(SCOPE_PHASE_CUSTOM_DEPTH);
		// I don't think we need to stencil this?
		// Some hud items in front of scopes may have incorrect blur, but this should be hardly noticeable
		// Allow a custom shader to override the depth for DOF calculations
		u_setrt(RImplementation.Target->rt_Position, nullptr, nullptr, nullptr, RImplementation.Target->baseZB);
		draw_scope(s_scope_depth_write, [](auto _) -> void {
			RCache.set_c("scope_phase", SCOPE_PHASE_DEPTHWRITE | SCOPE_PHASE_CUSTOM_DEPTH);
			RCache.set_c("scope_depth_value", 1);
		});
	}

	u_setrt(RImplementation.Target->rt_Generic_0, RImplementation.Target->rt_Position, RImplementation.Target->baseZB);
};

/** Run scope preprocesson the current frame and store in svp rt.
  * (Handle anything that needs to read from the g-buffer here.)
  */
void CRenderTarget::phase_svp_capture()
{
	PIX_EVENT(PHASE_SCOPE_SVP_CAPTURE);
	// This copy is not necessary, remove in the future and read directly from rt_Generic_0
	HW.pContext->CopyResource(RImplementation.TargetSVP->rt_secondVP->pSurface, RImplementation.TargetSVP->rt_Generic_0->pSurface);
};
#endif
