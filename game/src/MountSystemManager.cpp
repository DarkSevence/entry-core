//base CPetSystem

#include "stdafx.h"
#include "char.h"
#include "char_manager.h"
#include "config.h"
#include "item.h"
#include "item_manager.h"
#include "mob_manager.h"
#include "MountSystemManager.h"
#include "packet.h"
#include "sectree_manager.h"
#include "utils.h"
#include "vector.h"

EVENTINFO(mountsystem_event_info)
{
	CMountSystem* pMountSystem;
};

EVENTFUNC(mountsystem_update_event)
{
	auto* detailedEventInfo = dynamic_cast<mountsystem_event_info*>(event->info);
	
	if (!detailedEventInfo)
	{
		sys_err("<mountsystem_update_event> <Factor> Null pointer");
		return 0;
	}

	auto* mountSystem = detailedEventInfo->pMountSystem;

	if (!mountSystem)
	{
		return 0;
	}

	mountSystem->Update(0);
	return PASSES_PER_SEC(1) / 4;
}

CMountActor::CMountActor(LPCHARACTER owner, DWORD vnum): 
	mountVnum(vnum),
	m_dwVID(0),
	m_dwLastActionTime(0),
	m_pkChar(0),
	m_pkOwner(owner),
	m_originalMoveSpeed(0),
	m_dwSummonItemVID(0),
	m_dwSummonItemVnum(0)
{}

CMountActor::~CMountActor()
{
	this->Unsummon();
	m_pkOwner = 0;
}

void CMountActor::SetName()
{
	std::string MountName = m_pkOwner->GetName();

	if (IsSummoned())
	{
		MountName += " Wierzchowiec";
		m_pkChar->SetName(MountName);
	}

	m_name = std::move(MountName);
}

bool CMountActor::Mount(LPITEM mountItem)
{
	if (!m_pkOwner)
	{
		return false;
	}

	if (!mountItem)
	{
		return false;
	}
	
	if (m_pkOwner->IsHorseRiding())
	{
		m_pkOwner->StopRiding();
	}
	
	if (auto* horse = m_pkOwner->GetHorse())
	{
		horse->HorseSummon(false);
	}
	
	Unmount();

	m_pkOwner->AddAffect(AFFECT_MOUNT, POINT_MOUNT, mountVnum, AFF_NONE, static_cast<uint32_t>(mountItem->GetSocket(0) - time(nullptr)), 0, true);
	
	for (int32_t i = 0; i < ITEM_APPLY_MAX_NUM; ++i)
	{
		const auto& apply = mountItem->GetProto()->aApplies[i];
		
		if (apply.bType == APPLY_NONE)
		{
			continue;
		}

		m_pkOwner->AddAffect(AFFECT_MOUNT_BONUS, aApplyInfo[apply.bType].bPointType, apply.lValue, AFF_NONE, static_cast<uint32_t>(mountItem->GetSocket(0) - time(nullptr)), 0, false);
	}
	
	return m_pkOwner->GetMountVnum() == mountVnum;
}

void CMountActor::Unmount()
{
	if (!m_pkOwner)
	{
		return;
	}
	
	if (m_pkOwner->GetMountVnum() == 0)
	{
		return;
	}
	
	m_pkOwner->RemoveAffect(AFFECT_MOUNT);
	m_pkOwner->RemoveAffect(AFFECT_MOUNT_BONUS);
	m_pkOwner->MountVnum(0);
	
	if (m_pkOwner->IsHorseRiding())
	{
		m_pkOwner->StopRiding();
	}
	
	if (auto* horse = m_pkOwner->GetHorse())
	{
		horse->HorseSummon(false);
	}
	
	m_pkOwner->MountVnum(0);
}

void CMountActor::Unsummon()
{
	if (this->IsSummoned())
	{
		this->SetSummonItem(nullptr);
		
		if (m_pkChar)
		{
			M2_DESTROY_CHARACTER(m_pkChar);
		}

		m_pkChar = nullptr;
		m_dwVID = 0;
	}
}

DWORD CMountActor::Summon(LPITEM pSummonItem, bool bSpawnFar)
{
	auto x = m_pkOwner->GetX();
	auto y = m_pkOwner->GetY();
	auto z = m_pkOwner->GetZ();

	if (bSpawnFar)
	{
		x += (number(0, 1) * 2 - 1) * number(2000, 2500);
		y += (number(0, 1) * 2 - 1) * number(2000, 2500);
	}
	else
	{
		x += number(-100, 100);
		y += number(-100, 100);
	}
	
	if (m_pkChar)
	{
		m_pkChar->Show(m_pkOwner->GetMapIndex(), x, y);
		m_dwVID = m_pkChar->GetVID();
		return m_dwVID;
	}

	m_pkChar = CHARACTER_MANAGER::instance().SpawnMob(mountVnum, m_pkOwner->GetMapIndex(), x, y, z, false, static_cast<int>(m_pkOwner->GetRotation() + 180), false);

	if (!m_pkChar)
	{
		sys_err("[CMountActor::Summon] Failed to summon the mount. (vnum: %d)", mountVnum);
		
		return 0;
	}

	m_pkChar->SetMount();
	m_pkChar->SetEmpire(m_pkOwner->GetEmpire());
	m_dwVID = m_pkChar->GetVID();
	this->SetName();
	this->SetSummonItem(pSummonItem);
	m_pkChar->Show(m_pkOwner->GetMapIndex(), x, y, z);
	
	return m_dwVID;
}

