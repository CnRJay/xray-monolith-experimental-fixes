#include "stdafx.h"
#include "holder_custom.h"
#include "actor.h"

bool CHolderCustom::attach_Actor(CGameObject* actor)
{
	m_owner = actor;
	m_ownerActor = smart_cast<CActor*>(actor);

	return true;
}

void CHolderCustom::detach_Actor()
{
	m_owner = nullptr;
	m_ownerActor = nullptr;
}

#ifdef HOLDERCUSTOM_NEW
CScriptGameObject *CHolderCustom::Owner_script()
{
	return (m_owner) ? m_owner->lua_game_object() : nullptr;
}
#endif