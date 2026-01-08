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
#include "../../stalker_decision_space.h"
#include "../../ai_monster_space.h"
#include "../../stalker_planner.h"

#ifdef DEBUG
#	include "../../ai_debug.h"
extern Flags32 psAI_Flags;
#endif // DEBUG

bool CAI_Stalker::feel_vision_isRelevant(CObject* O)
{
	if (!g_Alive())
		return false;

	// Distance Check
	float dist_sq = Position().distance_to_sqr(O->Position());
	if (dist_sq > eye_range * eye_range)
		return false;

	// If the object is too far above or below then ignore it
	if (_abs(O->Position().y - Position().y) > 50.0f)
		return false;

	// If dot < 0 AND dist > 5.0f then return false
	if (dist_sq > 25.0f) {
		Fvector dir_to_target;
		dir_to_target.sub(O->Position(), Position());
		if (Direction().dotproduct(dir_to_target) < 0.0f)
			return false;
	}

	CGameObject* GO = smart_cast<CGameObject*>(O);
	if (!GO)
		return false;

	CEntityAlive* E = GO->cast_entity_alive();
	CInventoryItem* I = GO->cast_inventory_item();

	if (I) {
		// Limit item checks to about 3 times per second
		if ((Device.dwTimeGlobal + GO->ID() * 33) % 333 > 33)
			return false;
	}
	
	// If in combat
	bool bCombat = (brain().current_action_id() == StalkerDecisionSpace::eWorldOperatorCombatPlanner);
	if (!bCombat) 
		bCombat = (movement().mental_state() == MonsterSpace::eMentalStateDanger);

	if (bCombat) {
		if (I) return false; // Ignore items in combat
		if (E && !E->g_Alive()) return false; // Ignore corpses in combat
	}

	if (!E && !I) return (false);
	if (E && !is_relation_enemy(E) && E != Actor()) 
		return false; // Only look at enemies

	if (O->Visual()) {
		// 1.0 = fully transparent
		// 0.0 = opaque 
		float fTransparency = memory().visual().feel_vision_mtl_transp(O, 0);
		if (fTransparency > 0.95f)
			return false;
	}

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
			animation().add_animation_stats();
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