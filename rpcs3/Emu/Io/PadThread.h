#pragma once

#include "Utilities/types.h"
#include "Emu/Io/PadHandler.h"
#include "Emu/Io/EmulatedPad.h"

class PadThread
{
public:
	virtual void SetRumble(const u32 pad, u8 largeMotor, bool smallMotor) = 0;

	virtual void OverlayInUse(bool inuse) = 0;
	virtual bool IsOverlayInUse() = 0;

	virtual PadDataBuffer GetPadData(u32 pad_num) = 0;
	virtual PadStatus GetPadStatus(u32 pad_num) = 0;
	virtual void SetPortSetting(u32 pad_num, u32 port_setting) = 0;

	// todo: dont have the ui understand or care about actual handlers, doing this currently for ease of porting 

	// this is just for UI binding, created pad *will not* be reflected in above functions until config is saved and refresh is called
	// it will also return null if handler is invalid for current run(ex: evdev on windows), or if the device is not valid
	virtual std::unique_ptr<EmulatedPad> CreateTemporaryPad(pad_handler handler, const std::string& device) = 0;
	virtual std::vector<std::string> GetDeviceListForHandler(pad_handler handler) = 0;
	// Refreshs pads, this may kill any temporary pads that exist if their device doesnt exist anymore
	virtual void RefreshPads() = 0;

    // These functions enable reporting and capturing for device button presses / stick movements
	// mainly just for enabling qt capture on various windows
    virtual void EnableCapture(void* arg) = 0;

	virtual void init_config(pad_handler type, pad_config& cfg, const std::string& cfg_name) = 0;

	static std::string get_config_dir(pad_handler type) {
		return fs::get_config_dir() + "/InputConfigs/" + fmt::format("%s", type) + "/";
	}
};