bool CMountActor::_UpdateFollowAI()
{
	if (!m_pkChar->m_pkMobData)
	{
		return false;
	}

	if (m_originalMoveSpeed == 0)
	{
		if (auto* mobData = CMobManager::Instance().Get(mountVnum))
		{
			m_originalMoveSpeed = mobData->m_table.sMovingSpeed;
		}
	}
	
	constexpr float START_FOLLOW_DISTANCE = 300.0f;
	constexpr float RESPAWN_DISTANCE = 4500.f;
	constexpr int APPROACH = 200;

	auto currentTime = get_dword_time();

	auto ownerX = m_pkOwner->GetX();
	auto ownerY = m_pkOwner->GetY();
	auto charX = m_pkChar->GetX();
	auto charY = m_pkChar->GetY();

	auto fDist = DISTANCE_APPROX(charX - ownerX, charY - ownerY);

	if (fDist >= RESPAWN_DISTANCE)
	{
		auto fOwnerRot = m_pkOwner->GetRotation() * 3.141592f / 180.f;
		auto fx = -APPROACH * cos(fOwnerRot);
		auto fy = -APPROACH * sin(fOwnerRot);
		
		if (m_pkChar->Show(m_pkOwner->GetMapIndex(), ownerX + fx, ownerY + fy))
		{
			return true;
		}
	}

	if (fDist >= START_FOLLOW_DISTANCE)
	{
		m_pkChar->SetNowWalking(false);
		Follow(APPROACH);
		m_pkChar->SetLastAttacked(currentTime);
		m_dwLastActionTime = currentTime;
	}
	else
	{
		m_pkChar->SendMovePacket(FUNC_WAIT, 0, 0, 0, 0);
	}

	return true;
}

bool CMountActor::Update(DWORD deltaTime)
{
	bool bResult = true;

	if (m_pkOwner->IsDead() || (IsSummoned() && m_pkChar->IsDead()) || !ITEM_MANAGER::instance().FindByVID(this->GetSummonItemVID()) || ITEM_MANAGER::instance().FindByVID(this->GetSummonItemVID())->GetOwner() != this->GetOwner())
	{
		this->Unsummon();
		return true;
	}

	if (this->IsSummoned())
	{
		bResult &= this->_UpdateFollowAI();
	}

	return bResult;
}

bool CMountActor::Follow(float fMinDistance)
{
	if( !m_pkOwner || !m_pkChar)
	{
		return false;
	}

	auto fOwnerX = m_pkOwner->GetX();
	auto fOwnerY = m_pkOwner->GetY();
	auto fPetX = m_pkChar->GetX();
	auto fPetY = m_pkChar->GetY();

	auto fDist = DISTANCE_SQRT(fOwnerX - fPetX, fOwnerY - fPetY);
	if (fDist <= fMinDistance)
	{
		return false;
	}

	m_pkChar->SetRotationToXY(fOwnerX, fOwnerY);

	float fx, fy;

	auto fDistToGo = fDist - fMinDistance;
	GetDeltaByDegree(m_pkChar->GetRotation(), fDistToGo, &fx, &fy);

	if (!m_pkChar->Goto(static_cast<int32_t>(fPetX + fx + 0.5f), static_cast<int32_t>(fPetY + fy + 0.5f)))
	{
		return false;
	}

	m_pkChar->SendMovePacket(FUNC_WAIT, 0, 0, 0, 0, 0);

	return true;
}

void CMountActor::SetSummonItem(LPITEM pItem)
{
	if (!pItem)
	{
		m_dwSummonItemVID = 0;
		m_dwSummonItemVnum = 0;
		return;
	}

	m_dwSummonItemVID = pItem->GetVID();
	m_dwSummonItemVnum = pItem->GetVnum();
}

CMountSystem::CMountSystem(LPCHARACTER owner): m_pkOwner(owner), m_dwUpdatePeriod(400), m_dwLastUpdateTime(0) {}

CMountSystem::~CMountSystem()
{
	Destroy();
}

void CMountSystem::Destroy()
{
	for (auto& mountPair : m_mountActorMap)
	{
		delete mountPair.second;
	}
	
	event_cancel(&m_pkMountSystemUpdateEvent);
	m_mountActorMap.clear();
}

