#pragma once

// Return Codes
enum CellSailReturnCodes
{
	CELL_SAIL_RET_OK = 0,
	CELL_SAIL_ERROR_INVALID_ARG = 0x80610701,
	CELL_SAIL_ERROR_INVALID_STATE = 0x80610702,
	CELL_SAIL_ERROR_UNSUPPORTED_STREAM = 0x80610703,
	CELL_SAIL_ERROR_INDEX_OUT_OF_RANGE = 0x80610704,
	CELL_SAIL_ERROR_EMPTY = 0x80610705,
	CELL_SAIL_ERROR_FULLED = 0x80610706,
	CELL_SAIL_ERROR_USING = 0x80610707,
	CELL_SAIL_ERROR_NOT_AVAILABLE = 0x80610708,
	CELL_SAIL_ERROR_CANCEL = 0x80610709,
	CELL_SAIL_ERROR_MEMORY = 0x806107F0,
	CELL_SAIL_ERROR_INVALID_FD = 0x806107F1,
	CELL_SAIL_ERROR_FATAL = 0x806107FF,
};

enum CellSailEventTypes
{
	CELL_SAIL_EVENT_EMPTY = 0,
	CELL_SAIL_EVENT_ERROR_OCCURED = 1,
	CELL_SAIL_EVENT_PLAYER_CALL_COMPLETED = 2,
	CELL_SAIL_EVENT_PLAYER_STATE_CHANGED = 3,
	CELL_SAIL_EVENT_STREAM_OPENED = 4,
	CELL_SAIL_EVENT_STREAM_CLOSED = 5,
	CELL_SAIL_EVENT_SESSION_STARTED = 6,
	CELL_SAIL_EVENT_PAUSE_STATE_CHANGED = 7,
	CELL_SAIL_EVENT_SOURCE_EOS = 8,
	CELL_SAIL_EVENT_ES_OPENED = 9,
	CELL_SAIL_EVENT_ES_CLOSED = 10,
	CELL_SAIL_EVENT_MEDIA_STATE_CHANGED = 11,
	_CELL_SAIL_EVENT_TYPE_NUM_OF_ELEMENTS = 12,
};

//Audioformat enums
enum CellSailAudioFormatCoding
{
	CELL_SAIL_AUDIO_CODING_UNSPECIFIED = -1,
	CELL_SAIL_AUDIO_CODING_LPCM_FLOAT32 = 1,
};

enum CellSailAudioFormatNumChans
{
	CELL_SAIL_AUDIO_CHNUM_UNSPECIFIED = -1,
};

enum CellSailAudioFormatNumSamples
{
	CELL_SAIL_AUDIO_SAMPLE_NUM_UNSPECIFIED = -1,
};

enum CellSailAudioFormatFreq
{
	CELL_SAIL_AUDIO_FS_7350HZ = 7350,
	CELL_SAIL_AUDIO_FS_8000HZ = 8000,
	CELL_SAIL_AUDIO_FS_12000HZ = 12000,
	CELL_SAIL_AUDIO_FS_11025HZ = 11025,
	CELL_SAIL_AUDIO_FS_16000HZ = 16000,
	CELL_SAIL_AUDIO_FS_22050HZ = 22050,
	CELL_SAIL_AUDIO_FS_24000HZ = 24000,
	CELL_SAIL_AUDIO_FS_32000HZ = 32000,
	CELL_SAIL_AUDIO_FS_44100HZ = 44100,
	CELL_SAIL_AUDIO_FS_48000HZ = 48000,
	CELL_SAIL_AUDIO_FS_64000HZ = 64000,
	CELL_SAIL_AUDIO_FS_88200HZ = 88200,
	CELL_SAIL_AUDIO_FS_96000HZ = 96000,
	CELL_SAIL_AUDIO_FS_192000HZ = 192000,
	CELL_SAIL_AUDIO_FS_UNSPECIFIED = -1,
};

enum CellSailAudioFormatChanLayout
{
	CELL_SAIL_AUDIO_CH_LAYOUT_UNDEFINED = 0,
	CELL_SAIL_AUDIO_CH_LAYOUT_1CH = 1,

	// 1. Front Left
	// 2. Front Right
	CELL_SAIL_AUDIO_CH_LAYOUT_2CH_LR = 2,

	// 1. Front Left
	// 2. Front Center
	// 3. Front Right
	// for m4aac ac3
	CELL_SAIL_AUDIO_CH_LAYOUT_3CH_LCR = 3,

