#include "pch_script.h"
#include "map_manager.h"
#include "alife_registry_wrappers.h"
#include "inventoryowner.h"
#include "level.h"
#include "actor.h"
#include "relation_registry.h"
#include "GameObject.h"
#include "map_location.h"
#include "GameTaskManager.h"
#include "xrServer.h"
#include "game_object_space.h"
#include "script_callback_ex.h"

struct FindLocationBySpotID
{
	shared_str spot_id;
	u16 object_id;

	FindLocationBySpotID(const shared_str& s, u16 id): spot_id(s), object_id(id)
	{
	}

	bool operator ()(const SLocationKey& key)
	{
		return (spot_id == key.spot_type) && (object_id == key.object_id);
	}
};

struct FindLocationByID
{
	u16 object_id;

	FindLocationByID(u16 id): object_id(id)
	{
	}

	bool operator ()(const SLocationKey& key)
	{
		return (object_id == key.object_id);
	}
};

struct FindLocation
{
	CMapLocation* ml;

	FindLocation(CMapLocation* m): ml(m)
	{
	}

	bool operator ()(const SLocationKey& key)
	{
		return (ml == key.location);
	}
};

void SLocationKey::save(IWriter& stream)
{
	stream.w(&object_id, sizeof(object_id));

	stream.w_stringZ(spot_type);
	stream.w_u8(0);
	location->save(stream);
}

void SLocationKey::load(IReader& stream)
{
	stream.r(&object_id, sizeof(object_id));

	stream.r_stringZ(spot_type);
	stream.r_u8();

	location = xr_new<CMapLocation>(*spot_type, object_id);

	location->load(stream);
}

void SLocationKey::destroy()
{
	delete_data(location);
}

void CMapLocationRegistry::save(IWriter& stream)
{
	stream.w_u32((u32)objects().size());
	iterator I = m_objects.begin();
	iterator E = m_objects.end();
	for (; I != E; ++I)
	{
		u32 size = 0;
		Locations::iterator i = (*I).second.begin();
		Locations::iterator e = (*I).second.end();
		for (; i != e; ++i)
		{
			VERIFY((*i).location);
			if ((*i).location->Serializable())
				++size;
		}
		stream.w(&(*I).first, sizeof((*I).first));
		stream.w_u32(size);
		i = (*I).second.begin();
		for (; i != e; ++i)
			if ((*i).location->Serializable())
				(*i).save(stream);
	}
}


void CMapManager::AddToCache(u16 id)
{
	if (std::find(m_cached_spot_objects.begin(), m_cached_spot_objects.end(), id) == m_cached_spot_objects.end())
		m_cached_spot_objects.push_back(id);
}

void CMapManager::RemoveFromCache(u16 id)
{
	bool bFound = false;
	Locations_it it = Locations().begin();
	Locations_it it_e = Locations().end();
	for (; it != it_e; ++it)
	{
		if ((*it).actual && (*it).object_id == id)
		{
			bFound = true;
			break;
		}
	}

	if (!bFound)
	{
		xr_vector<u16>::iterator it_c = std::find(m_cached_spot_objects.begin(), m_cached_spot_objects.end(), id);
		if (it_c != m_cached_spot_objects.end())
			m_cached_spot_objects.erase(it_c);
	}
}

CMapManager::CMapManager()
{
	m_locations_wrapper = xr_new<CMapLocationWrapper>();
	m_locations_wrapper->registry().init(1);
	m_locations = NULL;
}

CMapManager::~CMapManager()
{
	delete_data(m_deffered_destroy_queue); //from prev frame
	delete_data(m_locations_wrapper);
}

CMapLocation* CMapManager::AddMapLocation(const shared_str& spot_type, u16 id)
{
	CMapLocation* l = xr_new<CMapLocation>(spot_type.c_str(), id);
	Locations().push_back(SLocationKey(spot_type, id));
	Locations().back().location = l;
	if (IsGameTypeSingle() && g_actor)
		Actor()->callback(GameObject::eMapLocationAdded)(spot_type.c_str(), id);

	AddToCache(id);
	
	std::sort(Locations().begin(), Locations().end());

	return l;
}

CMapLocation* CMapManager::AddRelationLocation(CInventoryOwner* pInvOwner)
{
	if (!Level().CurrentViewEntity())return NULL;

	ALife::ERelationType relation = ALife::eRelationTypeFriend;
	CInventoryOwner* pActor = smart_cast<CInventoryOwner*>(Level().CurrentViewEntity());
	relation = RELATION_REGISTRY().GetRelationType(pInvOwner, pActor);
	shared_str sname = RELATION_REGISTRY().GetSpotName(relation);

	CEntityAlive* pEntAlive = smart_cast<CEntityAlive*>(pInvOwner);
	if (!pEntAlive->g_Alive()) sname = "deadbody_location";


	R_ASSERT(!HasMapLocation(sname, pInvOwner->object_id()));
	CMapLocation* l = xr_new<CRelationMapLocation>(sname, pInvOwner->object_id(), pActor->object_id());
	Locations().push_back(SLocationKey(sname, pInvOwner->object_id()));
	Locations().back().location = l;
	AddToCache(pInvOwner->object_id());
	std::sort(Locations().begin(), Locations().end());
	return l;
}

void CMapManager::Destroy(CMapLocation* ml)
{
	m_deffered_destroy_queue.push_back(ml);
}

void CMapManager::RemoveMapLocation(const shared_str& spot_type, u16 id)
{
	FindLocationBySpotID key(spot_type, id);
	Locations_it it = std::find_if(Locations().begin(), Locations().end(), key);
	if (it != Locations().end())
	{
		if (IsGameTypeSingle())
			Level().GameTaskManager().MapLocationRelcase((*it).location);

		Destroy((*it).location);
		Locations().erase(it);
		RemoveFromCache(key.object_id);
	}
}

