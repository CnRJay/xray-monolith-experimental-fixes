#pragma once
#include "../BaseMonster/base_monster.h"
#include "../../../../xrServerEntities/script_export_space.h"

class CCat : public CBaseMonster
{
	typedef CBaseMonster inherited;
public:
	CCat();
	virtual ~CCat();

	virtual void Load(LPCSTR section);
	virtual void reinit();

	virtual void UpdateCL();

	virtual void CheckSpecParams(u32 spec_params);

	virtual void HitEntityInJump(const CEntity* pEntity);

	virtual char* get_monster_class_name() { return "cat"; }


DECLARE_SCRIPT_REGISTER_FUNCTION
};
