// This makes debugging on windows less painful
#define HAVE_LIBEVDEV

#ifdef HAVE_LIBEVDEV

#include "rpcs3qt/pad_settings_dialog.h"
#include "evdev_joystick_handler.h"
#include "Utilities/Thread.h"
#include "Utilities/Log.h"

#include <functional>
#include <algorithm>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>
#include <cstdio>
#include <cmath>

evdev_joystick_handler::evdev_joystick_handler() : PadHandlerBase(pad_handler::evdev, "EvDev", "")
{
	RefreshDevices();
}

evdev_joystick_handler::~evdev_joystick_handler()
{
	Close();
}

void evdev_joystick_handler::init_config(pad_config* cfg, const std::string& name)
{
	// Set this profile's save location
	cfg->cfg_name = name;

	// Set default button mapping
	cfg->ls_left.def  = axis_list.at(ABS_X) + "-";
	cfg->ls_down.def  = axis_list.at(ABS_Y) + "+";
	cfg->ls_right.def = axis_list.at(ABS_X) + "+";
	cfg->ls_up.def    = axis_list.at(ABS_Y) + "-";
	cfg->rs_left.def  = axis_list.at(ABS_RX) + "-";
	cfg->rs_down.def  = axis_list.at(ABS_RY) + "+";
	cfg->rs_right.def = axis_list.at(ABS_RX) + "+";
	cfg->rs_up.def    = axis_list.at(ABS_RY) + "-";
	cfg->start.def    = button_list.at(BTN_START);
	cfg->select.def   = button_list.at(BTN_SELECT);
	cfg->ps.def       = button_list.at(BTN_MODE);
	cfg->square.def   = button_list.at(BTN_X);
	cfg->cross.def    = button_list.at(BTN_A);
	cfg->circle.def   = button_list.at(BTN_B);
	cfg->triangle.def = button_list.at(BTN_Y);
	cfg->left.def     = axis_list.at(ABS_HAT0X) + "-";
	cfg->down.def     = axis_list.at(ABS_HAT0Y) + "+";
	cfg->right.def    = axis_list.at(ABS_HAT0X) + "+";
	cfg->up.def       = axis_list.at(ABS_HAT0Y) + "-";
	cfg->r1.def       = button_list.at(BTN_TR);
	cfg->r2.def       = axis_list.at(ABS_RZ) + "+";;
	cfg->r3.def       = button_list.at(BTN_THUMBR);
	cfg->l1.def       = button_list.at(BTN_TL);
	cfg->l2.def       = axis_list.at(ABS_Z) + "+";;
	cfg->l3.def       = button_list.at(BTN_THUMBL);

	// Set default misc variables
	cfg->lstickdeadzone.def    = 30;   // between 0 and 255
	cfg->rstickdeadzone.def    = 30;   // between 0 and 255
	cfg->ltriggerthreshold.def = 0;    // between 0 and 255
	cfg->rtriggerthreshold.def = 0;    // between 0 and 255
	cfg->padsquircling.def     = 5000;

	// apply defaults
	cfg->from_default();
}

bool evdev_joystick_handler::update_device(EvdevDevice& device)
{
	const auto& path = device.path;
	libevdev*& dev = device.device;

	bool was_connected = dev != nullptr;

	if (access(path.c_str(), R_OK) == -1)
	{
		if (was_connected)
		{
			int fd = libevdev_get_fd(dev);
			libevdev_free(dev);
			close(fd);
			dev = nullptr;
		}

		LOG_ERROR(GENERAL, "Joystick %s is not present or accessible [previous status: %d]", path.c_str(), was_connected ? 1 : 0);
		return false;
	}

	if (was_connected)
		return true;  // It's already been connected, and the js is still present.

	int fd = open(path.c_str(), O_RDWR | O_NONBLOCK);
	if (fd == -1)
	{
		int err = errno;
		LOG_ERROR(GENERAL, "Failed to open joystick: %s [errno %d]", strerror(err), err);
		return false;
	}

	int ret = libevdev_new_from_fd(fd, &dev);
	if (ret < 0)
	{
		LOG_ERROR(GENERAL, "Failed to initialize libevdev for joystick: %s [errno %d]", strerror(-ret), -ret);
		return false;
	}

	LOG_NOTICE(GENERAL, "Opened joystick: '%s' at %s (fd %d)", libevdev_get_name(dev), path, fd);
	return true;
}

