#pragma once

#include "Emu/Io/PadHandler.h"
#include <Windows.h>
#include <mmsystem.h>
#include "Utilities/Config.h"
#include <mutex>

class mm_joystick_handler final : public PadHandlerBase
{
	// Unique names for the config files and our pad settings dialog

	enum MMJoyKeyCodes
	{
		Button1,
		Button2,
		Button3,
		Button4,
		Button5,
		Button6,
		Button7,
		Button8,
		Button9,
		Button10,
		Button11,
		Button12,
		Button13,
		Button14,
		Button15,
		Button16,
		Button17,
		Button18,
		Button19,
		Button20,
		Button21,
		Button22,
		Button23,
		Button24,
		Button25,
		Button26,
		Button27,
		Button28,
		Button29,
		Button30,
		Button31,
		Button32,

		Pov_up,
		Pov_right,
		Pov_down,
		Pov_left,

		joy_x,
		joy_y,
		joy_z,
		joy_r,
		joy_u,
		joy_v,

		joy_pov_x,
		joy_pov_y,

		KeyCodeCount
	};

	const std::map<u32, std::string> button_list =
	{
		{ Button1, "Button 1"  },
		{ Button2, "Button 2"  },
		{ Button3, "Button 3"  },
		{ Button4, "Button 4"  },
		{ Button5, "Button 5"  },
		{ Button6, "Button 6"  },
		{ Button7, "Button 7"  },
		{ Button8, "Button 8"  },
		{ Button9, "Button 9"  },
		{ Button10, "Button 10" },
		{ Button11, "Button 11" },
		{ Button12, "Button 12" },
		{ Button13, "Button 13" },
		{ Button14, "Button 14" },
		{ Button15, "Button 15" },
		{ Button16, "Button 16" },
		{ Button17, "Button 17" },
		{ Button18, "Button 18" },
		{ Button19, "Button 19" },
		{ Button20, "Button 20" },
		{ Button21, "Button 21" },
		{ Button22, "Button 22" },
		{ Button23, "Button 23" },
		{ Button24, "Button 24" },
		{ Button25, "Button 25" },
		{ Button26, "Button 26" },
		{ Button27, "Button 27" },
		{ Button28, "Button 28" },
		{ Button29, "Button 29" },
		{ Button30, "Button 30" },
		{ Button31, "Button 31" },
		{ Button32, "Button 32" },

		{ Pov_up,  "POV Up" },
		{ Pov_right,    "POV Right" },
		{ Pov_down, "POV Down" },
		{ Pov_left,     "POV Left" }
	};

	// Unique names for the config files and our pad settings dialog
	const std::map<u32, std::string> axis_list =
	{
		{ joy_x, "X" },
		{ joy_y, "Y" },
		{ joy_z, "Z" },
		{ joy_r, "R" },
		{ joy_u, "U" },
		{ joy_v, "V" },
		{ joy_pov_x, "POV X" },
		{ joy_pov_y, "POV Y" },
	};

	struct MMJOYDevice
	{
		u32 device_id{ 0 };
		bool last_conn_status{ false };
		std::string device_name{ "" };
		JOYINFOEX device_info;
		JOYCAPS device_caps;
	};

	bool is_init = false;

	std::unordered_map<int, std::unique_ptr<MMJOYDevice>> m_devices;
	std::vector<std::pair<MMJOYDevice*, std::shared_ptr<Pad>>> bindings;
	std::mutex handlerLock;

public:
	mm_joystick_handler();
	~mm_joystick_handler();

	std::vector<std::string> ListDevices() override;
	void ThreadProc() override;
	void init_config(pad_config* cfg, const std::string& name) override;
	u32 GetNumPads() override { std::lock_guard<std::mutex> lock(handlerLock); return static_cast<u32>(bindings.size()); }
	s32 EnableGetDevice(const std::string& deviceName) override;
	bool IsDeviceConnected(u32 deviceNumber) override;
    void DisableDevice(u32 deviceNumber) override;
    void SetVibrate(u32, u32, u32) override {};
    std::shared_ptr<Pad> GetDeviceData(u32 deviceNumber) override;
    void SetRGBData(u32, u8, u8, u8) override {};

private:
	bool Init();
	bool GetMMJOYDevice(int index, MMJOYDevice& dev);
	int GetDeviceNumber(const std::string& padId);
};
