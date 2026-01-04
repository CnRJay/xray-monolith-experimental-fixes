#pragma once

#include "rocketlauncher.h"
#include "WeaponMagazined.h"
#include "script_export_space.h"
#include "WeaponSSRS.h"

class CWeaponSSRS : public CRocketLauncher,
                    public CWeaponMagazined
{
	typedef CRocketLauncher inheritedRL;
	typedef CWeaponMagazined inheritedWM;

public:
	virtual ~CWeaponSSRS();
	virtual BOOL net_Spawn(CSE_Abstract* DC);
	virtual void Load(LPCSTR section);
	virtual void OnEvent(NET_Packet& P, u16 type);

#ifdef CROCKETLAUNCHER_CHANGE
	virtual void UnloadRocket();
#endif

protected:
	virtual void FireStart();
	virtual void ReloadMagazine();
	virtual void state_Fire(float dt);
	float fAiOneShotTime;

DECLARE_SCRIPT_REGISTER_FUNCTION
};
