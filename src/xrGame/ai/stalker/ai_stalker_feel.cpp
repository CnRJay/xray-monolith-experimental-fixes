////////////////////////////////////////////////////////////////////////////
//	Module 		: ai_stalker_feel.cpp
//	Created 	: 25.02.2003
//  Modified 	: 25.02.2003
//	Author		: Dmitriy Iassenev
//	Description : Feelings for monster "Stalker"
////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ai_stalker.h"
#include "../../inventory_item.h"
#include "../../memory_manager.h"
#include "../../visual_memory_manager.h"
#include "../../sight_manager.h"
#include "../../stalker_movement_manager_smart_cover.h"
#include "../../stalker_animation_manager.h"
#include "CustomZone.h"
#include "../../sound_memory_manager.h"

#include <tbb/parallel_for.h>
#include <tbb/enumerable_thread_specific.h>

#ifdef DEBUG
#	include "../../ai_debug.h"
	extern Flags32 psAI_Flags;
#endif // DEBUG

bool CAI_Stalker::feel_vision_isRelevant(CObject* O)
{
	if (!g_Alive())
		return false;
	CEntityAlive* E = smart_cast<CEntityAlive*>(O);
	CInventoryItem* I = smart_cast<CInventoryItem*>(O);
	if (!E && !I) return (false);
	//	if (E && (E->g_Team() == g_Team()))			return false;
	return (true);
}

void CAI_Stalker::renderable_Render()
{
	inherited::renderable_Render();

	if (!already_dead())
		CInventoryOwner::renderable_Render();

#ifdef DEBUG
	if (g_Alive()) {
		if (psAI_Flags.test(aiAnimationStats))
			animation().add_animation_stats	();
	}
#endif // DEBUG
}

void CAI_Stalker::Exec_Look(float dt)
{
	sight().Exec_Look(dt);
}

bool CAI_Stalker::bfCheckForNodeVisibility(u32 dwNodeID, bool bIfRayPick)
{
	return (memory().visual().visible(dwNodeID, movement().m_head.current.yaw, ffGetFov()));
}

extern BOOL g_ai_die_in_anomaly;
bool CAI_Stalker::feel_touch_contact(CObject* O)
{
	if (!m_take_items_enabled && smart_cast<CInventoryItem*>(O))
		return (false);

	if (O == this)
		return (false);

	if (!inherited::feel_touch_contact(O))
		return (false);

	CGameObject* game_object = smart_cast<CGameObject*>(O);
	if (!game_object)
		return (false);

	// demonized: add g_ai_die_in_anomaly == 0 and m_enable_anomalies_pathfinding check
	// when 0 - disable pathfinding around anomaly
	if (!(g_ai_die_in_anomaly || m_enable_anomalies_pathfinding)) {
		CCustomZone* sr = smart_cast<CCustomZone*>(O);
		if (sr && (sr->spatial.type & STYPE_VISIBLEFORAI)) {
			return false;
		}
	}

	return (game_object->feel_touch_on_contact(this));
}

bool CAI_Stalker::feel_touch_on_contact(CObject* O)
{
	VERIFY(O != this);

	if ((O->spatial.type | STYPE_VISIBLEFORAI) != O->spatial.type)
		return (false);

	// demonized: add g_ai_die_in_anomaly == 0 and m_enable_anomalies_damage check
	// when 0 - prevent any damage from anomalies
	if (!(g_ai_die_in_anomaly || m_enable_anomalies_damage)) {
		CCustomZone* sr = smart_cast<CCustomZone*>(O);
		if (sr) {
			return false;
		}
	}

	return (inherited::feel_touch_on_contact(O));
}

void CAI_Stalker::feel_touch_delete(CObject* O)
{
	ignored_touched_objects_type::iterator i = std::find(m_ignored_touched_objects.begin(),
	                                                     m_ignored_touched_objects.end(), O);
	if (i == m_ignored_touched_objects.end())
		return;

	m_ignored_touched_objects.erase(i);
}

void CAI_Stalker::feel_sound_new()
{
	if (!g_Alive()) return;

	// Get all playing sounds
	const xr_vector<SoundEvent>& events = ::Sound->GetEvents();
	if (events.empty()) return;

	tbb::enumerable_thread_specific<xr_vector<SoundEvent>> audible_sounds_tls;

	Fvector my_pos = Position();
	float hearing_threshold = memory().sound().threshold();

	tbb::parallel_for(tbb::blocked_range<size_t>(0, events.size()),
		[&](const tbb::blocked_range<size_t>& r)
		{
			xrXRC xrc_local;
			collide::rq_results r_temp_local;
			xr_vector<ISpatial*> r_spatial_local;
			collide::rq_results results;

			xr_vector<SoundEvent>& local_audible = audible_sounds_tls.local();

			for (size_t i = r.begin(); i != r.end(); ++i)
			{
				const SoundEvent& event = events[i];
				ref_sound_data_ptr sd = event.first;
				
				if (!sd || !sd->feedback) continue;
				
				const CSound_params* p = sd->feedback->get_params();
				if (!p) continue;

				Fvector snd_pos = p->position;
				float power = event.second;
				
				float dist_sqr = my_pos.distance_to_sqr(snd_pos);
				float max_dist = power * hearing_threshold;
				
				if (dist_sqr > max_dist * max_dist) continue;

				float dist = _sqrt(dist_sqr);

				if (dist < EPS_L) {
					local_audible.push_back(event);
					continue;
				}

				// Check if sound is occluded by static geometry
				collide::ray_defs rq(my_pos, Fvector().sub(snd_pos, my_pos).normalize(), dist, CDB::OPT_ONLYNEAREST | CDB::OPT_CULL, collide::rqtStatic);
				
				BOOL hit = Level().ObjectSpace.RayQueryParallel(results, rq, NULL, NULL, NULL, this, xrc_local, r_temp_local, r_spatial_local);
				
				if (hit) {
					continue;
				}

				// the sound is audible here
				local_audible.push_back(event);
			}
		}
	);

	for (const auto& local_audible : audible_sounds_tls)
	{
		for (const auto& event : local_audible)
		{
			ref_sound_data_ptr sd = event.first;
			const CSound_params* p = sd->feedback->get_params();
			if (!p) continue;

			memory().sound().feel_sound_new(
				sd->g_object, 
				sd->g_type, 
				sd->g_userdata, 
				p->position, 
				event.second
			);
		}
	}
}
