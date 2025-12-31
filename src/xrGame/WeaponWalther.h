#pragma once

//#include "weaponrevolver.h"
#include "WeaponPistol.h"
#include "script_export_space.h"

class CWeaponWalther :
	public CWeaponPistol
{
	typedef CWeaponPistol inherited;
public:
	CWeaponWalther(void);
	virtual ~CWeaponWalther(void);

DECLARE_SCRIPT_REGISTER_FUNCTION
};