void evdev_joystick_handler::update_devs()
{
	for (auto& device : devices)
	{
		update_device(*device);
	}
}

void evdev_joystick_handler::Close()
{
	for (auto& device : devices)
	{
		auto& dev = device->device;
		if (dev != nullptr)
		{
			int fd = libevdev_get_fd(dev);
			if (device->effect_id != -1)
				ioctl(fd, EVIOCRMFF, device->effect_id);
			libevdev_free(dev);
			close(fd);
		}
	}
}

void evdev_joystick_handler::RefreshDevices() {

	// populate devices
	fs::dir devdir{ "/dev/input/" };
	fs::dir_entry et;
	while (devdir.read(et))
	{
		// Check if the entry starts with event (a 5-letter word)
		if (et.name.size() > 5 && et.name.compare(0, 5, "event") == 0)
		{
			std::string path = "/dev/input/" + et.name;
			int fd = open(path.c_str(), O_RDWR | O_NONBLOCK);
			struct libevdev *dev = nullptr;
			int rc = libevdev_new_from_fd(fd, &dev);
			if (rc < 0)
			{
				// If it's just a bad file descriptor, don't bother logging, but otherwise, log it.
				if (rc != -9)
					LOG_WARNING(GENERAL, "Failed to connect to device at %s, the error was: %s", path, strerror(-rc));
				close(fd);
				continue;
			}
			const std::string name = et.name + ": " + libevdev_get_name(dev);
			if (libevdev_has_event_type(dev, EV_KEY) &&
				libevdev_has_event_code(dev, EV_ABS, ABS_X) &&
				libevdev_has_event_code(dev, EV_ABS, ABS_Y))
			{
				// It's a joystick. Now let's make sure we don't already have this one.
				auto it = std::find_if(devices.begin(), devices.end(), [&path](const auto &device) { return path == device->path; });
				if (it != devices.end())
				{
					libevdev_free(dev);
					close(fd);
					continue;
				}

				// Alright, now that we've confirmed we haven't added this joystick yet, les do dis.
				auto evdev = std::make_unique<EvdevDevice>();
				evdev->device = dev;
				evdev->path = std::move(path);
				evdev->name = std::move(name);
				evdev->has_rumble = libevdev_has_event_type(dev, EV_FF) == 1;
				devices.push_back(std::move(evdev));
			}
			libevdev_free(dev);
			close(fd);
		}
	}
}

