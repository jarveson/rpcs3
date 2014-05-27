#include "stdafx.h"
#include "SAILManager.h"

SAILManager::SAILManager(mem_ptr_t<CellSailPlayer> player)
	: ThreadBase("SailThread")
	, m_booted(false)
{
	//TODO: this can/should be changed to use SPURS instead when thats complete..., 
	// but for now, what the game doesn't know, hopefully doesnt break / start things on fire
	m_sailPlayer = (CellSailPlayer*)Memory.VirtualToRealAddr(player.GetAddr());
	ConLog.Warning("SailThread Starting");
	ThreadBase::Start();
};

void SAILManager::Task()
{
	//this is also where it gets fun, callbacks galoure
	if (!m_booted)
		CallBoot();
}

void SAILManager::CallBoot()
{
	m_booted = true;
	CellSailPlayer* m_sailPlayer = this->m_sailPlayer;
	MemoryAllocator<CellSailEvent> sailEvent;
	sailEvent->major = CELL_SAIL_EVENT_PLAYER_CALL_COMPLETED;
	sailEvent->minor = CELL_SAIL_PLAYER_CALL_BOOT;
	mem_func_ptr_t<CellSailPlayerFuncNotified> playerFuncCallback(m_sailPlayer->playerFuncNotified.GetAddr());
	//ConLog.Warning("Calling CellSailPlayerFuncNotified - PLAYER_CALL_BOOT");
	playerFuncCallback.SetAddr(m_sailPlayer->playerFuncNotified.GetAddr());
	playerFuncCallback(m_sailPlayer->playerCallbackArg.ToLE(), sailEvent.GetAddr(), 0, m_sailPlayer->funcExecCompleteArg.ToLE());
}

void SAILManager::Close()
{
	Stop(false);
}