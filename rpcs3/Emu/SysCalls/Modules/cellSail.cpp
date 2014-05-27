#include "stdafx.h"
#include "cellSail.h"
#include "Emu/SysCalls/SysCalls.h"
#include "Emu/SysCalls/SC_FUNC.h"
#include "Emu/Cell/SAILManager.h"

void cellSail_init();
Module cellSail(0x001d, cellSail_init);

int cellSailMemAllocatorInitialize(mem_ptr_t<CellSailMemAllocator> pSelf, const mem_ptr_t<CellSailMemAllocatorFuncs> pCallbacks, mem32_t pArgs)
{
	cellSail.Warning("cellSailMemAllocatorInitialize(memAlloc_addr=0x%x, callbacks_addr=0x%x, args_addr=0x%x)",
		pSelf.GetAddr(), pCallbacks.GetAddr(), pArgs.GetAddr());
	//k this one seems easy, we just move all the junk into the memallocator
	//TODO: error handling?
	pSelf->callbacks.pAlloc.SetAddr(pCallbacks->pAlloc.GetAddr());
	pSelf->callbacks.pFree.SetAddr(pCallbacks->pFree.GetAddr());
	pSelf->pArg = pArgs.GetAddr();
	return CELL_OK;
}

int cellSailPlayerInitialize2(mem_ptr_t<CellSailPlayer> pPlayer, mem_ptr_t<CellSailMemAllocator> pAllocator, mem_func_ptr_t<CellSailPlayerFuncNotified> pCallback,
	u64 playerCallbackArg, mem_ptr_t<CellSailPlayerAttribute> pAttribute, mem_ptr_t<CellSailPlayerResource> pResource)
{
	cellSail.Warning("cellSailPlayerInitialize2(sailPlayer=0x%x, memAllocator=0x%x, callBack=0x%x, playerCallBackArg=%u, playerAttr=0x%x, playerRes=0x%x)",
		pPlayer.GetAddr(), pAllocator.GetAddr(), pCallback.GetAddr(), playerCallbackArg, pAttribute.GetAddr(), pResource.GetAddr());
	//now things get interesting
	//we have to initialize SailPlayer, plus side is its all internal, so lets use it for internal things!
	//shove all the things into pPlayer!
	//TODO: error handling?
	pPlayer->pMemAllocArgs = pAllocator->pArg; //k do we take this or the callbacks one?
	pPlayer->pMemAllocFuncAlloc.SetAddr(pAllocator->callbacks.pAlloc.GetAddr());
	pPlayer->pMemAllocFuncFree.SetAddr(pAllocator->callbacks.pFree.GetAddr());
	pPlayer->playerCallbackArg = playerCallbackArg;

	CellSailPlayer* temp = (CellSailPlayer*)pPlayer.GetAddr();
	Memory.Copy((u32)(&temp->playerAttributes), pAttribute.GetAddr(), sizeof(CellSailPlayerAttribute));

	if (pResource.IsGood())
	{
		pPlayer->playerResource.pSpurs = pResource->pSpurs;
	}
	pPlayer->playerFuncNotified.SetAddr(pCallback.GetAddr());
	pPlayer->subscribedEvents = 0xffffffff; //default, subscribed to all

	//allocate space for specified number of adapters
	//these might have to be changed to use the memalloc callback, not sure though
	MemoryAllocator<CellSailSoundAdapter*> soundadapters(sizeof(CellSailSoundAdapter)* pAttribute->maxAudioStreamNum);
	MemoryAllocator<CellSailGraphicsAdapter*> graphicsAdapters(sizeof(CellSailGraphicsAdapter)* pAttribute->maxVideoStreamNum);
	pPlayer->soundAdapters.SetAddr((u32)soundadapters.GetPtr());
	pPlayer->graphicsAdapters.SetAddr((u32)graphicsAdapters.GetPtr());

	//set out a change state callback
	MemoryAllocator<CellSailEvent> sailEvent;
	sailEvent->major = CELL_SAIL_EVENT_PLAYER_STATE_CHANGED;
	sailEvent->minor = CELL_SAIL_PLAYER_CALL_NONE;
	mem_func_ptr_t<CellSailPlayerFuncNotified> playerFuncCallback(pPlayer->playerFuncNotified.GetAddr());

	ConLog.Warning("Calling CellSailPlayerFuncNotified - PLAYER_STATE_CHANGED, INITIALIZED");
	playerFuncCallback.SetAddr(pPlayer->playerFuncNotified.GetAddr());
	playerFuncCallback.async(pPlayer->playerCallbackArg.ToLE(), sailEvent.GetAddr(), CELL_SAIL_PLAYER_STATE_INITIALIZED, 0);

	return CELL_OK;
}

