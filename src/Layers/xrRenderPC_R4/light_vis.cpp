#include "StdAfx.h"
#include "../xrRender/light.h"
#include "../../xrEngine/cl_intersect.h"
#include "../../xrGame/debug_renderer.h"

const u32 delay_small_min = 1;
const u32 delay_small_max = 3;
const u32 delay_large_min = 10;
const u32 delay_large_max = 20;
const u32 cullfragments = 4;

void light::vis_prepare()
{
	PIX_EVENT(LIGHT_PREPARE);
	if (int(indirect_photons) != ps_r2_GI_photons) gi_generate();

	//	. test is sheduled for future	= keep old result
	//	. test time comes :)
	//		. camera inside light volume	= visible,	shedule for 'small' interval
	//		. perform testing				= ???,		pending

	u32 frame = Device.dwFrame;
	if (frame < vis.frame2test)
		return; // Not time to test

	if (vis.r4_queries.empty())
		vis.queryframe = frame;

	if (frame != vis.queryframe)
		return; // Queries have already been sent
	
	R_occlusion::occq_try_result r;
	xform_calc();
	RCache.set_xform_world(m_xform);
	vis.query_order = RImplementation.occq_begin(vis.query_id);
	vis.r4_queries.push_back({vis.query_id, r});
	RImplementation.Target->draw_volume(this);
	RImplementation.occq_end(vis.query_id);
}

void light::vis_update()
{
	auto frame = Device.dwFrame;

	auto light_to_player = Fvector(Device.vCameraPosition).sub(position);
	auto distance = light_to_player.magnitude();
	auto inside_dist = distance < range;
	auto inside_fov  = acos(light_to_player.normalize().dotproduct(direction.normalize())) < deg2rad(120.0f * 0.5);
	auto critical_dist = distance < 1.0;

	auto always = critical_dist || (inside_dist && inside_fov);

	if (scope_debug >= 3) {
		auto p = Fvector(direction.normalize()).mul(range).add(position);
		auto c = color_rgba_f(inside_dist && inside_fov, inside_dist && !inside_fov, inside_fov && !inside_dist, 1.0);
		CDebugRenderer().draw_aabb(p, 0.05, 0.05, 0.05, c, false);
	}
	
	if (always) {
		// Light could potentially be visible with a pending query
		//    so we force it on here.
		vis.visible = true;
		vis.visible_frags = RImplementation.Target->Width * RImplementation.Target->Height;
	}

	//	. not pending	->>> return (early out)
	//	. test-result:	visible:
	//		. shedule for 'large' interval
	//	. test-result:	invisible:
	//		. shedule for 'next-frame' interval
	if (vis.r4_queries.empty()) return;

	// Non blocking vis check.
	// Possible minor delays with light visiblity changes
	for (auto &q : vis.r4_queries) {
		if (!q.second.complete) {
			auto r = RImplementation.occq_try_get(q.first);
			if (r.complete) q.second = r;
			else return;
		}
	}

	if (!always) vis.visible_frags = 0;
	for (auto &q : vis.r4_queries) {
		vis.visible_frags += q.second.fragments;
	}
	vis.r4_queries.clear();

	vis.visible = vis.visible_frags > cullfragments;
	vis.pending = false;
	if (vis.visible)
	{
		vis.frame2test = frame + ::Random.randI(delay_large_min, delay_large_max);
	}
	else
	{
		vis.frame2test = frame + 1;
	}
}