	// 1. Front Left
	// 2. Front Center
	// 3. Surround
	// for m4aac ac3
	CELL_SAIL_AUDIO_CH_LAYOUT_3CH_LRc = 4,

	// 1. Front Left
	// 2. Front Center
	// 3. Front Right
	// 4. Surround
	// for m4aac ac3
	CELL_SAIL_AUDIO_CH_LAYOUT_4CH_LCRc = 5,

	// 1. Front Left
	// 2. Front Right
	// 3. Surround Left
	// 4. Surround Right
	// for m4aac
	CELL_SAIL_AUDIO_CH_LAYOUT_4CH_LRlr = 6,

	// 1. Front Left
	// 2. Front Center
	// 3. Front Right
	// 4. Surround Left
	// 5. Surround Right
	// for m4aac
	CELL_SAIL_AUDIO_CH_LAYOUT_5CH_LCRlr = 7,

	// 1. Front Left
	// 2. Front Center
	// 3. Front Right
	// 4. Surround Left
	// 5. Surround Right
	// 6. LFE
	// for lpcm ac3 m4aac
	CELL_SAIL_AUDIO_CH_LAYOUT_6CH_LCRlrE = 8,

	// 1. Front Left
	// 2. Front Center
	// 3. Front Right
	// 4. Back Left
	// 5. Back Right
	// 6. LFE
	// for at3plus
	CELL_SAIL_AUDIO_CH_LAYOUT_6CH_LCRxyE = 9,

	// 1. Front Left
	// 2. Front Center
	// 3. Front Right
	// 4. Back Left
	// 5. Back Right
	// 6. Back Center
	// 7. LFE
	// (for at3plus) 
	CELL_SAIL_AUDIO_CH_LAYOUT_7CH_LCRxycE = 10,

	// 1. Front Left
	// 2. Front Center
	// 3. Front Right
	// 4. LFE
	// 5. Surround Left
	// 6. Surround Right
	// 7. Back Left  (Left-Extend)
	// 8. Back Right (Right-Extend)
	// for lpcm at3plus
	CELL_SAIL_AUDIO_CH_LAYOUT_8CH_LRCElrxy = 11,
	CELL_SAIL_AUDIO_CH_LAYOUT_2CH_DUAL = 12,
	CELL_SAIL_AUDIO_CH_LAYOUT_UNSPECIFIED = -1,
};

//video format enums
enum CellSailVideoFormatEncodingType
{
	CELL_SAIL_VIDEO_CODING_UNSPECIFIED = -1,
	CELL_SAIL_VIDEO_CODING_ARGB_INTERLEAVED = 0,
	CELL_SAIL_VIDEO_CODING_RGBA_INTERLEAVED = 1,
	CELL_SAIL_VIDEO_CODING_YUV422_U_Y0_V_Y1 = 2,
	CELL_SAIL_VIDEO_CODING_YUV420_PLANAR = 3,

	// suported by libcamera
	CELL_SAIL_VIDEO_CODING_YUV422_Y0_U_Y1_V = 4,
	CELL_SAIL_VIDEO_CODING_YUV422_V_Y1_U_Y0 = 9,
	CELL_SAIL_VIDEO_CODING_YUV422_Y1_V_Y0_U = 10,
	CELL_SAIL_VIDEO_CODING_JPEG = 11,
	CELL_SAIL_VIDEO_CODING_RAW8_BAYER_BGGR = 12,
	_CELL_SAIL_VIDEO_CODING_TYPE_NUM_OF_ELEMENTS = 13,

	/*CELL_SAIL_VIDEO_CODING_UYVY422_INTERLEAVED = 2,
	CELL_SAIL_VIDEO_CODING_YUYV422_INTERLEAVED = 4,
	CELL_SAIL_VIDEO_CODING_VYUY422_REVERSE_INTERLEAVED = 9,
	CELL_SAIL_VIDEO_CODING_RAW8_BAYER_GRBG = 12,*/
};

enum CellSailVideoFormatColorMatrixType
{
	CELL_SAIL_VIDEO_COLOR_MATRIX_UNSPECIFIED = -1,
	CELL_SAIL_VIDEO_COLOR_MATRIX_BT601 = 0,
	CELL_SAIL_VIDEO_COLOR_MATRIX_BT709 = 1,

	_CELL_SAIL_VIDEO_COLOR_MATRIX_TYPE_NUM_OF_ELEMENTS = 2,
};