// demonized: remove all map object spots by id
void CMapManager::RemoveAllMapLocationsById(u16 id)
{
	for (Locations_it it = Locations().begin(); it != Locations().end(); ) {
		if (it->object_id == id) {
			if (IsGameTypeSingle())
				Level().GameTaskManager().MapLocationRelcase((*it).location);
			Destroy((*it).location);
			it = Locations().erase(it);
		} else {
			it++;
		}
	}
	RemoveFromCache(id);
}

void CMapManager::RemoveMapLocationByObjectID(u16 id) //call on destroy object
{
	FindLocationByID key(id);
	Locations_it it = std::find_if(Locations().begin(), Locations().end(), key);
	while (it != Locations().end())
	{
		if (IsGameTypeSingle())
			Level().GameTaskManager().MapLocationRelcase((*it).location);

		Destroy((*it).location);
		Locations().erase(it);

		it = std::find_if(Locations().begin(), Locations().end(), key);
	}
	RemoveFromCache(id);
}

void CMapManager::RemoveMapLocation(CMapLocation* ml)
{
	FindLocation key(ml);

	Locations_it it = std::find_if(Locations().begin(), Locations().end(), key);
	if (it != Locations().end())
	{
		u16 id = (*it).object_id;
		if (IsGameTypeSingle())
			Level().GameTaskManager().MapLocationRelcase((*it).location);

		Destroy((*it).location);
		Locations().erase(it);
		RemoveFromCache(id);
	}
}

bool CMapManager::GetMapLocationsForObject(u16 id, xr_vector<CMapLocation*>& res)
{
	res.clear_not_free();
	
	SLocationKey key;
	key.object_id = id;
	key.actual = true;

	Locations_it it = std::lower_bound(Locations().begin(), Locations().end(), key);
	Locations_it it_e = Locations().end();

	for (; it != it_e; ++it)
	{
		if (!(*it).actual || (*it).object_id != id)
			break;

		res.push_back((*it).location);
	}

	return (res.size() != 0);
}

bool CMapManager::HasMapLocation(const shared_str& spot_type, u16 id)
{
	CMapLocation* l = GetMapLocation(spot_type, id);

	return (l != NULL);
}

CMapLocation* CMapManager::GetMapLocation(const shared_str& spot_type, u16 id)
{
	FindLocationBySpotID key(spot_type, id);
	Locations_it it = std::find_if(Locations().begin(), Locations().end(), key);
	if (it != Locations().end())
		return (*it).location;

	return 0;
}

void CMapManager::GetMapLocations(const shared_str& spot_type, u16 id, xr_vector<CMapLocation*>& res)
{
	FindLocationBySpotID key(spot_type, id);
	Locations_it it = std::find_if(Locations().begin(), Locations().end(), key);

	while (it != Locations().end())
	{
		res.push_back((*it).location);
		it = std::find_if(++it, Locations().end(), key);
	}
}

// demonized: get all map locations by id
void CMapManager::GetMapLocations(u16 id, xr_vector<CMapLocation*>& res)
{
	FindLocationByID key(id);
	Locations_it it = std::find_if(Locations().begin(), Locations().end(), key);
	while (it != Locations().end())
	{
		res.push_back((*it).location);
		it = std::find_if(++it, Locations().end(), key);
	}
}

void CMapManager::Update()
{
	delete_data(m_deffered_destroy_queue); //from prev frame

	Locations_it it = Locations().begin();
	Locations_it it_e = Locations().end();

	for (u32 idx = 0; it != it_e; ++it, ++idx)
	{
		bool bForce = Device.dwFrame % 3 == idx % 3;
		(*it).actual = (*it).location->Update();

		if ((*it).actual && bForce)
			(*it).location->CalcPosition();
	}
	std::sort(Locations().begin(), Locations().end());

	while ((!Locations().empty()) && (!Locations().back().actual))
	{
		u16 id = Locations().back().object_id;
		if (IsGameTypeSingle())
			Level().GameTaskManager().MapLocationRelcase(Locations().back().location);

		Destroy(Locations().back().location);
		Locations().pop_back();
		RemoveFromCache(id);
	}
}

void CMapManager::DisableAllPointers()
{
	Locations_it it = Locations().begin();
	Locations_it it_e = Locations().end();

	for (; it != it_e; ++it)
		(*it).location->DisablePointer();
}

void CMapManager::ReloadSpots()
{
	Locations_it it = Locations().begin();
	Locations_it it_e = Locations().end();

	for (; it != it_e; ++it)
		(*it).location->LoadSpot((*it).location->spot_type, false);
}

Locations& CMapManager::Locations()
{
	if (!m_locations)
	{
		m_locations = &m_locations_wrapper->registry().objects();
#ifdef DEBUG
		Msg("m_locations size=%d",m_locations->size());
#endif // #ifdef DEBUG
	}
	return *m_locations;
}

void CMapManager::OnObjectDestroyNotify(u16 id)
{
	RemoveMapLocationByObjectID(id);
}

#ifdef DEBUG
void CMapManager::Dump						()
{
	Msg("begin of map_locations dump");
	Locations_it it = Locations().begin();
	Locations_it it_e = Locations().end();
	for(; it!=it_e;++it)
	{
		Msg("spot_type=[%s] object_id=[%d]",*((*it).spot_type), (*it).object_id);
		(*it).location->Dump();
	}

	Msg("end of map_locations dump");
}
#endif