bool CMountSystem::Update(DWORD deltaTime)
{
	bool bResult = true;
	DWORD currentTime = get_dword_time();

	if (m_dwUpdatePeriod > currentTime - m_dwLastUpdateTime)
	{
		return true;
	}
	
	std::vector<CMountActor*> garbageActors;

	for (auto& mountPair : m_mountActorMap)
	{
		CMountActor* mountActor = mountPair.second;
		
		if (mountActor && mountActor->IsSummoned())
		{
			LPCHARACTER pMount = mountActor->GetCharacter();
			
			if (!CHARACTER_MANAGER::instance().Find(pMount->GetVID()))
			{
				garbageActors.push_back(mountActor);
			}
			else
			{
				bResult &= mountActor->Update(deltaTime);
			}
		}
	}
	
	for (auto* mountActor : garbageActors)
	{
		DeleteMount(mountActor);
	}

	m_dwLastUpdateTime = currentTime;
	return bResult;
}

void CMountSystem::DeleteMount(DWORD mobVnum)
{
	auto iter = m_mountActorMap.find(mobVnum);

	if (m_mountActorMap.end() == iter)
	{
		sys_err("[CMountSystem::DeleteMount] Cannot find mount in list. Vnum: %d", mobVnum);
		return;
	}

	CMountActor* mountActor = iter->second;

	if (!mountActor)
	{
		sys_err("[CMountSystem::DeleteMount] Null Pointer (mountActor)");
	}
	else
	{
		delete mountActor;
	}

	m_mountActorMap.erase(iter);
}

void CMountSystem::DeleteMount(CMountActor* mountActor)
{
	auto iter = std::find_if(m_mountActorMap.begin(), m_mountActorMap.end(), [mountActor](const auto& pair) {return pair.second == mountActor;});	
	
	if (iter != m_mountActorMap.end())
	{
		delete iter->second;
		m_mountActorMap.erase(iter);
	}
	else
	{
		sys_err("[CMountSystem::DeleteMount] Mount actor not found. Address: 0x%x", mountActor);

	}
}

void CMountSystem::Unsummon(DWORD vnum, bool bDeleteFromList)
{
	CMountActor* actor = this->GetByVnum(vnum);

	if (!actor)
	{
		sys_err("[CMountSystem::Unsummon] Null pointer for mount actor. Vnum: %d", vnum);
		return;
	}
	
	actor->Unsummon();

	if (bDeleteFromList)
	{
		this->DeleteMount(actor);
	}

	if (std::none_of(m_mountActorMap.begin(), m_mountActorMap.end(), [](const auto& pair) { return pair.second->IsSummoned(); }))
	{
		event_cancel(&m_pkMountSystemUpdateEvent);
		m_pkMountSystemUpdateEvent = nullptr;
	}
}

void CMountSystem::Summon(DWORD mobVnum, LPITEM pSummonItem, bool bSpawnFar)
{
	CMountActor* mountActor = this->GetByVnum(mobVnum);
	
	if (!mountActor)
	{
		mountActor = new CMountActor(m_pkOwner, mobVnum);
		m_mountActorMap.emplace(mobVnum, mountActor);
	}

	DWORD mountVID = mountActor->Summon(pSummonItem, bSpawnFar);

	if (!mountVID)
	{
		sys_err("[CMountSystem::Summon] Failed to summon mount. Vnum: %d", pSummonItem->GetID());

	}

	if (!m_pkMountSystemUpdateEvent)
	{
		auto* info = AllocEventInfo<mountsystem_event_info>();
		info->pMountSystem = this;
		m_pkMountSystemUpdateEvent = event_create(mountsystem_update_event, info, PASSES_PER_SEC(1) / 4);
	}
}

void CMountSystem::Mount(DWORD mobVnum, LPITEM mountItem)
{
	CMountActor* mountActor = this->GetByVnum(mobVnum);

	if (!mountActor)
	{
		sys_err("[CMountSystem::Mount] Null Pointer (mountActor)");
		return;
	}
	
	if(!mountItem)
	{
		return;
	}

	this->Unsummon(mobVnum, false);
	mountActor->Mount(mountItem);
}

void CMountSystem::Unmount(DWORD mobVnum)
{
	CMountActor* mountActor = this->GetByVnum(mobVnum);

	if (!mountActor)
	{
		sys_err("[CMountSystem::Mount] Null Pointer (mountActor)");
		return;
	}

	if (auto pSummonItem = m_pkOwner->GetWear(WEAR_COSTUME_MOUNT))
	{
		this->Summon(mobVnum, pSummonItem, false);
	}
	
	mountActor->Unmount();
}

CMountActor* CMountSystem::GetByVID(DWORD vid) const
{
	auto iter = std::find_if(m_mountActorMap.begin(), m_mountActorMap.end(), [vid](const auto& pair) {return pair.second && pair.second->GetVID() == vid;});
	return (iter != m_mountActorMap.end()) ? iter->second : nullptr;
}

CMountActor* CMountSystem::GetByVnum(DWORD vnum) const
{
	auto iter = m_mountActorMap.find(vnum);
	return (iter != m_mountActorMap.end()) ? iter->second : nullptr;
}

size_t CMountSystem::CountSummoned() const
{
	return std::count_if(m_mountActorMap.begin(), m_mountActorMap.end(), [](const auto& pair) {return pair.second && pair.second->IsSummoned();});
}