enum CellSailVideoFromatScanType
{
	CELL_SAIL_VIDEO_SCAN_UNSPECIFIED = -1,
	CELL_SAIL_VIDEO_SCAN_PROGRESSIVE = 0,
	CELL_SAIL_VIDEO_SCAN_INTERLACE = 1,

	_CELL_SAIL_VIDEO_SCAN_TYPE_NUM_OF_ELEMENTS = 2,
};

enum CellSailVideoFormatFrameRate
{
	CELL_SAIL_VIDEO_FRAME_RATE_UNSPECIFIED = -1,
	CELL_SAIL_VIDEO_FRAME_RATE_24000_1001HZ = 0,
	CELL_SAIL_VIDEO_FRAME_RATE_24HZ = 1,
	CELL_SAIL_VIDEO_FRAME_RATE_25HZ = 2,
	CELL_SAIL_VIDEO_FRAME_RATE_30000_1001HZ = 3,
	CELL_SAIL_VIDEO_FRAME_RATE_30HZ = 4,
	CELL_SAIL_VIDEO_FRAME_RATE_50HZ = 5,
	CELL_SAIL_VIDEO_FRAME_RATE_60000_1001HZ = 6,
	CELL_SAIL_VIDEO_FRAME_RATE_60HZ = 7,

	_CELL_SAIL_VIDEO_FRAME_RATE_TYPE_NUM_OF_ELEMENTS = 8,
};

enum CellSailVideoFormatAspectRatio
{
	CELL_SAIL_VIDEO_ASPECT_RATIO_UNSPECIFIED = -1,
	CELL_SAIL_VIDEO_ASPECT_RATIO_1_1 = 1, // 1920x1080 1280x720
	CELL_SAIL_VIDEO_ASPECT_RATIO_12_11 = 2, // 720x576 normal
	CELL_SAIL_VIDEO_ASPECT_RATIO_10_11 = 3, // 720x480 normal
	CELL_SAIL_VIDEO_ASPECT_RATIO_16_11 = 4, // 720x576 wide
	CELL_SAIL_VIDEO_ASPECT_RATIO_40_33 = 5, // 720x480 wide
	CELL_SAIL_VIDEO_ASPECT_RATIO_4_3 = 14, // 1440x1080
};

enum CellSailVideoFormatPixelInfo
{
	CELL_SAIL_VIDEO_WIDTH_UNSPECIFIED = -1,
	CELL_SAIL_VIDEO_HEIGHT_UNSPECIFIED = -1,
	CELL_SAIL_VIDEO_PITCH_UNSPECIFIED = -1,
	CELL_SAIL_VIDEO_BITS_PER_COLOR_UNSPECIFIED = -1,
	CELL_SAIL_VIDEO_ALPHA_UNSPECIFIED = -1,
};


enum CellSailVideoFromatColorRange
{
	CELL_SAIL_VIDEO_COLOR_RANGE_UNSPECIFIED = -1,
	CELL_SAIL_VIDEO_COLOR_RANGE_LIMITED = 1,
	CELL_SAIL_VIDEO_COLOR_RANGE_FULL = 0,
};

//avoid compiler error, this has to be first
struct CellSailEvent
{
	be_t<u32> major;
	be_t<u32> minor;
};

struct CellSailAudioFormat
{
	s8 coding;
	s8 chNum;
	be_t<s16> sampleNum;
	be_t<s32> fs;
	be_t<s32> chLayout;
	be_t<s32> reserved0;
	be_t<s64> reserved1;
};

struct CellSailVideoFormat
{
	s8 coding;
	s8 scan;
	s8 bitsPerColor;
	s8 frameRate;
	be_t<s16> width;
	be_t<s16> height;
	be_t<s32> pitch;
	be_t<s32> alpha;
	s8 colorMatrix;
	s8 aspectRatio;
	s8 colorRange;
	s8 reserved1;
	be_t<s32> reserved2;
	be_t<s64> reserved3;
};

struct CellSailSourceStreamingProfile
{
	be_t<u32> reserved0;
	be_t<u32> numItems;
	be_t<u32> maxBitrate;
	be_t<u32> reserved1;
	be_t<u64> duration;
	be_t<u64> streamSize;
};

struct CellSailSourceStartCommand
{
	be_t<u64> startFlags;
	be_t<s64> startArg;
	be_t<s64> lengthArg;
	be_t<u64> optionalArg0;
	be_t<u64> optionalArg1;
};

struct CellSailSourceBufferItem
{
	mem8_ptr_t pBuf;
	be_t<u32> size;
	be_t<u32> sessionId;
	be_t<u32> reserved;
};


