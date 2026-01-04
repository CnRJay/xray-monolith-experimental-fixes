////////////////////////////////////////////////////////////////////////////
//	Module 		: base_client_classes_wrappers.h
//	Created 	: 20.12.2004
//  Modified 	: 20.12.2004
//	Author		: Dmitriy Iassenev
//	Description : XRay base client classes wrappers
////////////////////////////////////////////////////////////////////////////

#pragma once

#include "script_export_space.h"
#include "base_client_classes.h"
#include "../xrEngine/engineapi.h"
#include "../xrcdb/ispatial.h"
#include "../xrEngine/isheduled.h"
#include "../xrEngine/irenderable.h"
#include "../xrEngine/icollidable.h"
#include "../xrEngine/xr_object.h"
#include "entity.h"
#include "ai_space.h"
#include "script_engine.h"
#include "xrServer_Object_Base.h"

#pragma warning(push)
#pragma warning(disable: 4584)
template <typename Base, typename LuabindBase = xr_empty>
class DLL_PureWrapper : public Base, public LuabindBase
{
public:
	IC DLL_PureWrapper() {}
	virtual ~DLL_PureWrapper() {}

	virtual DLL_Pure* _construct()
	{
		return call_member<DLL_Pure*>(this, "_construct");
	}

	static DLL_Pure* _construct_static(Base* self_)
	{
		return self_->Base::_construct();
	}
};

/*	
template <typename base, typename luabind_base = Loki::EmptyType>
class ISpatialWrapper : public heritage<base,luabind_base>::result {
public:
	IC						ISpatialWrapper				() {};
	virtual					~ISpatialWrapper			() {};
	virtual	void			spatial_register			()
	{
		call_member<void>(this,"spatial_register");
	}

	static	void			spatial_register_static		(base *self)
	{
		self->base::spatial_register();
	}

	virtual	void			spatial_unregister			()
	{
		call_member<void>(this,"spatial_unregister");
	}
	
	static	void			spatial_unregister_static	(base *self)
	{
		self->base::spatial_unregister();
	}

	virtual	void			spatial_move				()
	{
		call_member<void>(this,"spatial_move");
	}

	static	void			spatial_move_static			(base *self)
	{
		self->base::spatial_move();
	}

	virtual	Fvector			spatial_sector_point		()
	{
		return	(call_member<Fvector>(this,"spatial_sector_point"));
	}

	static	Fvector			spatial_sector_point_static	(base *self)
	{
		return	(self->base::spatial_sector_point());
	}
	
	virtual	CObject*		dcast_CObject				()
	{
		return	(call_member<CObject*>(this,"dcast_CObject"));
	}

	static	CObject*		dcast_CObject_static		(base *self)
	{
		return	(self->base::dcast_CObject());
	}

	virtual	Feel::Sound*	dcast_FeelSound				()
	{
		return	(call_member<Feel::Sound*>(this,"dcast_FeelSound"));
	}

	static	Feel::Sound*	dcast_FeelSound_static		(base *self)
	{
		return	(self->base::dcast_FeelSound());
	}

	virtual	IRenderable*	dcast_Renderable			()
	{
		return	(call_member<IRenderable*>(this,"dcast_Renderable"));
	}

	static	IRenderable*	dcast_Renderable_static		(base *self)
	{
		return	(self->base::dcast_Renderable());
	}
};

typedef ISpatialWrapper<ISpatial,::luabind::wrap_base> CISpatialWrapper;
*/

template <typename Base, typename LuabindBase = xr_empty>
class ISheduledWrapper : public Base, public LuabindBase
{
public:
	IC ISheduledWrapper() = default;
	virtual ~ISheduledWrapper() = default;

	virtual float shedule_Scale() override
	{
		return 1.f;
	}

	virtual void shedule_Update(u32 dt) override
	{
		Base::shedule_Update(dt);
	}
};

template <typename Base, typename LuabindBase = xr_empty>
class IRenderableWrapper : public Base, public LuabindBase
{
public:
	IC IRenderableWrapper() = default;
	virtual ~IRenderableWrapper() = default;
};

//typedef DLL_PureWrapper<CObject,::luabind::wrap_base> CObjectDLL_Pure;
//typedef ISpatialWrapper<CObjectDLL_Pure>			CObjectISpatial;
//typedef ISheduledWrapper<CObjectDLL_Pure>			CObjectISheduled;
//typedef IRenderableWrapper<CObjectISheduled>		CObjectIRenderable;

