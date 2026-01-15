// HUDRecon.cpp: Distance and identification readout
// 
//////////////////////////////////////////////////////////////////////
#include "stdafx.h"
#include "HUDRecon.h"

#include "../xrEngine/Environment.h"
#include "../xrEngine/CustomHUD.h"
#include "Entity.h"
#include "Actor.h"
#include "Weapon.h"
#include "WeaponKnife.h"
#include "player_hud.h"
#include "Missile.h"
#include "level.h"
#include "game_cl_base.h"
#include "../xrEngine/igame_persistent.h"
#include "script_render_device.h"
#include "HUDManager.h"

#include "ui_base.h"
#include "InventoryOwner.h"
#include "relation_registry.h"
#include "character_info.h"

#include "string_table.h"
#include "entity_alive.h"

#include "inventory_item.h"
#include "inventory.h"

#include <ai/monsters/poltergeist/poltergeist.h>

u32 C_DEFAULT D3DCOLOR_RGBA(0xff, 0xff, 0xff, 0x80);
u32 C_ON_ENEMY D3DCOLOR_RGBA(0xff, 0, 0, 0x80);
u32 C_ON_NEUTRAL D3DCOLOR_RGBA(0xff, 0xff, 0x80, 0x80);
u32 C_ON_FRIEND D3DCOLOR_RGBA(0, 0xff, 0, 0x80);

float recon_show_speed = .5f;
float recon_hide_speed = 10.f;
float recon_mindist = 2.f;
float recon_maxdist = 50.f;
float recon_minspeed = .5f;
float recon_maxspeed = 10.f;

CHUDRecon::CHUDRecon()
{
	fuzzyShowInfo = 0.f;
	bDoTransform = false;
}

CHUDRecon::~CHUDRecon()
{
}

void CHUDRecon::SetDoTransform(bool d)
{
	bDoTransform = d;
}

void CHUDRecon::SetTransform(const Fmatrix& m)
{
	transform.set(m);
}

void CHUDRecon::SetOpacity(float a)
{
	color = subst_alpha(color, u8(iFloor(255.f * a)));
}

void CHUDRecon::Update(const SPickParam& pp)
{
	dist = pp.result.range;
	power = pp.power;
	pass = pp.pass;
	line1 = nullptr;
	line2 = nullptr;
	color = C_DEFAULT;

	CObject* O = pp.result.O;

	if (psHUD_Flags.test(HUD_INFO))
	{
		bool const is_poltergeist = O && !!smart_cast<CPoltergeist*>(O);

		if ((O && O->getVisible()) || is_poltergeist)
		{
			CEntityAlive* EA = smart_cast<CEntityAlive*>(O);
			CEntityAlive* pCurEnt = smart_cast<CEntityAlive*>(Level().CurrentEntity());
			PIItem l_pI = smart_cast<PIItem>(O);

			if (IsGameTypeSingle())
			{
				CInventoryOwner* our_inv_owner = smart_cast<CInventoryOwner*>(pCurEnt);

				if (EA && EA->g_Alive() && EA->cast_base_monster())
				{
					color = C_ON_ENEMY;
				}
				else if (EA && EA->g_Alive() && !EA->cast_base_monster())
				{
					CInventoryOwner* others_inv_owner = smart_cast<CInventoryOwner*>(EA);

					if (our_inv_owner && others_inv_owner)
					{
						switch (RELATION_REGISTRY().GetRelationType(others_inv_owner, our_inv_owner))
						{
						case ALife::eRelationTypeEnemy:
							color = C_ON_ENEMY;
							break;
						case ALife::eRelationTypeNeutral:
							color = C_ON_NEUTRAL;
							break;
						case ALife::eRelationTypeFriend:
							color = C_ON_FRIEND;
							break;
						}

						if (fuzzyShowInfo > 0.5f)
						{
							CStringTable strtbl;
							line1 = strtbl.translate(others_inv_owner->Name()).c_str();
							line2 = strtbl.translate(others_inv_owner->CharacterInfo().Community().id()).c_str();
						}
					}

					fuzzyShowInfo += recon_show_speed * Device.fTimeDelta;
				}
				else if (l_pI && our_inv_owner && dist < 2.0f * 2.0f)
				{
					fuzzyShowInfo += recon_show_speed * Device.fTimeDelta;
					if (fuzzyShowInfo > 0.5f && l_pI->NameItem())
					{
						line1 = l_pI->NameItem();
					}
				}
			}
			else
			{
				if (EA && (EA->GetfHealth() > 0))
				{
					if (pCurEnt && GameID() & eGameIDSingle)
					{
						if (GameID() & eGameIDDeathmatch) color = C_ON_ENEMY;
						else
						{
							if (EA->g_Team() != pCurEnt->g_Team()) color = C_ON_ENEMY;
							else color = C_ON_FRIEND;
						};
						if (dist >= recon_mindist && dist <= recon_maxdist)
						{
							float ddist = (dist - recon_mindist) / (recon_maxdist - recon_mindist);
							float dspeed = recon_minspeed + (recon_maxspeed - recon_minspeed) * ddist;
							fuzzyShowInfo += Device.fTimeDelta / dspeed;
						}
						else
						{
							if (dist < recon_mindist)
								fuzzyShowInfo += recon_minspeed * Device.fTimeDelta;
							else
								fuzzyShowInfo = 0;
						};

						if (fuzzyShowInfo > 0.5f)
						{
							line1 = pp.result.O->cName().c_str();
						}
					}
				};
			};
		}
		else
		{
			fuzzyShowInfo -= recon_hide_speed * Device.fTimeDelta;
		}
		clamp(fuzzyShowInfo, 0.f, 1.f);
	}
}

void CHUDRecon::Render() const
{
	Fvector4 pt = Fvector4();
	if (bDoTransform)
	{
		Device.mFullTransform.transform(pt, transform.c);
		pt.y = -pt.y;
	}

	// Readout font
	CGameFont* F = UI().Font().pFontGraffiti19Russian;
	F->SetAligment(CGameFont::alCenter);
	F->OutSetI(pt.x, pt.y + 0.05f);

	if (psHUD_Flags.test(HUD_CROSSHAIR_DIST))
		F->OutSkip();

	if (psHUD_Flags.test(HUD_INFO))
	{
		CStringTable strtbl;
		F->SetColor(subst_alpha(color, u8(iFloor(255.f * (fuzzyShowInfo - 0.5f) * 2.f))));

		if (line1)
			F->OutNext("%s", line1);

		if (line2)
			F->OutNext("%s", line2);
	}

	if (psHUD_Flags.test(HUD_CROSSHAIR_DIST))
	{
		F->OutSetI(pt.x, pt.y + 0.05f);
		F->SetColor(color);
#ifdef DEBUG
		F->OutNext("%4.1f - %4.2f - %d", dist, power, pass);
#else
		F->OutNext("%4.1f", dist);
#endif
	}
}