struct CellSailAudioFrameInfo
{
	be_t<u32> pPcm;
	be_t<s32> status;
	be_t<u64> pts;
	be_t<u64> reserved;
};

struct CellSailVideoFrameInfo
{
	be_t<u32> pPic;
	be_t<s32> status;
	be_t<u64> pts;
	be_t<u64> reserved;
	be_t<u16> interval;
	u8 structure;
	s8 repeatNum;
	u8 reserved2[4];
};


// Callback Functions, christ on a bike theres alot....have to split these to avoid compiler warnings
//void* return
typedef mem32_ptr_t(*CellSailMemAllocatorFuncAlloc)(mem32_t pArg, u32 boundary, u32 size);
typedef void(*CellSailMemAllocatorFuncFree) (mem32_t pArg, u32 boundary, mem32_t pMemory);
typedef void(*CellSailPlayerFuncNotified)(mem32_t pArg, mem_ptr_t<CellSailEvent> evnt, u64 arg0, u64 arg1);
typedef void(*CellSailSoundAdapterFuncFormatChanged)(mem32_t pArg, mem_ptr_t<CellSailAudioFormat> pFormat, u32 sessionId);
typedef int(*CellSailSoundAdapterFuncMakeup)(mem32_t pArg);
typedef int(*CellSailSoundAdapterFuncCleanup)(mem32_t pArg);
typedef int(*CellSailGraphicsAdapterFuncMakeup)(mem32_t pArg);
typedef int(*CellSailGraphicsAdapterFuncCleanup)(mem32_t pArg);
typedef void(*CellSailGraphicsAdapterFuncFormatChanged)(mem32_t pArg, mem_ptr_t<CellSailVideoFormat> pFormat, u32 sessionId);
typedef int(*CellSailGraphicsAdapterFuncAllocFrame)(mem32_t pArg, u32 size, s32 num, mem8_ptr_t ppFrame); //screw u mr.double-pointer of u8's
typedef int(*CellSailGraphicsAdapterFuncFreeFrame)(mem32_t pArg, s32 num, mem8_ptr_t ppFrame);
typedef int(*CellSailRendererAudioFuncMakeup)(mem32_t pArg);
typedef int(*CellSailRendererAudioFuncCleanup)(mem32_t pArg);
typedef void(*CellSailRendererAudioFuncOpen)(mem32_t pArg, const mem_ptr_t<CellSailAudioFormat> pInfo, u32 frameNum);
typedef void(*CellSailRendererAudioFuncClose)(mem32_t pArg);
typedef void(*CellSailRendererAudioFuncStart)(mem32_t pArg, bool buffering);
typedef void(*CellSailRendererAudioFuncStop)(mem32_t pArg, bool flush);
typedef void(*CellSailRendererAudioFuncCancel)(mem32_t pArg);
typedef int(*CellSailSourceFuncMakeup)(mem32_t pArg, const mem8_ptr_t pProtocolNames);//char *'s are mem8_ptr_t
typedef int(*CellSailSourceFuncCleanup)(mem32_t pArg);
typedef void(*CellSailSourceFuncOpen)(mem32_t pArg, s32 streamType, mem32_t pMediaInfo, const mem8_ptr_t pUri, const mem_ptr_t<CellSailSourceStreamingProfile> pProfile);
typedef void(*CellSailSourceFuncClose)(mem32_t pArg);
typedef void(*CellSailSourceFuncStart)(mem32_t pArg, const mem_ptr_t<CellSailSourceStartCommand> pCommand, u32 sessionId);
typedef void(*CellSailSourceFuncStop)(mem32_t pArg);
typedef void(*CellSailSourceFuncCancel)(mem32_t pArg);
typedef int(*CellSailSourceFuncCheckout)(mem32_t pArg, const mem_list_ptr_t<CellSailSourceBufferItem> ppItem); //ugh more double-pointers
typedef int(*CellSailSourceFuncCheckin)(mem32_t pArg, const mem_ptr_t<CellSailSourceBufferItem> pItem);
typedef int(*CellSailSourceFuncClear)(mem32_t pArg);
typedef void(*CellSailSourceCheckFuncError)(mem32_t pArg, const mem8_ptr_t pMsg, s32 line);
typedef int(*CellSailFsFuncOpen)(const mem8_ptr_t pPath, s32 flag, mem_ptr_t<s32> pFd, mem32_t pArg, u64 size);
typedef int(*CellSailFsFuncOpenSecond)(const mem8_ptr_t pPath, s32 flag, s32 fd, mem32_t pArg, u64 size);
typedef int(*CellSailFsFuncClose)(s32 fd);
typedef int(*CellSailFsFuncFstat)(s32 fd, mem_ptr_t<CellFsStat> pStat);
typedef int(*CellSailFsFuncRead)(s32 fd, mem8_ptr_t pBuf, u64 numBytes, u64 pNumRead);
typedef int(*CellSailFsFuncLseek)(s32 fd, s64 offset, s32 whence, mem64_t pPosition);
typedef int(*CellSailFsFuncCancel)(s32 fd);
typedef int(*CellSailRendererAudioFuncCheckout)(mem32_t pArg, mem_list_ptr_t<CellSailAudioFrameInfo> ppInfo);
typedef int(*CellSailRendererAudioFuncCheckin)(mem32_t pArg, mem_ptr_t<CellSailAudioFrameInfo> pInfo);
typedef int(*CellSailRendererVideoFuncMakeup)(mem32_t pArg);
typedef int(*CellSailRendererVideoFuncCleanup)(mem32_t pArg);
typedef void(*CellSailRendererVideoFuncOpen)(mem32_t pArg, const mem_ptr_t<CellSailVideoFormat> pInfo, uint32_t frameNum, uint32_t minFrameNum);
typedef void(*CellSailRendererVideoFuncClose)(mem32_t pArg);
typedef void(*CellSailRendererVideoFuncStart)(mem32_t pArg, bool buffering);
typedef void(*CellSailRendererVideoFuncStop)(mem32_t pArg, bool flush, bool keepRendering);
typedef void(*CellSailRendererVideoFuncCancel)(mem32_t pArg);
typedef int(*CellSailRendererVideoFuncCheckout)(mem32_t pArg, mem_list_ptr_t<CellSailVideoFrameInfo> ppInfo);
typedef int(*CellSailRendererVideoFuncCheckin)(mem32_t pArg, mem_ptr_t<CellSailVideoFrameInfo> pInfo);