int cellSailPlayerSubscribeEvent(mem_ptr_t<CellSailPlayer> pPlayer, s32 eventType, u64 reserved)
{
	cellSail.Warning("cellSailPlayerSubscribeEvent(sailPlayer=0x%x, eventType=%u)", pPlayer.GetAddr(), eventType);
	//TODO: error handling?
	//converting eventType to a mask bit
	if (eventType > 0) //eventType 0 shouldnt ever be used
	{
		u32 eventMask = 1 << (eventType - 1);
		pPlayer->subscribedEvents |= eventMask;
	}
	return CELL_OK;
}

int cellSailPlayerUnsubscribeEvent(mem_ptr_t<CellSailPlayer> pPlayer, s32 eventType)
{
	cellSail.Warning("cellSailPlayerUnsubscribeEvent(sailPlayer=0x%x, eventType=%d)", pPlayer.GetAddr(), eventType);
	//TODO: error handling?
	//converting eventType to a mask bit
	if (eventType > 0)
	{
		u32 eventMask = 1 << (eventType - 1);
		pPlayer->subscribedEvents &= ~eventMask;
	}
	return CELL_OK;
}

int cellSailSoundAdapterInitialize(mem_ptr_t<CellSailSoundAdapter> pSoundAdapter, const mem_ptr_t<CellSailSoundAdapterFuncs> pCallbacks, mem32_t pArgs)
{
	cellSail.Warning("cellSailSoundAdapterInitialize(sailSoundAdapter=0x%x, soundCallbackFuncs=0x%x, soundArgs=0x%x)", pSoundAdapter.GetAddr(), pCallbacks.GetAddr(), pArgs.GetAddr());
	//TODO: error handling?
	pSoundAdapter->soundAdapterFuncsArgs = pArgs.GetAddr();
	pSoundAdapter->soundAdapterFuncs.pMakeup = pCallbacks->pMakeup;
	pSoundAdapter->soundAdapterFuncs.pCleanup = pCallbacks->pCleanup;
	pSoundAdapter->soundAdapterFuncs.pFormatChanged = pCallbacks->pFormatChanged;
	//set defaults for 'perferred audio format', -1 is no preference
	pSoundAdapter->audioFormat.chLayout = CELL_SAIL_AUDIO_CH_LAYOUT_UNSPECIFIED;
	pSoundAdapter->audioFormat.chNum = CELL_SAIL_AUDIO_CHNUM_UNSPECIFIED;
	pSoundAdapter->audioFormat.coding = CELL_SAIL_AUDIO_CODING_UNSPECIFIED;
	pSoundAdapter->audioFormat.fs = CELL_SAIL_AUDIO_FS_UNSPECIFIED;
	pSoundAdapter->audioFormat.sampleNum = CELL_SAIL_AUDIO_SAMPLE_NUM_UNSPECIFIED;
	pSoundAdapter->audioFormat.reserved0 = -1;
	pSoundAdapter->audioFormat.reserved1 = -1;
	return CELL_OK;
}

int cellSailSourceInitialize(mem_ptr_t<CellSailSource> pSource, const mem_ptr_t<CellSailSourceFuncs> pFuncs, mem32_t pArgs)
{
	cellSail.Warning("cellSailSourceInitialize(sailSource=0x%x, sourceFuncs=0x%x, sourceArgs=0x%x)", pSource.GetAddr(), pFuncs.GetAddr(), pArgs.GetAddr());
	//TODO: error handling?
	pSource->pSourceFuncsArgs = pArgs.GetAddr();
	//Memory.WriteData((u32)&pSource->sourceFuncs, pFuncs);
	return CELL_OK;
}


