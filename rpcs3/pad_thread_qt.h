#pragma once

#include <unordered_map>
#include <thread>
#include <array>
#include <mutex>

#include "../Utilities/types.h"
#include "../Utilities/mutex.h"
#include "Emu/Io/PadThread.h"

class pad_thread_qt : public PadThread
{
private:
	struct pad_device {
		u32 dev_id{0};
		// port_setting is only kept locally and not sent to any of the handlers or devices
		atomic_t<u32> port_setting{0};
        std::unique_ptr<EmulatedPad> ds3_pad;
	};
public:
	pad_thread_qt(void *_curthread, void *_curwindow);
	~pad_thread_qt();

	void SetRumble(const u32 pad, u8 largeMotor, bool smallMotor) override;

	void OverlayInUse(bool inuse) override { overlay_in_use = inuse; }
	bool IsOverlayInUse() override { return overlay_in_use; }

	PadDataBuffer GetPadData(u32 pad_num) override;
	PadStatus GetPadStatus(u32 pad_num) override;
	void SetPortSetting(u32 pad_num, u32 port_setting) override;

	// todo: dont have the ui understand or care about actual handlers, doing this currently for ease of porting 

	// this is just for UI binding, created pad *will not* be reflected in above functions until config is saved and refresh is called
	// it will also return null if handler is invalid for current run(ex: evdev on windows), or if the device is not valid
	std::unique_ptr<EmulatedPad> CreateTemporaryPad(pad_handler handler, const std::string& device) override;
	std::vector<std::string> GetDeviceListForHandler(pad_handler handler) override;
	// Refreshs pads based on config, this may kill any temporary pads that exist if their device doesnt exist in the config anymore
	void RefreshPads() override;

    void EnableCapture(void* arg);

	void init_config(pad_handler type, pad_config& cfg, const std::string& cfg_name) override;

private:
	void ThreadFunc();

    std::string get_config_filename(u32 i) {
		if (i > g_cfg_input.player.size()) 
			return "";
		return fs::get_config_dir() + "/InputConfigs/" + g_cfg_input.player[i]->handler.to_string() + "/" + g_cfg_input.player[i]->profile.to_string() + ".yml";
    }

	const static u32 MAX_PADS = 7;

	bool overlay_in_use{false};

    std::array<pad_config, MAX_PADS> m_pad_configs;

	//List of all handlers
	std::map<pad_handler, std::shared_ptr<PadHandlerBase>> handlers;

	//Used for pad_handler::keyboard
	void *curthread;
	void *curwindow;

	std::array<std::pair<pad_handler, pad_device>, MAX_PADS> m_devices;
	std::mutex m_deviceLock;

	bool active;
	std::shared_ptr<std::thread> thread;
};