//These will probly need to be on hold for a while as they have 4+ args in the callback....
typedef int(*CellSailSourceFuncRead)(mem32_t pArg, s32 streamType, mem32_t pMediaInfo, const mem8_ptr_t pUri, u64 offset, mem8_ptr_t pBuf, u32 size, mem64_t pTotalSize);
typedef int(*CellSailSourceFuncReadSync)(mem32_t pArg, s32 streamType, mem32_t pMediaInfo, const mem8_ptr_t pUri, u64 offset, mem8_ptr_t pBuf, u32 size, mem64_t pTotalSize);
typedef int(*CellSailSourceFuncGetCapabilities)(mem32_t pArg, s32 streamType, mem32_t pMediaInfo, const mem8_ptr_t pUri, mem64_t pCapabilities);
typedef int(*CellSailSourceFuncInquireCapability)(mem32_t pArg, s32 streamType, mem32_t pMediaInfo, const mem8_ptr_t pUri, const mem_ptr_t<CellSailSourceStartCommand> pCommand);

struct CellSailPlayerResource
{
	//CellSpurs *pSpurs; todo: need spurs :(
	be_t<u32> pSpurs;
	be_t<u32> reserved0;
	be_t<u32> reserved1;
	be_t<u32> reserved2;
};

struct CellSailPlayerAttribute
{
	be_t<s32> preset;
	be_t<u32> maxAudioStreamNum;
	be_t<u32> maxVideoStreamNum;
	be_t<u32> maxUserStreamNum;
	be_t<u32> queueDepth;
	be_t<u32> reserved0;
	be_t<u32> reserved1;
	be_t<u32> reserved2;
};

struct CellSailMemAllocatorFuncs
{
	mem_beptr_t<CellSailMemAllocatorFuncAlloc> pAlloc;
	mem_beptr_t<CellSailMemAllocatorFuncFree> pFree;
};

struct CellSailMemAllocator
{
	CellSailMemAllocatorFuncs callbacks;
	be_t<u32> pArg; //void *
};

struct CellSailFuture
{
	be_t<u32> mutex; //sys_mutex_t
	be_t<u32> cond; //sys_cond_t
	be_t<volatile s32> flags;
	be_t<s32> result;
	be_t<u64> userParam;
};

struct CellSailSoundAdapterFuncs
{
	mem_beptr_t<CellSailSoundAdapterFuncMakeup> pMakeup;
	mem_beptr_t<CellSailSoundAdapterFuncCleanup> pCleanup;
	mem_beptr_t<CellSailSoundAdapterFuncFormatChanged> pFormatChanged;
};

