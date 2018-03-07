#include "pad_thread_qt.h"
#include "rpcs3qt/gamepads_settings_dialog.h"
#include "../ds4_pad_handler.h"
#ifdef _WIN32
#include "../xinput_pad_handler.h"
#include "../mm_joystick_handler.h"
#elif HAVE_LIBEVDEV
#include "../evdev_joystick_handler.h"
#endif
#include "../keyboard_pad_handler.h"
#include "../Emu/Io/Null/NullPadHandler.h"

pad_thread_qt::pad_thread_qt(void *_curthread, void *_curwindow) : curthread(_curthread), curwindow(_curwindow)
{
	g_cfg_input.from_default();
	g_cfg_input.load();

	handlers.emplace(pad_handler::ds4, std::make_shared<ds4_pad_handler>());

	auto keyhandler = std::make_shared<keyboard_pad_handler>();
	keyhandler->moveToThread((QThread *)curthread);
	if (curwindow != nullptr)
		keyhandler->SetTargetWindow((QWindow *)curwindow);
	handlers.emplace(pad_handler::keyboard, std::move(keyhandler));

#ifdef _MSC_VER
	handlers.emplace(pad_handler::xinput, std::make_shared<xinput_pad_handler>());
#endif
#ifdef _WIN32
	handlers.emplace(pad_handler::mm, std::make_shared<mm_joystick_handler>());
#endif
#ifdef HAVE_LIBEVDEV
	handlers.emplace(pad_handler::evdev, std::make_shared<evdev_joystick_handler>());
#endif

	//Always have a Null Pad Handler
	auto nullpad = handlers.emplace(pad_handler::null, std::make_shared<NullPadHandler>()).first->second;

	for (u32 i = 0; i < MAX_PADS; i++)
	{
		std::shared_ptr<PadHandlerBase> cur_pad_handler = nullpad;

		const auto &handler_type = g_cfg_input.player[i]->handler;

		auto it = handlers.find(handler_type);
		if (it != handlers.end())
			cur_pad_handler = it->second;

		s32 devid = cur_pad_handler->EnableGetDevice(g_cfg_input.player[i]->device.to_string());
		if (devid > -1) {
            cur_pad_handler->init_config(&m_pad_configs[i], get_config_filename(i));
			m_pad_configs[i].load();

			m_devices[i].first = handler_type;
			m_devices[i].second.dev_id = static_cast<u32>(devid);
            // todo: base this class on the emulated 'type' of controller
            m_devices[i].second.ds3_pad = std::make_unique<EmulatedPad>(devid, m_pad_configs[i], cur_pad_handler);
		}
		else {
			LOG_ERROR(GENERAL, "Failed to bind device %s to handler %s", g_cfg_input.player[i]->device.to_string(), handler_type.to_string());
			m_devices[i].first = pad_handler::null;
			m_devices[i].second.dev_id = 0;
		}
	}

	thread = std::make_shared<std::thread>(&pad_thread_qt::ThreadFunc, this);
}

pad_thread_qt::~pad_thread_qt()
{
	active = false;
	thread->join();

	handlers.clear();
}

void pad_thread_qt::SetRumble(const u32 pad, u8 largeMotor, bool smallMotor)
{
	if (pad >= MAX_PADS)
		return;

	std::lock_guard<std::mutex> lock(m_deviceLock);
	const auto& dev = m_devices[pad];

    dev.second.ds3_pad->SetRumble(largeMotor, smallMotor);
}

void pad_thread_qt::ThreadFunc()
{
	active = true;
	while (active)
	{
		for (auto& cur_pad_handler : handlers)
		{
			if (cur_pad_handler.second->GetNumPads() > 0)
				cur_pad_handler.second->ThreadProc();
		}
	    std::this_thread::sleep_for(1ms);
	}
}

