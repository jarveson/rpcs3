
#ifdef _MSC_VER
#include "xinput_pad_handler.h"

xinput_pad_handler::xinput_pad_handler() : PadHandlerBase(pad_handler::xinput, "XInput", "XInput Pad #")
{
	Init();
}

xinput_pad_handler::~xinput_pad_handler()
{
	if (library)
	{
		FreeLibrary(library);
		library = nullptr;
		xinputGetState = nullptr;
		xinputEnable = nullptr;
		xinputGetBatteryInformation = nullptr;
	}
}

void xinput_pad_handler::init_config(pad_config* cfg, const std::string& name)
{
	// Set this profile's save location
	cfg->cfg_name = name;

	// Set default button mapping
	cfg->ls_left.def  = axis_list.at(XInputKeyCodes::LSX) + "-";
	cfg->ls_down.def  = axis_list.at(XInputKeyCodes::LSY) + "-";
	cfg->ls_right.def = axis_list.at(XInputKeyCodes::LSX) + "+";
	cfg->ls_up.def    = axis_list.at(XInputKeyCodes::LSY) + "+";
	cfg->rs_left.def  = axis_list.at(XInputKeyCodes::RSX) + "-";
	cfg->rs_down.def  = axis_list.at(XInputKeyCodes::RSY) + "-";
	cfg->rs_right.def = axis_list.at(XInputKeyCodes::RSX) + "+";
	cfg->rs_up.def    = axis_list.at(XInputKeyCodes::RSY) + "+";
	cfg->start.def    = button_list.at(XInputKeyCodes::Start);
	cfg->select.def   = button_list.at(XInputKeyCodes::Back);
	cfg->ps.def       = button_list.at(XInputKeyCodes::Guide);
	cfg->square.def   = button_list.at(XInputKeyCodes::X);
	cfg->cross.def    = button_list.at(XInputKeyCodes::A);
	cfg->circle.def   = button_list.at(XInputKeyCodes::B);
	cfg->triangle.def = button_list.at(XInputKeyCodes::Y);
	cfg->left.def     = button_list.at(XInputKeyCodes::Left);
	cfg->down.def     = button_list.at(XInputKeyCodes::Down);
	cfg->right.def    = button_list.at(XInputKeyCodes::Right);
	cfg->up.def       = button_list.at(XInputKeyCodes::Up);
	cfg->r1.def       = button_list.at(XInputKeyCodes::RB);
	cfg->r2.def       = button_list.at(XInputKeyCodes::RT);
	cfg->r3.def       = button_list.at(XInputKeyCodes::RS);
	cfg->l1.def       = button_list.at(XInputKeyCodes::LB);
	cfg->l2.def       = button_list.at(XInputKeyCodes::LT);
	cfg->l3.def       = button_list.at(XInputKeyCodes::LS);

	// Set default misc variables
	cfg->lstickdeadzone.def    = static_cast<u16>((XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE / 32767.f) * 255);  
	cfg->rstickdeadzone.def    = static_cast<u16>((XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE / 32767.f) * 255);
	cfg->ltriggerthreshold.def = 0;
	cfg->rtriggerthreshold.def = 0;
	cfg->padsquircling.def     = 8000;

	// apply defaults
	cfg->from_default();
}

int xinput_pad_handler::GetDeviceNumber(const std::string& padId)
{
	size_t pos = padId.find(m_device_name_prefix);
	if (pos == std::string::npos)
		return -1;

	int device_number = std::stoul(padId.substr(pos + m_device_name_prefix.size()));
	if (device_number >= XUSER_MAX_COUNT)
		return -1;

	return device_number;
}