struct CellSailSoundFrameInfo
{
	be_t<u32> pBuffer;
	be_t<u32> sessionId;
	be_t<u32> tag;
	be_t<s32> status;
	be_t<u64> pts;
};

struct CellSailSoundAdapter
{
	//be_t<u64> internalData[32];
	//more internal use of things, hope maths is right
	CellSailSoundAdapterFuncs soundAdapterFuncs; //
	be_t<u32> soundAdapterFuncsArgs;  // end of index 1 for u64
	CellSailAudioFormat audioFormat; //size of 3 u64, end of index 4
	be_t<u64> reservedData[31 - 5];
};

struct CellSailGraphicsAdapterFuncs
{
	mem_beptr_t<CellSailGraphicsAdapterFuncMakeup> pMakeup;
	mem_beptr_t<CellSailGraphicsAdapterFuncCleanup> pCleanup;
	mem_beptr_t<CellSailGraphicsAdapterFuncFormatChanged> pFormatChanged;
	mem_beptr_t<CellSailGraphicsAdapterFuncAllocFrame> pAlloc;
	mem_beptr_t<CellSailGraphicsAdapterFuncFreeFrame> pFree;
};

struct CellSailGraphicsFrameInfo
{
	be_t<u32> pBuffer;
	be_t<u32> sessionId;
	be_t<u32> tag;
	be_t<s32> status;
	be_t<u64> pts;
};

struct CellSailGraphicsAdapter
{
	//be_t<u64> internalData[32];
	CellSailGraphicsAdapterFuncs graphicsAdapterFuncs;
	be_t<u32> graphicsAdapterFuncsArgs; //end of index 2
	CellSailVideoFormat videoFormat; //size of 4 u64's end of index 6
	be_t<u64> reservedData[31 - 7];

};

struct CellSailPlayer
{
	//be_t<u64> internalData[128]; //if its internal, lets make liberal use of it
	mem_beptr_t<CellSailMemAllocatorFuncAlloc> pMemAllocFuncAlloc; //0
	mem_beptr_t<CellSailMemAllocatorFuncFree> pMemAllocFuncFree;
	be_t<u32> pMemAllocArgs; //1
	mem_beptr_t<CellSailPlayerFuncNotified> playerFuncNotified;
	be_t<u64> playerCallbackArg; //2
	CellSailPlayerAttribute playerAttributes; //should be 4x64bits...3 to 6
	CellSailPlayerResource playerResource; //should be 2x64bits..7 to 8
	be_t<u64> subscribedEvents; // 9
	mem_beptr_t<CellSailSoundAdapter> soundAdapters; // pointer to array of sound adapters
	mem_beptr_t<CellSailGraphicsAdapter> graphicsAdapters; //pointer to array of graphics adapters start of index 10
	be_t<u32> padding2;
	be_t<u64> funcExecCompleteArg; // 11?
	be_t<u64> padding3[127 - 12]; //hopefully my math is right
};

//access unit structs

struct CellSailAuInfo
{
	be_t<u32> pAu;
	be_t<u32> size;
	be_t<s32> status;
	be_t<u32> sessionId;
	be_t<u64> pts;
	be_t<u64> dts;
	be_t<u64> reserved;
};

struct CellSailAuReceiver
{
	be_t<u64> internalData[64];
};

struct CellSailRendererAudioFuncs
{
	mem_beptr_t<CellSailRendererAudioFuncMakeup> pMakeup;
	mem_beptr_t<CellSailRendererAudioFuncCleanup> pCleanup;
	mem_beptr_t<CellSailRendererAudioFuncOpen> pOpen;
	mem_beptr_t<CellSailRendererAudioFuncClose> pClose;
	mem_beptr_t<CellSailRendererAudioFuncStart> pStart;
	mem_beptr_t<CellSailRendererAudioFuncStop> pStop;
	mem_beptr_t<CellSailRendererAudioFuncCancel> pCancel;
	mem_beptr_t<CellSailRendererAudioFuncCheckout> pCheckout;
	mem_beptr_t<CellSailRendererAudioFuncCheckin> pCheckin;
};

struct CellSailRendererAudioAttribute
{
	be_t<u32> thisSize;
	mem_beptr_t<CellSailAudioFormat> pPreferredFormat;
};

struct CellSailRendererAudio
{
	be_t<u64> internalData[32];
};

