#pragma once 

class CHARACTER;
class CMountActor
{
	protected:
		friend class CMountSystem;

	CMountActor(CHARACTER* owner, DWORD vnum);
	virtual ~CMountActor();

	virtual bool Update(DWORD deltaTime);
	virtual bool _UpdateFollowAI();
	
	private:
		bool Follow(float fMinDistance = 50.f);

	public:
		CHARACTER* GetCharacter() const	
		{ 
			return m_pkChar; 
		}
		
		CHARACTER* GetOwner() const 
		{
			return m_pkOwner; 
		}
		
		DWORD GetVID() const
		{ 
			return m_dwVID; 
		}
		
		DWORD GetVnum() const
		{ 
			return mountVnum; 
		}
		
		void SetName();
		bool Mount(LPITEM mountItem);
		void Unmount();
		DWORD Summon(LPITEM pSummonItem, bool bSpawnFar = false);
		void Unsummon();
		
		bool IsSummoned() const 
		{ 
			return m_pkChar != nullptr; 
		}

		void SetSummonItem (LPITEM pItem);
		
		DWORD GetSummonItemVID() const
		{ 
			return m_dwSummonItemVID;
		}
	
	private:
		DWORD mountVnum;
		DWORD m_dwVID;
		DWORD m_dwLastActionTime;
		DWORD m_dwSummonItemVID;
		DWORD m_dwSummonItemVnum;
		short m_originalMoveSpeed;
		std::string m_name;
	
		CHARACTER* m_pkChar;
		CHARACTER* m_pkOwner;
};

class CMountSystem
{
	public:
		using TMountActorMap = boost::unordered_map<DWORD, CMountActor*>;

	public:
		CMountSystem(LPCHARACTER owner);
		virtual ~CMountSystem();

		CMountActor* GetByVID(DWORD vid) const;
		CMountActor* GetByVnum(DWORD vnum) const;

		bool Update(DWORD deltaTime);
		void Destroy();

		size_t CountSummoned() const;

	public:
		void SetUpdatePeriod(DWORD ms);
		void Summon(DWORD mobVnum, LPITEM pSummonItem, bool bSpawnFar);
		void Unsummon(DWORD mobVnum, bool bDeleteFromList = false);
		void Unsummon(CMountActor* mountActor, bool bDeleteFromList = false);
		
		void Mount(DWORD mobVnum, LPITEM mountItem);
		void Unmount(DWORD mobVnum);

		void DeleteMount(DWORD mobVnum);
		void DeleteMount(CMountActor* mountActor);
		
	private:
		TMountActorMap m_mountActorMap;
		LPCHARACTER m_pkOwner;
		DWORD m_dwUpdatePeriod;
		DWORD m_dwLastUpdateTime;
		LPEVENT m_pkMountSystemUpdateEvent;
};