// https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/InputCommon/ControllerInterface/evdev/evdev.cpp
// https://github.com/reicast/reicast-emulator/blob/master/core/linux-dist/evdev.cpp
// http://www.infradead.org/~mchehab/kernel_docs_pdf/linux-input.pdf
void evdev_joystick_handler::SetRumble(EvdevDevice* device, u16 large, u16 small)
{
	if (device == nullptr || !device->has_rumble || device->effect_id == -2)
		return;

	int fd = libevdev_get_fd(device->device);
	if (fd < 0)
		return;

	if (large == device->force_large && small == device->force_small)
		return;

	// XBox One Controller can't handle faster vibration updates than ~10ms. Elite is even worse.
	// So I'll use 20ms to be on the safe side. No lag was noticable.
	if (clock() - device->last_vibration < 20)
		return;

	device->last_vibration = clock();

	// delete the previous effect (which also stops it)
	if (device->effect_id != -1)
	{
		ioctl(fd, EVIOCRMFF, device->effect_id);
		device->effect_id = -1;
	}

	if (large == 0 && small == 0)
	{
		device->force_large = large;
		device->force_small = small;
		return;
	}

	ff_effect effect;

	if (libevdev_has_event_code(device->device, EV_FF, FF_RUMBLE))
	{
		effect.type = FF_RUMBLE;
		effect.id = device->effect_id;
		effect.direction = 0;
		effect.u.rumble.strong_magnitude = large;
		effect.u.rumble.weak_magnitude = small;
		effect.replay.length = 0;
		effect.replay.delay = 0;
		effect.trigger.button = 0;
		effect.trigger.interval = 0;
	}
	else
	{
		// TODO: handle other Rumble effects
		device->effect_id = -2;
		return;
	}

	if (ioctl(fd, EVIOCSFF, &effect) == -1)
	{
		LOG_ERROR(HLE, "evdev SetRumble ioctl failed! [large = %d] [small = %d] [fd = %d]", large, small, fd);
		device->effect_id = -2;
	}

	device->effect_id = effect.id;

	input_event play;
	play.type = EV_FF;
	play.code = device->effect_id;
	play.value = 1;

	if (write(fd, &play, sizeof(play)) == -1)
	{
		LOG_ERROR(HLE, "evdev SetRumble write failed! [large = %d] [small = %d] [fd = %d] [effect_id = %d]", large, small, fd, device->effect_id);
		device->effect_id = -2;
	}

	device->force_large = large;
	device->force_small = small;
}

void evdev_joystick_handler::ThreadProc()
{
	std::lock_guard<std::mutex> lock(handlerLock);
	update_devs();

	for (const auto& bind : bindings)
	{
		auto device = bind.first;
		auto& dev = device->device;
		auto pad = bind.second.get();
		std::lock_guard<std::mutex> lock2(pad->pad_lock);
		if (dev == nullptr)
		{
			if (device->last_conn_status == true)
			{
				// It was disconnected.
				LOG_ERROR(HLE, "evdev device %s disconnected", device->name);
				device->last_conn_status = false;
			}
			continue;
		}

		if (device->last_conn_status == false)
		{
			// Connection status changed from disconnected to connected.
			LOG_ERROR(HLE, "evdev device %d reconnected", device->name);
			device->last_conn_status = true;
		}

		// Handle vibration
		u16 force_large = pad->m_vibrateMotors[0].m_value * 257;
		u16 force_small = pad->m_vibrateMotors[1].m_value * 257;
		SetRumble(device, force_large, force_small);

		// Try to query the latest event from the joystick.
		input_event evt;
		int ret = LIBEVDEV_READ_STATUS_SUCCESS;
		while (ret >= 0) {
			if (ret == LIBEVDEV_READ_STATUS_SYNC)
				ret = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &evt);
			else
				ret = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &evt);
		}

		// -EAGAIN signifies no available events, not an actual *error*.
		if (ret != -EAGAIN)
		{
			LOG_ERROR(GENERAL, "Failed to read latest event from joystick: %s [errno %d]", strerror(-ret), -ret);
			continue;
		}

		// Translate button presses to axis
		for (auto& btn : pad->m_buttons) {
			int val = 0;
			if (libevdev_fetch_event_value(dev, EV_KEY, btn.m_keyCode, &val) == 0)
				btn.m_value = 0;
			else
				btn.m_value = val > 0 ? 255 : 0;
		}

		for (auto& axi : pad->m_sticks) {
			int val = 0;
			if (libevdev_fetch_event_value(dev, EV_ABS, axi.m_keyCode, &val) == 0)
				axi.m_valueHigh = axi.m_valueLow = 0.f;
			else {
				int min = libevdev_get_abs_minimum(dev, axi.m_keyCode);
				int max = libevdev_get_abs_maximum(dev, axi.m_keyCode);
				float fval = Clamp(float(val - min) / (max - min), 0.f, 1.f);
				int mid = (min + (max - min) / 2);
				if (val > mid) {
					axi.m_valueLow = 0.f;
					axi.m_valueHigh = (fval - 0.5f) * 2.f;
				}
				else {
					axi.m_valueHigh = 0.f;
					axi.m_valueLow = (0.5f - std::min(0.5f, fval)) * 2.0;
				}
			}
		}
	}
}