struct CellSailRendererVideoFuncs
{
	mem_beptr_t<CellSailRendererVideoFuncMakeup> pMakeup;
	mem_beptr_t<CellSailRendererVideoFuncCleanup> pCleanup;
	mem_beptr_t<CellSailRendererVideoFuncOpen> pOpen;
	mem_beptr_t<CellSailRendererVideoFuncClose> pClose;
	mem_beptr_t<CellSailRendererVideoFuncStart> pStart;
	mem_beptr_t<CellSailRendererVideoFuncStop> pStop;
	mem_beptr_t<CellSailRendererVideoFuncCancel> pCancel;
	mem_beptr_t<CellSailRendererVideoFuncCheckout> pCheckout;
	mem_beptr_t<CellSailRendererVideoFuncCheckin> pCheckin;
};

struct CellSailRendererVideoAttribute
{
	be_t<u32> thisSize;
	mem_beptr_t<CellSailVideoFormat> pPreferredFormat;
};

struct CellSailRendererVideo
{
	be_t<u64> internalData[32];
};

struct CellSailSourceFuncs
{
	mem_beptr_t<CellSailSourceFuncMakeup> pMakeup;
	mem_beptr_t<CellSailSourceFuncCleanup> pCleanup;
	mem_beptr_t<CellSailSourceFuncOpen> pOpen;
	mem_beptr_t<CellSailSourceFuncClose> pClose;
	mem_beptr_t<CellSailSourceFuncStart> pStart;
	mem_beptr_t<CellSailSourceFuncStop> pStop;
	mem_beptr_t<CellSailSourceFuncCancel> pCancel;
	mem_beptr_t<CellSailSourceFuncCheckout> pCheckout;
	mem_beptr_t<CellSailSourceFuncCheckin> pCheckin;
	mem_beptr_t<CellSailSourceFuncClear> pClear;
	mem_beptr_t<CellSailSourceFuncRead> pRead;
	mem_beptr_t<CellSailSourceFuncReadSync> pReadSync;
	mem_beptr_t<CellSailSourceFuncGetCapabilities> pGetCapabilities;
	mem_beptr_t<CellSailSourceFuncInquireCapability> pInquireCapability;
};

struct CellSailSource
{
	//be_t<u64> internalData[20];
	//treating this like cellSailPlayer, using this data for internal things
	CellSailSourceFuncs sourceFuncs; //size of 7 u64
	be_t<mem32_t> pSourceFuncsArgs;
	be_t<u64> reservedData[20 - 9];

};

struct CellSailSourceCheckStream
{
	be_t<s32> streamType;
	be_t<u32> pMediaInfo;
	const mem_beptr_t<u8> pUri;//char *, might be correct
};

struct CellSailSourceCheckResource
{
	CellSailSourceCheckStream ok;
	CellSailSourceCheckStream readError;
	CellSailSourceCheckStream openError;
	CellSailSourceCheckStream startError;
	CellSailSourceCheckStream runningError;
};

struct CellSailMp4DateTime
{
	be_t<u16> second;
	be_t<u16> minute;
	be_t<u16> hour;
	be_t<u16> day;
	be_t<u16> month;
	be_t<u16> year;
	be_t<u16> reserved0;
	be_t<u16> reserved1;
};

struct CellSailMp4Movie
{
	be_t<u64> internalData[16];
};

struct CellSailMp4MovieInfo {
	CellSailMp4DateTime creationDateTime;
	CellSailMp4DateTime modificationDateTime;
	be_t<u32> trackCount;
	be_t<u32> movieTimeScale;
	be_t<u32> movieDuration;
	be_t<u32> reserved[16];
};

struct CellSailMp4Track
{
	be_t<u64> internalData[6];
};

struct CellSailMp4TrackInfo
{
	bool isTrackEnabled;
	u8 reserved0[3];
	be_t<u32> trackId;
	be_t<u64> trackDuration;
	be_t<s16> layer;
	be_t<s16> alternateGroup;
	be_t<u16> reserved1[2];
	be_t<u32> trackWidth;
	be_t<u32> trackHeight;
	be_t<u16> language;
	be_t<u16> reserved2;
	be_t<u16> mediaType;
	be_t<u32> reserved3[3];
};

struct CellSailAviMovie
{
	be_t<u64> internalData[16];
};