int cellSailSoundAdapterSetPreferredFormat(mem_ptr_t<CellSailSoundAdapter> pSoundAdapter, const mem_ptr_t<CellSailAudioFormat> pFormat)
{
	cellSail.Warning("cellSailSoundAdapterSetPreferredFormat(soundAdapter=0x%x, audioFormat=0x%x)", pSoundAdapter.GetAddr(), pFormat.GetAddr());
	//TODO: error handling?

	CellSailSoundAdapter* tempSoundAdapter = (CellSailSoundAdapter*)pSoundAdapter.GetAddr();
	Memory.Copy((u32)(&tempSoundAdapter->audioFormat), pFormat.GetAddr(), sizeof(CellSailAudioFormat));

	return CELL_OK;
}


int cellSailPlayerSetSoundAdapter(mem_ptr_t<CellSailPlayer> pPlayer, s32 index, mem_ptr_t<CellSailSoundAdapter> pAdapter)
{
	cellSail.Warning("cellSailPlayerSetSoundAdapter(sailPlayer=0x%x, index=%d, soundAdapter=0x%x)", pPlayer.GetAddr(), index, pAdapter.GetAddr());
	//TODO: error handling?

	CellSailPlayer* temp = (CellSailPlayer*)pPlayer.GetAddr();
	CellSailSoundAdapter* temp2 = (CellSailSoundAdapter*)&temp->soundAdapters;
	Memory.Copy((u32)(&temp2[index]), pAdapter.GetAddr(), sizeof(CellSailSoundAdapter));
	return CELL_OK;
}

int cellSailGraphicsAdapterInitialize(mem_ptr_t<CellSailGraphicsAdapter> pGraphicsAdapter, const mem_ptr_t<CellSailGraphicsAdapterFuncs> pCallbacks, mem32_t pArgs)
{
	cellSail.Warning("cellSailGraphicsAdapterInitialize(graphicsAdapter=0x%x, graphicsAdapterCallbacks=0x%x, graphicsArgs=0x%x)",
		pGraphicsAdapter.GetAddr(), pCallbacks.GetAddr(), pArgs.GetAddr());

	//TODO: error handling?
	pGraphicsAdapter->graphicsAdapterFuncsArgs = pArgs.GetAddr();
	CellSailGraphicsAdapter* temp = (CellSailGraphicsAdapter*)pGraphicsAdapter.GetAddr();
	Memory.Copy((u32)(&temp->graphicsAdapterFuncs), pCallbacks.GetAddr(), sizeof(CellSailGraphicsAdapterFuncs));
	//set default videoformat to -1, aka unspecified;
	pGraphicsAdapter->videoFormat.coding = -1;
	pGraphicsAdapter->videoFormat.scan = -1;
	pGraphicsAdapter->videoFormat.bitsPerColor = -1;
	pGraphicsAdapter->videoFormat.frameRate = -1;
	pGraphicsAdapter->videoFormat.width = -1;
	pGraphicsAdapter->videoFormat.height = -1;
	pGraphicsAdapter->videoFormat.pitch = -1;
	pGraphicsAdapter->videoFormat.alpha = -1;
	pGraphicsAdapter->videoFormat.colorMatrix = -1;
	pGraphicsAdapter->videoFormat.aspectRatio = -1;
	pGraphicsAdapter->videoFormat.colorRange = -1;
	pGraphicsAdapter->videoFormat.reserved1 = -1;
	pGraphicsAdapter->videoFormat.reserved2 = -1;
	pGraphicsAdapter->videoFormat.reserved3 = -1;
	return CELL_OK;
}

int cellSailGraphicsAdapterSetPreferredFormat(mem_ptr_t<CellSailGraphicsAdapter> pGraphicsAdapter, const mem_ptr_t<CellSailVideoFormat> pFormat)
{
	cellSail.Warning("cellSailGraphicsAdapterSetPreferredFormat(graphicsAdapter=0x%x, videoFormat=0x%x)", pGraphicsAdapter.GetAddr(), pFormat.GetAddr());
	//TODO: error handling?
	CellSailGraphicsAdapter* temp = (CellSailGraphicsAdapter*)pGraphicsAdapter.GetAddr();
	Memory.Copy((u32)(&temp->videoFormat), pFormat.GetAddr(), sizeof(CellSailVideoFormat));
	return CELL_OK;
}