std::vector<std::string> evdev_joystick_handler::ListDevices()
{
	std::lock_guard<std::mutex> lock(handlerLock);
	std::vector<std::string> evdev_dev_list;

	RefreshDevices();

	evdev_dev_list.reserve(devices.size());
	for (const auto& dev : devices) {
		if (dev->last_conn_status)
			evdev_dev_list.emplace_back(dev->name);
	}

	return evdev_dev_list;
}

s32 evdev_joystick_handler::EnableGetDevice(const std::string& dname) {
	std::lock_guard<std::mutex> lock(handlerLock);
	RefreshDevices();

	EvdevDevice* dev = nullptr;

	auto cont = std::find_if(devices.begin(), devices.end(), [&dname](const auto& d) {return d->name == dname; });
	if (cont == devices.end()) {
		// if not found, force emplace an 'invalid' device
		auto tmp = std::make_unique<EvdevDevice>();
		tmp->name = dname;
		tmp->device_id = static_cast<u32>(devices.size() - 1);
		devices.push_back(std::move(tmp));
	}
	else
		dev = cont->get();

	// check if device is already 'enabled'
	auto it = std::find_if(bindings.begin(), bindings.end(), [dev](const auto& d) {return d.first->device_id == dev->device_id; });
	if (it != bindings.end())
		return dev->device_id;

	std::shared_ptr<Pad> pad = std::make_shared<Pad>();
	pad->m_buttons.reserve(button_list.size());
	for(const auto& btn : button_list)
		pad->m_buttons.emplace_back(btn.first, btn.second);

	pad->m_buttons.reserve(axis_list.size());
	for(const auto& axi: axis_list)
		pad->m_sticks.emplace_back(axi.first, axi.second);

	pad->m_vibrateMotors.emplace_back(true, 0);
	pad->m_vibrateMotors.emplace_back(false, 0);

	bindings.emplace_back(dev, std::move(pad));
	return dev->device_id;
}

bool evdev_joystick_handler::IsDeviceConnected(u32 deviceNumber) {
	std::lock_guard<std::mutex> lock(handlerLock);
	auto it = std::find_if(bindings.begin(), bindings.end(), [deviceNumber](const auto& d) {return d.first->device_id == deviceNumber; });
	if (it == bindings.end())
		return false;

	return it->first->last_conn_status;
}

void evdev_joystick_handler::DisableDevice(u32 deviceNumber) {
	std::lock_guard<std::mutex> lock(handlerLock);
	auto it = std::find_if(bindings.begin(), bindings.end(), [deviceNumber](const auto& d) {return d.first->device_id == deviceNumber; });
	if (it == bindings.end())
		return;

	bindings.erase(it);
}

std::shared_ptr<Pad> evdev_joystick_handler::GetDeviceData(u32 deviceNumber) {
	std::lock_guard<std::mutex> lock(handlerLock);
	auto it = std::find_if(bindings.begin(), bindings.end(), [deviceNumber](const auto& d) { return d.first->device_id == deviceNumber; });
	if (it == bindings.end())
		return nullptr;

	return it->second;
}

void evdev_joystick_handler::SetVibrate(u32 deviceNumber, u32 keycode, u32 value) {
	std::lock_guard<std::mutex> lock(handlerLock);
	auto it = std::find_if(bindings.begin(), bindings.end(), [deviceNumber](const auto& d) { return d.first->device_id == deviceNumber; });
	if (it == bindings.end())
		return;

	auto &motors = it->second->m_vibrateMotors;
	if (keycode > motors.size())
		return;

	motors[keycode].m_value = value;
}

#endif