PadStatus pad_thread_qt::GetPadStatus(u32 pad_num) {
	PadStatus padStatus;

	if (pad_num >= MAX_PADS)
		return PadStatus();

	std::lock_guard<std::mutex> lock(m_deviceLock);

	const auto& dev = m_devices[pad_num];
	bool connected = handlers[dev.first]->IsDeviceConnected(dev.second.dev_id);

	if (connected) {
        padStatus.m_device_capability = dev.second.ds3_pad->pad_capabilities;
		padStatus.m_device_type = dev.second.ds3_pad->device_type;
		padStatus.m_port_status = connected;
	}
	padStatus.m_port_setting = dev.second.port_setting;
	return padStatus;
}

void pad_thread_qt::SetPortSetting(u32 pad_num, u32 port_setting) {
	
	if (pad_num >= MAX_PADS)
		return;

	m_devices[pad_num].second.port_setting = port_setting;
}

std::unique_ptr<EmulatedPad> pad_thread_qt::CreateTemporaryPad(pad_handler handler, const std::string& device) {
	auto it = handlers.find(handler);
	if (it == handlers.end())
		return nullptr;

	s32 devid = it->second->EnableGetDevice(device);
	if (devid == -1)
		return nullptr;
	pad_config cfg;
	it->second->init_config(&cfg, "");
	return std::make_unique<EmulatedPad>(devid, cfg, it->second);
}

std::vector<std::string> pad_thread_qt::GetDeviceListForHandler(pad_handler handler) {
	auto it = handlers.find(handler);
	if (it == handlers.end())
		return{""};
	else return it->second->ListDevices();
}

void pad_thread_qt::RefreshPads() {
	// todo: this could possibly be changed to happen in thread proc if its too slow
	// Always have a Null Pad Handler
	auto nullcheck = handlers.find(pad_handler::null);
	if (nullcheck == handlers.end()) {
		LOG_ERROR(GENERAL, "No null handler found during pad refresh");
		return;
	}

	// todo: 'refresh' controller if only config changed, 
	// also disable devices that are no longer used
	std::lock_guard<std::mutex> lock(m_deviceLock);

	auto nullpad = nullcheck->second;

	for (u32 i = 0; i < MAX_PADS; i++)
	{
		std::shared_ptr<PadHandlerBase> cur_pad_handler = nullpad;

		const auto &handler_type = g_cfg_input.player[i]->handler;

		auto it = handlers.find(handler_type);
		if (it != handlers.end())
			cur_pad_handler = it->second;

		s32 devid = cur_pad_handler->EnableGetDevice(g_cfg_input.player[i]->device.to_string());
		if (devid > -1) {
			cur_pad_handler->init_config(&m_pad_configs[i], get_config_filename(i));
			m_devices[i].first = handler_type;
			m_devices[i].second.dev_id = static_cast<u32>(devid);
			// todo: base this class on the emulated 'type' of controller
			m_devices[i].second.ds3_pad = std::make_unique<EmulatedPad>(devid, m_pad_configs[i], cur_pad_handler);
		}
		else {
			LOG_ERROR(GENERAL, "Failed to bind device %s to handler %s", g_cfg_input.player[i]->device.to_string(), handler_type.to_string());
			m_devices[i].first = pad_handler::null;
			m_devices[i].second.dev_id = 0;
		}
	}
}

void pad_thread_qt::init_config(pad_handler type, pad_config& cfg, const std::string& cfg_name) {
	auto it = handlers.find(type);
	if (it != handlers.end())
		it->second->init_config(&cfg, cfg_name);
}

PadDataBuffer pad_thread_qt::GetPadData(u32 pad_num) {
	if (pad_num >= MAX_PADS)
		return PadDataBuffer();

	// todo: merge padstatus above with this?
	std::lock_guard<std::mutex> lock(m_deviceLock);
	const auto& dev = m_devices[pad_num];

    auto padData = dev.second.ds3_pad->GetPadData();

#ifdef _WIN32
	if (padData.m_digital_1 || padData.m_digital_2)
		SetThreadExecutionState(ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);
#endif

	return padData;
}

void pad_thread_qt::EnableCapture(void* arg) {
    auto it = handlers.find(pad_handler::keyboard);
    if (it != handlers.end()) {
        auto k = std::dynamic_pointer_cast<keyboard_pad_handler>(it->second);
        k->SetTargetWindow((QWindow *)arg);
    }
}