std::array<u16, xinput_pad_handler::XInputKeyCodes::KeyCodeCount> xinput_pad_handler::GetButtonValues(const XINPUT_STATE& state)
{
	std::array<u16, xinput_pad_handler::XInputKeyCodes::KeyCodeCount> values;

	// Triggers
	values[XInputKeyCodes::LT] = state.Gamepad.bLeftTrigger;
	values[XInputKeyCodes::RT] = state.Gamepad.bRightTrigger;

	// Stick
	values[XInputKeyCodes::LSX] = state.Gamepad.sThumbLX;
	values[XInputKeyCodes::LSY] = state.Gamepad.sThumbLY;
	values[XInputKeyCodes::RSX] = state.Gamepad.sThumbRX;
	values[XInputKeyCodes::RSY] = state.Gamepad.sThumbRY;

	// Buttons
	WORD buttons = state.Gamepad.wButtons;

	// A, B, X, Y
	values[XInputKeyCodes::A] = buttons & XINPUT_GAMEPAD_A ? 255 : 0;
	values[XInputKeyCodes::B] = buttons & XINPUT_GAMEPAD_B ? 255 : 0;
	values[XInputKeyCodes::X] = buttons & XINPUT_GAMEPAD_X ? 255 : 0;
	values[XInputKeyCodes::Y] = buttons & XINPUT_GAMEPAD_Y ? 255 : 0;

	// D-Pad
	values[XInputKeyCodes::Left]  = buttons & XINPUT_GAMEPAD_DPAD_LEFT ? 255 : 0;
	values[XInputKeyCodes::Right] = buttons & XINPUT_GAMEPAD_DPAD_RIGHT ? 255 : 0;
	values[XInputKeyCodes::Up]    = buttons & XINPUT_GAMEPAD_DPAD_UP ? 255 : 0;
	values[XInputKeyCodes::Down]  = buttons & XINPUT_GAMEPAD_DPAD_DOWN ? 255 : 0;

	// LB, RB, LS, RS
	values[XInputKeyCodes::LB] = buttons & XINPUT_GAMEPAD_LEFT_SHOULDER ? 255 : 0;
	values[XInputKeyCodes::RB] = buttons & XINPUT_GAMEPAD_RIGHT_SHOULDER ? 255 : 0;
	values[XInputKeyCodes::LS] = buttons & XINPUT_GAMEPAD_LEFT_THUMB ? 255 : 0;
	values[XInputKeyCodes::RS] = buttons & XINPUT_GAMEPAD_RIGHT_THUMB ? 255 : 0;

	// Start, Back, Guide
	values[XInputKeyCodes::Start] = buttons & XINPUT_GAMEPAD_START ? 255 : 0;
	values[XInputKeyCodes::Back]  = buttons & XINPUT_GAMEPAD_BACK ? 255 : 0;
	values[XInputKeyCodes::Guide] = buttons & XINPUT_INFO::GUIDE_BUTTON ? 255 : 0;

	return values;
}

bool xinput_pad_handler::Init()
{
	if (is_init)
		return true;

	for (auto it : XINPUT_INFO::LIBRARY_FILENAMES)
	{
		library = LoadLibrary(it);
		if (library)
		{
			xinputEnable = reinterpret_cast<PFN_XINPUTENABLE>(GetProcAddress(library, "XInputEnable"));
			xinputGetState = reinterpret_cast<PFN_XINPUTGETSTATE>(GetProcAddress(library, reinterpret_cast<LPCSTR>(100)));
			if (!xinputGetState)
				xinputGetState = reinterpret_cast<PFN_XINPUTGETSTATE>(GetProcAddress(library, "XInputGetState"));

			xinputSetState = reinterpret_cast<PFN_XINPUTSETSTATE>(GetProcAddress(library, "XInputSetState"));
			xinputGetBatteryInformation = reinterpret_cast<PFN_XINPUTGETBATTERYINFORMATION>(GetProcAddress(library, "XInputGetBatteryInformation"));

			if (xinputEnable && xinputGetState && xinputSetState && xinputGetBatteryInformation)
			{
				is_init = true;
				break;
			}

			FreeLibrary(library);
			library = nullptr;
			xinputEnable = nullptr;
			xinputGetState = nullptr;
			xinputGetBatteryInformation = nullptr;
		}
	}

	if (!is_init)
		return false;

	return true;
}

void xinput_pad_handler::ThreadProc()
{
	if (!Init())
		return;

	std::lock_guard<std::mutex> lock(handlerLock);
	for (auto &bind : bindings)
	{
		auto dev = bind.first.get();
		auto padnum = dev->deviceNumber;
		auto pad = bind.second.get();
		std::lock_guard<std::mutex> lock2(pad->pad_lock);

		XINPUT_STATE state;
		DWORD result = (*xinputGetState)(padnum, &state);

		switch (result)
		{
		case ERROR_DEVICE_NOT_CONNECTED:
			if (dev->last_conn_status == true)
			{
				LOG_ERROR(HLE, "XInput device %d disconnected", padnum);
				dev->last_conn_status = false;
			}
			pad->connected = false;
			continue;

		case ERROR_SUCCESS:
			if (dev->last_conn_status == false)
			{
				LOG_SUCCESS(HLE, "XInput device %d reconnected", padnum);
				dev->last_conn_status = true;
			}
			pad->connected = true;

			std::array<u16, XInputKeyCodes::KeyCodeCount> button_values = GetButtonValues(state);

			for (const auto& btn : button_list)
				pad->m_buttons[btn.first].m_value = button_values[btn.first];

			for (const auto& axi : axis_list) {
				u32 idx = axi.first - XInputKeyCodes::LSX;
				s16 val = static_cast<s32>(button_values[axi.first]);
				bool neg = val < 0;
				float convertedVal = std::min(std::abs(val / 32767.f), 1.f);
				pad->m_sticks[idx].m_valueLow = neg ? convertedVal : 0;
				pad->m_sticks[idx].m_valueHigh = neg ? 0 : convertedVal;
			}

			// Receive Battery Info. If device is not on cable, get battery level, else assume full
			XINPUT_BATTERY_INFORMATION battery_info;
			(*xinputGetBatteryInformation)(padnum, BATTERY_DEVTYPE_GAMEPAD, &battery_info);
			pad->m_cable_state = battery_info.BatteryType == BATTERY_TYPE_WIRED ? 1 : 0;
			pad->m_battery_level = pad->m_cable_state ? BATTERY_LEVEL_FULL : battery_info.BatteryLevel;

			// The left motor is the low-frequency rumble motor. The right motor is the high-frequency rumble motor.
			// The two motors are not the same, and they create different vibration effects. Values range between 0 to 65535.
			int speed_large = pad->m_vibrateMotors[0].m_value * 257;
			int speed_small = pad->m_vibrateMotors[1].m_value * 257;

			dev->newVibrateData = dev->newVibrateData || dev->largeVibrate != speed_large || dev->smallVibrate != speed_small;

			dev->largeVibrate = speed_large;
			dev->smallVibrate = speed_small;

			// XBox One Controller can't handle faster vibration updates than ~10ms. Elite is even worse. So I'll use 20ms to be on the safe side. No lag was noticable.
			if (dev->newVibrateData && (clock() - dev->last_vibration > 20))
			{
				XINPUT_VIBRATION vibrate;
				vibrate.wLeftMotorSpeed = speed_large;
				vibrate.wRightMotorSpeed = speed_small;

				if ((*xinputSetState)(padnum, &vibrate) == ERROR_SUCCESS)
				{
					dev->newVibrateData = false;
					dev->last_vibration = clock();
				}
			}

			break;
		}
	}
}

