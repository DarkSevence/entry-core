#ifndef __INC_MESSENGER_MANAGER_H
#define __INC_MESSENGER_MANAGER_H

#include "db.h"

class MessengerManager : public singleton<MessengerManager>
{
	public:
		using AccountKey = std::string;
		using accountKeyReference = const std::string&;

		MessengerManager() = default;
		virtual ~MessengerManager() = default;

	public:
		void	P2PLogin(accountKeyReference account);
		void	P2PLogout(accountKeyReference account);

		void	Login(accountKeyReference account);
		void	Logout(accountKeyReference account);

		void	RequestToAdd(LPCHARACTER ch, LPCHARACTER target);
		bool AuthToAdd(accountKeyReference account, accountKeyReference companion, bool bDeny);

		void	__AddToList(accountKeyReference account, accountKeyReference companion);	// 실제 m_Relation, m_InverseRelation 수정하는 메소드
		void	AddToList(accountKeyReference account, accountKeyReference companion);

		void	__RemoveFromList(accountKeyReference account, accountKeyReference companion); // 실제 m_Relation, m_InverseRelation 수정하는 메소드
		void	RemoveFromList(accountKeyReference account, accountKeyReference companion);	

		void	RemoveAllList(accountKeyReference account);
			void Initialize();


	private:
		void	SendList(accountKeyReference account);
		void	SendLogin(accountKeyReference account, accountKeyReference companion);
		void	SendLogout(accountKeyReference account, accountKeyReference companion);
		bool IsAlreadyFriends(accountKeyReference account, accountKeyReference companion);

		void	LoadList(SQLMsg * pmsg);
		void Destroy();

		std::set<AccountKey> m_set_loginAccount;
		std::map<AccountKey, std::set<AccountKey>> m_Relation;
		std::map<AccountKey, std::set<AccountKey>>	m_InverseRelation;
		std::set<DWORD>			m_set_requestToAdd;
};

#endif