//class CObjectWrapper : public CObjectIRenderable {
//public:
//	IC						CObjectWrapper		() {};
//	virtual					~CObjectWrapper		() {};
///**
//	virtual BOOL			Ready				();
//	virtual CObject*		H_SetParent			(CObject* O);
//	virtual void			Center				(Fvector& C) const;
//	virtual float			Radius				() const;
//	virtual const Fbox&		BoundingBox			() const;
//	virtual void			Load				(LPCSTR section);
//	virtual void			UpdateCL			();
//	virtual BOOL			net_Spawn			(CSE_Abstract* data);
//	virtual void			net_Destroy			();
//	virtual void			net_Export			(NET_Packet& P);
//	virtual void			net_Import			(NET_Packet& P);
//	virtual	void			net_ImportInput		(NET_Packet& P);
//	virtual BOOL			net_Relevant		();
//	virtual void			net_MigrateInactive	(NET_Packet& P);
//	virtual void			net_MigrateActive	(NET_Packet& P);
//	virtual void			net_Relcase			(CObject* O);
//	virtual	SavedPosition	ps_Element			(u32 ID) const;
//	virtual void			ForceTransform		(const Fmatrix& m);
//	virtual void			OnHUDDraw			(CCustomHUD* hud);
//	virtual void			OnH_B_Chield		();
//	virtual void			OnH_B_Independent	(bool just_before_destroy);
//	virtual void			OnH_A_Chield		();
//	virtual void			OnH_A_Independent	();
///**/
//};


using CDLL_PureWrapper = DLL_PureWrapper<DLL_Pure, luabind::wrap_base>;
using CGameObjectDLL_Pure = DLL_PureWrapper<CGameObject, luabind::wrap_base>;
//typedef ISpatialWrapper<CGameObjectDLL_Pure>				CGameObjectISpatial;

using CISheduledWrapper = ISheduledWrapper<ISheduled, luabind::wrap_base>;
using CGameObjectISheduled = ISheduledWrapper<CGameObjectDLL_Pure>;

using CIRenderableWrapper = IRenderableWrapper<IRenderable, luabind::wrap_base>;
using CGameObjectIRenderable = IRenderableWrapper<CGameObjectISheduled>;

#pragma warning(pop)

class CGameObjectWrapper :
	public CGameObjectIRenderable
{
public:
	IC CGameObjectWrapper()
	{
	};

	virtual ~CGameObjectWrapper()
	{
	};

	virtual bool use(CGameObject* who_use)
	{
		return call<bool>("use", who_use);
	}

	static bool use_static(CGameObject* self, CGameObject* who_use)
	{
		return self->CGameObject::use(who_use);
	}


	virtual void net_Import(NET_Packet& packet)
	{
		call<void>("net_Import", &packet);
	}

	static void net_Import_static(CGameObject* self, NET_Packet* packet)
	{
		self->CGameObject::net_Import(*packet);
	}

	virtual void net_Export(NET_Packet& packet)
	{
		call<void>("net_Export", &packet);
	}

	static void net_Export_static(CGameObject* self, NET_Packet* packet)
	{
		self->CGameObject::net_Export(*packet);
	}

	virtual BOOL net_Spawn(CSE_Abstract* data)
	{
		return (::luabind::call_member<bool>(this, "net_Spawn", data));
	}

	static bool net_Spawn_static(CGameObject* self, CSE_Abstract* abstract)
	{
		return (!!self->CGameObject::net_Spawn(abstract));
	}
};

class CEntityWrapper : public CEntity, public ::luabind::wrap_base
{
public:
	IC CEntityWrapper()
	{
	}

	virtual ~CEntityWrapper()
	{
	}

	virtual void HitSignal(float P, Fvector& local_dir, CObject* who, s16 element)
	{
		::luabind::call_member<void>(this, "HitSignal", P, local_dir, who, element);
	}

	static void HitSignal_static(CEntity* self, float P, Fvector& local_dir, CObject* who, s16 element)
	{
		ai().script_engine().script_log(eLuaMessageTypeError,
		                                "You are trying to call a pure virtual function CEntity::HitSignal!");
	}

	virtual void HitImpulse(float P, Fvector& vWorldDir, Fvector& vLocalDir)
	{
		::luabind::call_member<void>(this, "HitImpulse", P, vWorldDir, vLocalDir);
	}

	static void HitImpulse_static(float P, Fvector& vWorldDir, Fvector& vLocalDir)
	{
		ai().script_engine().script_log(eLuaMessageTypeError,
		                                "You are trying to call a pure virtual function CEntity::HitImpulse!");
	}
};