void xinput_pad_handler::SetVibrate(u32 deviceNumber, u32 keycode, u32 value) {
	std::lock_guard<std::mutex> lock(handlerLock);
	if (deviceNumber > bindings.size())
		return;

	auto &motors = bindings[deviceNumber].second->m_vibrateMotors;
	if (keycode > motors.size())
		return;

	motors[keycode].m_value = value;
}

std::shared_ptr<Pad> xinput_pad_handler::GetDeviceData(u32 deviceNumber) {
	std::lock_guard<std::mutex> lock(handlerLock);
	if (deviceNumber > bindings.size())
		return nullptr;
	return bindings[deviceNumber].second;
}

std::vector<std::string> xinput_pad_handler::ListDevices()
{
	std::vector<std::string> xinput_pads_list;

	if (!Init())
		return xinput_pads_list;

	for (DWORD i = 0; i < XUSER_MAX_COUNT; i++)
	{
		XINPUT_STATE state;
		DWORD result = (*xinputGetState)(i, &state);
		if (result == ERROR_SUCCESS)
			xinput_pads_list.push_back(m_device_name_prefix + std::to_string(i));
	}
	return xinput_pads_list;
}

s32 xinput_pad_handler::EnableGetDevice(const std::string& device) {
	std::lock_guard<std::mutex> lock(handlerLock);
	//Convert device string to u32 representing xinput device number
	int device_number = GetDeviceNumber(device);
	if (device_number < 0)
		return -1;

	auto it = std::find_if(bindings.begin(), bindings.end(), [device_number](const auto& d) {return d.first->deviceNumber == device_number; });
	if (it != bindings.end())
		return device_number;

	std::unique_ptr<XInputDevice> x_device = std::make_unique<XInputDevice>();
	x_device->deviceNumber = static_cast<u32>(device_number);

	std::shared_ptr<Pad> pad = std::make_shared<Pad>();

	pad->m_buttons.reserve(button_list.size());
	for (u32 i = 0; i < XInputKeyCodes::LSX; ++i)
		pad->m_buttons.emplace_back(i, button_list.at(i));

	pad->m_buttons.reserve(axis_list.size());
	for (u32 i = LSX; i < XInputKeyCodes::KeyCodeCount; ++i)
		pad->m_sticks.emplace_back(i, axis_list.at(i));

	pad->m_vibrateMotors.emplace_back(true, 0);
	pad->m_vibrateMotors.emplace_back(false, 0);

	bindings.emplace_back(std::move(x_device), std::move(pad));
	return device_number;
}

void xinput_pad_handler::DisableDevice(u32 deviceNumber) {
	std::lock_guard<std::mutex> lock(handlerLock);
    auto it = std::find_if(bindings.begin(), bindings.end(), [deviceNumber](const auto& d) {return d.first->deviceNumber == deviceNumber; });
    if (it == bindings.end())
        return;

    bindings.erase(it);
}

bool xinput_pad_handler::IsDeviceConnected(u32 deviceNumber) {
	std::lock_guard<std::mutex> lock(handlerLock);
	auto it = std::find_if(bindings.begin(), bindings.end(), [deviceNumber](const auto& d) {return d.first->deviceNumber == deviceNumber; });
	if (it == bindings.end())
		return false;

	return it->first->last_conn_status;
}

#endif