struct CellSailAviMovieInfo
{
	be_t<u32> maxBytesPerSec;
	be_t<u32> flags;
	be_t<u32> reserved0;
	be_t<u32> streams;
	be_t<u32> suggestedBufferSize;
	be_t<u32> width;
	be_t<u32> height;
	be_t<u32> scale;
	be_t<u32> rate;
	be_t<u32> length;
	be_t<u32> reserved1;
	be_t<u32> reserved2;
};

struct CellSailAviMainHeader
{
	be_t<u32> microSecPerFrame;
	be_t<u32> maxBytesPerSec;
	be_t<u32> paddingGranularity;
	be_t<u32> flags;
	be_t<u32> totalFrames;
	be_t<u32> initialFrames;
	be_t<u32> streams;
	be_t<u32> suggestedBufferSize;
	be_t<u32> width;
	be_t<u32> height;
	be_t<u32> reserved[4];
};

struct CellSailAviExtendedHeader
{
	be_t<u32> totalFrames;
};

struct CellSailAviStream
{
	be_t<u64> internalData[2];
};

//pretty sure i got this one, stupid unions
//todo: figure out what this union amounts to...
struct CellSailAviMediaType
{
	uint32_t fccType;
	uint32_t fccHandler;
	union {
		struct {
			uint16_t formatTag;
			uint16_t reserved;
			union {
				struct {
					uint16_t headLayer;
					uint16_t reserved;
				} mpeg;
			} u;
		} audio;
		struct {
			uint32_t compression;
			uint32_t reserved;
		} video;
	} u;
};

struct CellSailAviStreamHeader
{
	be_t<u32> fccType;
	be_t<u32> fccHandler;
	be_t<u32> flags;
	be_t<u16> priority;
	be_t<u32> initialFrames;
	be_t<u32> scale;
	be_t<u32> rate;
	be_t<u32> start;
	be_t<u32> length;
	be_t<u32> suggestedBufferSize;
	be_t<u32> quality;
	be_t<u32> sampleSize;
	be_t<u16> frameLeft;
	be_t<u16> frameTop;
	be_t<u16> frameRight;
	be_t<u16> frameBottom;
};

struct CellSailBitmapInfoHeader
{
	be_t<u32> size;
	be_t<s32> width;
	be_t<s32> height;
	be_t<u16> planes;
	be_t<u16> bitCount;
	be_t<u32> compression;
	be_t<u32> sizeImage;
	be_t<s32> xPelsPerMeter;
	be_t<s32> yPelsPerMeter;
	be_t<u32> clrUsed;
	be_t<u32> clrImportant;
};

struct CellSailWaveFormatEx
{
	be_t<u16> formatTag;
	be_t<u16> channels;
	be_t<u32> samplesPerSec;
	be_t<u32> avgBytesPerSec;
	be_t<u16> blockAlign;
	be_t<u16> bitsPerSample;
	be_t<u16> cbSize;
};

struct CellSailMpeg1WaveFormat
{
	CellSailWaveFormatEx wfx;
	be_t<u16> headLayer;
	be_t<u32> headBitrate;
	be_t<u16> headMode;
	be_t<u16> headModeExt;
	be_t<u16> headEmphasis;
	be_t<u16> headFlags;
	be_t<u32> PTSLow;
	be_t<u32> PTSHigh;
};

struct CellSailMpegLayer3WaveFormat
{
	CellSailWaveFormatEx wfx;
	be_t<u16> ID;
	be_t<u32> flags;
	be_t<u16> blockSize;
	be_t<u16> framesPerBlock;
	be_t<u16> codecDelay;
};

struct CellSailDescriptor
{
	be_t<u64> internalData[32];
};

struct CellSailStartCommand
{
	be_t<u32> startType;
	be_t<u32> seekType;
	be_t<u32> terminusType;
	be_t<u32> flags;
	be_t<u32> startArg;
	be_t<u32> reserved;
	be_t<u64> seekArg;
	be_t<u64> terminusArg;
};

struct CellSailFsReadFuncs
{
	mem_beptr_t<CellSailFsFuncOpen> pOpen;
	mem_beptr_t<CellSailFsFuncOpenSecond> pOpenSecond;
	mem_beptr_t<CellSailFsFuncClose> pClose;
	mem_beptr_t<CellSailFsFuncFstat> pFstat;
	mem_beptr_t<CellSailFsFuncRead> pRead;
	mem_beptr_t<CellSailFsFuncLseek> pLseek;
	mem_beptr_t<CellSailFsFuncCancel> pCancel;
	be_t<u32> reserved[2];
};

struct CellSailFsRead
{
	be_t<u32> capability;
	CellSailFsReadFuncs funcs;
};