int cellSailPlayerSetGraphicsAdapter(mem_ptr_t<CellSailPlayer> pSailPlayer, s32 index, mem_ptr_t<CellSailGraphicsAdapter> pAdapter)
{
	cellSail.Warning("cellSailPlayerSetGraphicsAdapter(sailPlayer=0x%x, index=%d, graphicsAdapter=0x%x)", pSailPlayer.GetAddr(), index, pAdapter.GetAddr());
	//TODO: error handling?

	CellSailPlayer* temp = (CellSailPlayer*)pSailPlayer.GetAddr();
	CellSailGraphicsAdapter* temp2 = (CellSailGraphicsAdapter*)&temp->graphicsAdapters;
	Memory.Copy((u32)(&temp2[index]), pAdapter.GetAddr(), sizeof(CellSailGraphicsAdapter));
	return CELL_OK;
}

int cellSailPlayerSetParameter(mem_ptr_t<CellSailPlayer> pSailPlayer, s32 paramType, u64 param0, u64 param1)
{
	cellSail.Log("cellSailPlayerSetParameter(sailPlayer=0x%x, paramType=%d, param0=%u, param1=%u)", pSailPlayer.GetAddr(), paramType, param0, param1);
	//its helpful to see what games want to set, but im not sure what to do with any of it yet, soo, unimplemented!
	//looks like most are for thread/spurs priority and performance, so possibly unimportant for now
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailPlayerGetParameter()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailPlayerBoot(mem_ptr_t<CellSailPlayer> pSailPlayer, u64 execCompleteArg)
{
	cellSail.Warning("cellSailPlayerBoot(sailPlayer=0x%x, completionArg=%u)", pSailPlayer.GetAddr(), execCompleteArg);
	

	//hold up, lets try to do this all with async callbacks, let ffmpeg or some other library deal with playing it
	//SAILManager sailManager = SAILManager(pSailPlayer.GetAddr());

	//set out a change state callback
	MemoryAllocator<CellSailEvent> sailEvent;
	sailEvent->major = CELL_SAIL_EVENT_PLAYER_STATE_CHANGED;
	sailEvent->minor = CELL_SAIL_PLAYER_CALL_NONE;
	mem_func_ptr_t<CellSailPlayerFuncNotified> playerFuncCallback(pSailPlayer->playerFuncNotified.GetAddr());

	ConLog.Warning("Calling CellSailPlayerFuncNotified - PLAYER_STATE_CHANGED, BOOT_TRANSITION");
	playerFuncCallback.SetAddr(pSailPlayer->playerFuncNotified.GetAddr());
	playerFuncCallback.async(pSailPlayer->playerCallbackArg.ToLE(), sailEvent.GetAddr(), CELL_SAIL_PLAYER_STATE_BOOT_TRANSITION, 0);

	//umm, i guess lets just take this arg in and call us booted
	pSailPlayer->funcExecCompleteArg = execCompleteArg;

	MemoryAllocator<CellSailEvent> sailEvent2;
	sailEvent2->major = CELL_SAIL_EVENT_PLAYER_CALL_COMPLETED;
	sailEvent2->minor = CELL_SAIL_PLAYER_CALL_BOOT;
	mem_func_ptr_t<CellSailPlayerFuncNotified> playerFuncCallback2(pSailPlayer->playerFuncNotified.GetAddr());
	ConLog.Warning("Calling CellSailPlayerFuncNotified - PLAYER_CALL_BOOT");
	playerFuncCallback2.SetAddr(pSailPlayer->playerFuncNotified.GetAddr());
	playerFuncCallback2.async(pSailPlayer->playerCallbackArg.ToLE(), sailEvent2.GetAddr(), 0, pSailPlayer->funcExecCompleteArg.ToLE());

	return CELL_OK;
}

int cellSailPlayerRegisterSource(mem_ptr_t<CellSailPlayer> pPlayer, const mem8_ptr_t pProtocolName, mem_ptr_t<CellSailSource> pSource)
{
	UNIMPLEMENTED_FUNC(cellSail);
	//cellSail.Warning("cellSailPlayerRegisterSource(sailPlayer=0x%x, protocolName=%s, sailSource=0x%x)", pPlayer.GetAddr(), pProtocolName.GetString(), pSource.GetAddr());
	return CELL_OK;
}

int cellSailFutureInitialize(mem_ptr_t<CellSailFuture> pSelf)
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailFutureFinalize(mem_ptr_t<CellSailFuture> pSelf)
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailFutureReset(mem_ptr_t<CellSailFuture> pSelf, bool wait)
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailFutureSet(mem_ptr_t<CellSailFuture> pSelf, s32 result)
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailFutureGet(mem_ptr_t<CellSailFuture> pSelf, u64 timeout, mem_ptr_t<s32> pResult)
{
	//timeout is in microseconds
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailFutureIsDone(mem_ptr_t<CellSailFuture> pSelf, mem_ptr_t<s32> pResult)
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailDescriptorGetStreamType(mem_ptr_t<CellSailDescriptor> pSelf)
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailDescriptorGetUri(mem_ptr_t<CellSailDescriptor> pSelf)
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailDescriptorGetMediaInfo(mem_ptr_t<CellSailDescriptor> pSelf)
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailDescriptorSetAutoSelection(mem_ptr_t<CellSailDescriptor> pSelf, bool autoSelection)
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailDescriptorIsAutoSelection(mem_ptr_t<CellSailDescriptor> pSelf)
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailDescriptorCreateDatabase()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailDescriptorDestroyDatabase()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailDescriptorOpen()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailDescriptorClose()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailDescriptorSetEs()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailDescriptorClearEs()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailDescriptorGetCapabilities()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailDescriptorInquireCapability()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailDescriptorSetParameter()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailSoundAdapterFinalize()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailSoundAdapterGetFrame()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailSoundAdapterGetFormat()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailSoundAdapterUpdateAvSync()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailSoundAdapterPtsToTimePosition()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailGraphicsAdapterFinalize()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailGraphicsAdapterGetFrame()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailGraphicsAdapterGetFrame2()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailGraphicsAdapterGetFormat()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailGraphicsAdapterUpdateAvSync()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailGraphicsAdapterPtsToTimePosition()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailAuReceiverInitialize()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailAuReceiverFinalize()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailAuReceiverGet()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailRendererAudioInitialize()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailRendererAudioFinalize()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailRendererAudioNotifyCallCompleted()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailRendererAudioNotifyFrameDone()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailRendererAudioNotifyOutputEos()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailRendererVideoInitialize()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailRendererVideoFinalize()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailRendererVideoNotifyCallCompleted()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailRendererVideoNotifyFrameDone()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailRendererVideoNotifyOutputEos()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailSourceFinalize()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailSourceNotifyCallCompleted()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailSourceNotifyInputEos()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailSourceNotifyStreamOut()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailSourceNotifySessionError()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailSourceNotifyMediaStateChanged()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailSourceCheck()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailSourceNotifyOpenCompleted()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailSourceNotifyStartCompleted()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailSourceNotifyStopCompleted()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailSourceNotifyReadCompleted()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailSourceSetDiagHandler()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailSourceNotifyCloseCompleted()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailMp4MovieGetBrand()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailMp4MovieIsCompatibleBrand()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailMp4MovieGetMovieInfo()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailMp4MovieGetTrackByIndex()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailMp4MovieGetTrackById()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailMp4MovieGetTrackByTypeAndIndex()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailMp4TrackGetTrackInfo()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailMp4TrackGetTrackReferenceCount()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailMp4TrackGetTrackReference()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailMp4ConvertTimeScale()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailAviMovieGetMovieInfo()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailAviMovieGetStreamByIndex()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailAviMovieGetStreamByTypeAndIndex()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailAviMovieGetHeader()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailAviStreamGetMediaType()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailAviStreamGetHeader()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailPlayerInitialize()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailPlayerFinalize()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailPlayerGetRegisteredProtocols()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailPlayerSetAuReceiver()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailPlayerSetRendererAudio()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailPlayerSetRendererVideo()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailPlayerReplaceEventHandler()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailPlayerCreateDescriptor()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailPlayerDestroyDescriptor()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailPlayerAddDescriptor()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailPlayerRemoveDescriptor()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailPlayerGetDescriptorCount()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailPlayerGetCurrentDescriptor()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailPlayerOpenStream()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailPlayerCloseStream()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailPlayerOpenEsAudio()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailPlayerOpenEsVideo()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailPlayerOpenEsUser()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailPlayerReopenEsAudio()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailPlayerReopenEsVideo()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailPlayerReopenEsUser()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailPlayerCloseEsAudio()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailPlayerCloseEsVideo()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailPlayerCloseEsUser()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailPlayerStart()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailPlayerStop()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailPlayerNext()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailPlayerCancel()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailPlayerSetPaused()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailPlayerIsPaused()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailPlayerSetRepeatMode()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailPlayerGetRepeatMode()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailPlayerSetEsAudioMuted()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailPlayerSetEsVideoMuted()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailPlayerIsEsAudioMuted()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailPlayerIsEsVideoMuted()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailPlayerDumpImage()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

int cellSailPlayerUnregisterSource()
{
	UNIMPLEMENTED_FUNC(cellSail);
	return CELL_OK;
}

void cellSail_init()
{
	cellSail.AddFunc(0x346ebba3, cellSailMemAllocatorInitialize);

	cellSail.AddFunc(0x4cc54f8e, cellSailFutureInitialize);
	cellSail.AddFunc(0x9553af65, cellSailFutureFinalize);
	cellSail.AddFunc(0x0c4cb439, cellSailFutureReset);
	cellSail.AddFunc(0xa37fed15, cellSailFutureSet);
	cellSail.AddFunc(0x3a2d806c, cellSailFutureGet);
	cellSail.AddFunc(0x51ecf361, cellSailFutureIsDone);

	cellSail.AddFunc(0xd5f9a15b, cellSailDescriptorGetStreamType);
	cellSail.AddFunc(0x4c191088, cellSailDescriptorGetUri);
	cellSail.AddFunc(0xbd1635f4, cellSailDescriptorGetMediaInfo);
	cellSail.AddFunc(0x76b1a425, cellSailDescriptorSetAutoSelection);
	cellSail.AddFunc(0x277adf21, cellSailDescriptorIsAutoSelection);
	cellSail.AddFunc(0x0abb318b, cellSailDescriptorCreateDatabase);
	cellSail.AddFunc(0x28336e89, cellSailDescriptorDestroyDatabase);
	cellSail.AddFunc(0xc044fab1, cellSailDescriptorOpen);
	cellSail.AddFunc(0x15fd6a2a, cellSailDescriptorClose);
	cellSail.AddFunc(0x0d0c2f0c, cellSailDescriptorSetEs);
	cellSail.AddFunc(0xdf5553ef, cellSailDescriptorClearEs);
	cellSail.AddFunc(0xac9c3b1f, cellSailDescriptorGetCapabilities);
	cellSail.AddFunc(0x92590d52, cellSailDescriptorInquireCapability);
	cellSail.AddFunc(0xee94b99b, cellSailDescriptorSetParameter);

	cellSail.AddFunc(0x3d0d3b72, cellSailSoundAdapterInitialize);
	cellSail.AddFunc(0xd1462438, cellSailSoundAdapterFinalize);
	cellSail.AddFunc(0x1c9d5e5a, cellSailSoundAdapterSetPreferredFormat);
	cellSail.AddFunc(0x7eb8d6b5, cellSailSoundAdapterGetFrame);
	cellSail.AddFunc(0xf25f197d, cellSailSoundAdapterGetFormat);
	cellSail.AddFunc(0xeec22809, cellSailSoundAdapterUpdateAvSync);
	cellSail.AddFunc(0x4ae979df, cellSailSoundAdapterPtsToTimePosition);

	cellSail.AddFunc(0x1c983864, cellSailGraphicsAdapterInitialize);
	cellSail.AddFunc(0x76488bb1, cellSailGraphicsAdapterFinalize);
	cellSail.AddFunc(0x2e3ccb5e, cellSailGraphicsAdapterSetPreferredFormat);
	cellSail.AddFunc(0x0247c69e, cellSailGraphicsAdapterGetFrame);
	cellSail.AddFunc(0x018281a8, cellSailGraphicsAdapterGetFrame2);
	cellSail.AddFunc(0xffd58aa4, cellSailGraphicsAdapterGetFormat);
	cellSail.AddFunc(0x44a20e79, cellSailGraphicsAdapterUpdateAvSync);
	cellSail.AddFunc(0x1872331b, cellSailGraphicsAdapterPtsToTimePosition);

	cellSail.AddFunc(0x3dd9639a, cellSailAuReceiverInitialize);
	cellSail.AddFunc(0xed58e3ec, cellSailAuReceiverFinalize);
	cellSail.AddFunc(0x3a1132ed, cellSailAuReceiverGet);

	cellSail.AddFunc(0x67b4d01f, cellSailRendererAudioInitialize);
	cellSail.AddFunc(0x06dd4174, cellSailRendererAudioFinalize);
	cellSail.AddFunc(0xb7b4ecee, cellSailRendererAudioNotifyCallCompleted);
	cellSail.AddFunc(0xf841a537, cellSailRendererAudioNotifyFrameDone);
	cellSail.AddFunc(0x325039b9, cellSailRendererAudioNotifyOutputEos);

	cellSail.AddFunc(0x8d1ff475, cellSailRendererVideoInitialize);
	cellSail.AddFunc(0x47055fea, cellSailRendererVideoFinalize);
	cellSail.AddFunc(0x954f48f8, cellSailRendererVideoNotifyCallCompleted);
	cellSail.AddFunc(0x5f77e8df, cellSailRendererVideoNotifyFrameDone);
	cellSail.AddFunc(0xdff1cda2, cellSailRendererVideoNotifyOutputEos);

	cellSail.AddFunc(0x9d30bdce, cellSailSourceInitialize);
	cellSail.AddFunc(0xee724c99, cellSailSourceFinalize);
	cellSail.AddFunc(0x764ec2d2, cellSailSourceNotifyCallCompleted);
	cellSail.AddFunc(0x54c53688, cellSailSourceNotifyInputEos);
	cellSail.AddFunc(0x95ee1695, cellSailSourceNotifyStreamOut);
	cellSail.AddFunc(0xf289f0cd, cellSailSourceNotifySessionError);
	cellSail.AddFunc(0xf4009a94, cellSailSourceNotifyMediaStateChanged);
	//cellSail.AddFunc(, cellSailSourceCheck);
	cellSail.AddFunc(0x3df98d41, cellSailSourceNotifyOpenCompleted);
	cellSail.AddFunc(0x640c7278, cellSailSourceNotifyStartCompleted);
	cellSail.AddFunc(0x7473970a, cellSailSourceNotifyStopCompleted);
	cellSail.AddFunc(0x946ecca0, cellSailSourceNotifyReadCompleted);
	cellSail.AddFunc(0xbdb2251a, cellSailSourceSetDiagHandler);
	cellSail.AddFunc(0xc457b203, cellSailSourceNotifyCloseCompleted);

	cellSail.AddFunc(0xb980b76e, cellSailMp4MovieGetBrand);
	cellSail.AddFunc(0xd4049de0, cellSailMp4MovieIsCompatibleBrand);
	cellSail.AddFunc(0x5783a454, cellSailMp4MovieGetMovieInfo);
	cellSail.AddFunc(0x5faf802b, cellSailMp4MovieGetTrackByIndex);
	cellSail.AddFunc(0x85b07126, cellSailMp4MovieGetTrackById);
	cellSail.AddFunc(0xc2d90ec9, cellSailMp4MovieGetTrackByTypeAndIndex);
	cellSail.AddFunc(0xa48be428, cellSailMp4TrackGetTrackInfo);
	cellSail.AddFunc(0x72236ec1, cellSailMp4TrackGetTrackReferenceCount);
	cellSail.AddFunc(0x5f44f64f, cellSailMp4TrackGetTrackReference);
	//cellSail.AddFunc(, cellSailMp4ConvertTimeScale);

	cellSail.AddFunc(0x6e83f5c0, cellSailAviMovieGetMovieInfo);
	cellSail.AddFunc(0x3e908c56, cellSailAviMovieGetStreamByIndex);
	cellSail.AddFunc(0xddebd2a5, cellSailAviMovieGetStreamByTypeAndIndex);
	cellSail.AddFunc(0x10298371, cellSailAviMovieGetHeader);
	cellSail.AddFunc(0xc09e2f23, cellSailAviStreamGetMediaType);
	cellSail.AddFunc(0xcc3cca60, cellSailAviStreamGetHeader);

	cellSail.AddFunc(0x17932b26, cellSailPlayerInitialize);
	cellSail.AddFunc(0x23654375, cellSailPlayerInitialize2);
	cellSail.AddFunc(0x18b4629d, cellSailPlayerFinalize);
	cellSail.AddFunc(0xbedccc74, cellSailPlayerRegisterSource);
	cellSail.AddFunc(0x186b98d3, cellSailPlayerGetRegisteredProtocols);
	cellSail.AddFunc(0x1139a206, cellSailPlayerSetSoundAdapter);
	cellSail.AddFunc(0x18bcd21b, cellSailPlayerSetGraphicsAdapter);
	cellSail.AddFunc(0xf5747e1f, cellSailPlayerSetAuReceiver);
	cellSail.AddFunc(0x92eaf6ca, cellSailPlayerSetRendererAudio);
	cellSail.AddFunc(0xecf56150, cellSailPlayerSetRendererVideo);
	cellSail.AddFunc(0x5f7c7a6f, cellSailPlayerSetParameter);
	cellSail.AddFunc(0x952269c9, cellSailPlayerGetParameter);
	cellSail.AddFunc(0x6f0b1002, cellSailPlayerSubscribeEvent);
	cellSail.AddFunc(0x69793952, cellSailPlayerUnsubscribeEvent);
	cellSail.AddFunc(0x47632810, cellSailPlayerReplaceEventHandler);
	cellSail.AddFunc(0xbdf21b0f, cellSailPlayerBoot);
	cellSail.AddFunc(0xd7938b8d, cellSailPlayerCreateDescriptor);
	cellSail.AddFunc(0xfc839bd4, cellSailPlayerDestroyDescriptor);
	cellSail.AddFunc(0x7c8dff3b, cellSailPlayerAddDescriptor);
	cellSail.AddFunc(0x9897fbd1, cellSailPlayerRemoveDescriptor);
	cellSail.AddFunc(0x752f8585, cellSailPlayerGetDescriptorCount);
	cellSail.AddFunc(0x75fca288, cellSailPlayerGetCurrentDescriptor);
	cellSail.AddFunc(0x34ecc1b9, cellSailPlayerOpenStream);
	cellSail.AddFunc(0x85beffcc, cellSailPlayerCloseStream);
	cellSail.AddFunc(0x145f9b11, cellSailPlayerOpenEsAudio);
	cellSail.AddFunc(0x477501f6, cellSailPlayerOpenEsVideo);
	cellSail.AddFunc(0xa849d0a7, cellSailPlayerOpenEsUser);
	cellSail.AddFunc(0x4fa5ad09, cellSailPlayerReopenEsAudio);
	cellSail.AddFunc(0xf60a8a69, cellSailPlayerReopenEsVideo);
	cellSail.AddFunc(0x7b6fa92e, cellSailPlayerReopenEsUser);
	cellSail.AddFunc(0xbf9b8d72, cellSailPlayerCloseEsAudio);
	cellSail.AddFunc(0x07924359, cellSailPlayerCloseEsVideo);
	cellSail.AddFunc(0xaed9d6cd, cellSailPlayerCloseEsUser);
	cellSail.AddFunc(0xe535b0d3, cellSailPlayerStart);
	cellSail.AddFunc(0xeba8d4ec, cellSailPlayerStop);
	cellSail.AddFunc(0x26563ddc, cellSailPlayerNext);
	cellSail.AddFunc(0x950d53c1, cellSailPlayerCancel);
	cellSail.AddFunc(0xd1d55a90, cellSailPlayerSetPaused);
	cellSail.AddFunc(0xaafa17b8, cellSailPlayerIsPaused);
	cellSail.AddFunc(0xfc5baf8a, cellSailPlayerSetRepeatMode);
	cellSail.AddFunc(0x38144ecf, cellSailPlayerGetRepeatMode);
	cellSail.AddFunc(0x91d287f6, cellSailPlayerSetEsAudioMuted);
	cellSail.AddFunc(0xf1446a40, cellSailPlayerSetEsVideoMuted);
	cellSail.AddFunc(0x09de25fd, cellSailPlayerIsEsAudioMuted);
	cellSail.AddFunc(0xdbe32ed4, cellSailPlayerIsEsVideoMuted);
	cellSail.AddFunc(0xcc987ba6, cellSailPlayerDumpImage);
	cellSail.AddFunc(0x025b4974, cellSailPlayerUnregisterSource);
}