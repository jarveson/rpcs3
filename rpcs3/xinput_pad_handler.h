#pragma once

#include "Utilities/Config.h"
#include "Emu/Io/PadHandler.h"
#define NOMINMAX
#include <Windows.h>
#include <Xinput.h>
#include <ctime>
#include <mutex>

namespace XINPUT_INFO
{
	const DWORD THREAD_TIMEOUT = 1000;
	const DWORD THREAD_SLEEP = 10;
	const DWORD THREAD_SLEEP_INACTIVE = 100;
	const DWORD GUIDE_BUTTON = 0x0400;
	const LPCWSTR LIBRARY_FILENAMES[] = {
		L"xinput1_4.dll",
		L"xinput1_3.dll",
		L"xinput1_2.dll",
		L"xinput9_1_0.dll"
	};
}

class xinput_pad_handler final : public PadHandlerBase
{
	// These are all the possible buttons on a standard xbox 360 or xbox one controller
	enum XInputKeyCodes
	{
		A,
		B,
		X,
		Y,
		Left,
		Right,
		Up,
		Down,
		LB,
		RB,
		LS,
		RS,
		Start,
		Back,
		Guide,

		LT,
		RT,

		LSX,
		LSY,
		RSX,
		RSY,

		KeyCodeCount
	};

	// Unique names for the config files and our pad settings dialog
	const std::unordered_map<u32, std::string> button_list =
	{
		{ XInputKeyCodes::A,      "A" },
		{ XInputKeyCodes::B,      "B" },
		{ XInputKeyCodes::X,      "X" },
		{ XInputKeyCodes::Y,      "Y" },
		{ XInputKeyCodes::Left,   "Left" },
		{ XInputKeyCodes::Right,  "Right" },
		{ XInputKeyCodes::Up,     "Up" },
		{ XInputKeyCodes::Down,   "Down" },
		{ XInputKeyCodes::LB,     "LB" },
		{ XInputKeyCodes::RB,     "RB" },
		{ XInputKeyCodes::Back,   "Back" },
		{ XInputKeyCodes::Start,  "Start" },
		{ XInputKeyCodes::LS,     "LS" },
		{ XInputKeyCodes::RS,     "RS" },
		{ XInputKeyCodes::Guide,  "Guide" },
		// I realize these aren't 'buttons', but for the purposes of the emulator, they are
		{ XInputKeyCodes::LT,     "LT" },
		{ XInputKeyCodes::RT,     "RT" },
	};

	const std::unordered_map<u32, std::string> axis_list = 
	{
		{ XInputKeyCodes::LSX,    "LS X" },
		{ XInputKeyCodes::LSY,    "LS Y" },
		{ XInputKeyCodes::RSX,    "RS X" },
		{ XInputKeyCodes::RSY,    "RS Y" },
	};

	struct XInputDevice
	{
		u32 deviceNumber{ 0 };
		bool last_conn_status{ false };
		bool newVibrateData{ true };
		u8 largeVibrate{ 0 };
		u8 smallVibrate{ 0 };
		clock_t last_vibration{ 0 };
	};

	typedef void (WINAPI * PFN_XINPUTENABLE)(BOOL);
	typedef DWORD(WINAPI * PFN_XINPUTGETSTATE)(DWORD, XINPUT_STATE *);
	typedef DWORD(WINAPI * PFN_XINPUTSETSTATE)(DWORD, XINPUT_VIBRATION *);
	typedef DWORD(WINAPI * PFN_XINPUTGETBATTERYINFORMATION)(DWORD, BYTE, XINPUT_BATTERY_INFORMATION *);

	bool is_init{ false };
	HMODULE library{ nullptr };
	PFN_XINPUTGETSTATE xinputGetState{ nullptr };
	PFN_XINPUTSETSTATE xinputSetState{ nullptr };
	PFN_XINPUTENABLE xinputEnable{ nullptr };
	PFN_XINPUTGETBATTERYINFORMATION xinputGetBatteryInformation{ nullptr };

	std::vector<std::pair<std::unique_ptr<XInputDevice>, std::shared_ptr<Pad>>> bindings;
	std::mutex handlerLock;

public:
	xinput_pad_handler();
	~xinput_pad_handler();

	std::vector<std::string> ListDevices() override;
	void ThreadProc() override;
	void init_config(pad_config* cfg, const std::string& name) override;
	u32 GetNumPads() override { std::lock_guard<std::mutex> lock(handlerLock); return static_cast<u32>(bindings.size()); }

	s32 EnableGetDevice(const std::string& deviceName) override;
	bool IsDeviceConnected(u32 deviceNumber) override;
    void DisableDevice(u32 deviceNumber) override;
	std::shared_ptr<Pad> GetDeviceData(u32 deviceNumber) override;

	void SetVibrate(u32 deviceNumber, u32 keycode, u32 value) override;
    void SetRGBData(u32, u8, u8, u8) override {};

private:
	bool Init();
	int GetDeviceNumber(const std::string& padId);
	std::array<u16, XInputKeyCodes::KeyCodeCount> GetButtonValues(const XINPUT_STATE& state);
};
