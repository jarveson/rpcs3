#ifdef _WIN32
#include "mm_joystick_handler.h"

mm_joystick_handler::mm_joystick_handler() : PadHandlerBase(pad_handler::mm, "mmjoy", "Joystick #")
{
	Init();
}

mm_joystick_handler::~mm_joystick_handler()
{
}

void mm_joystick_handler::init_config(pad_config* cfg, const std::string& name)
{
	// Set this profile's save location
	cfg->cfg_name = name;

	// Set default button mapping
	cfg->ls_left.def  = axis_list.at(MMJoyKeyCodes::joy_x) + "-";
	cfg->ls_down.def  = axis_list.at(MMJoyKeyCodes::joy_y) + "-";
	cfg->ls_right.def = axis_list.at(MMJoyKeyCodes::joy_x) + "+";
	cfg->ls_up.def    = axis_list.at(MMJoyKeyCodes::joy_y) + "+";
	cfg->rs_left.def  = axis_list.at(MMJoyKeyCodes::joy_z) + "-";
	cfg->rs_down.def  = axis_list.at(MMJoyKeyCodes::joy_r) + "-";
	cfg->rs_right.def = axis_list.at(MMJoyKeyCodes::joy_z) + "+";
	cfg->rs_up.def    = axis_list.at(MMJoyKeyCodes::joy_r) + "+";
	cfg->start.def    = button_list.at(MMJoyKeyCodes::Button9);
	cfg->select.def   = button_list.at(MMJoyKeyCodes::Button10);
	cfg->ps.def       = button_list.at(MMJoyKeyCodes::Button17);
	cfg->square.def   = button_list.at(MMJoyKeyCodes::Button4);
	cfg->cross.def    = button_list.at(MMJoyKeyCodes::Button3);
	cfg->circle.def   = button_list.at(MMJoyKeyCodes::Button2);
	cfg->triangle.def = button_list.at(MMJoyKeyCodes::Button1);
	cfg->left.def     = button_list.at(MMJoyKeyCodes::Pov_left);
	cfg->down.def     = button_list.at(MMJoyKeyCodes::Pov_down);
	cfg->right.def    = button_list.at(MMJoyKeyCodes::Pov_right);
	cfg->up.def       = button_list.at(MMJoyKeyCodes::Pov_up);
	cfg->r1.def       = button_list.at(MMJoyKeyCodes::Button8);
	cfg->r2.def       = button_list.at(MMJoyKeyCodes::Button6);
	cfg->r3.def       = button_list.at(MMJoyKeyCodes::Button12);
	cfg->l1.def       = button_list.at(MMJoyKeyCodes::Button7);
	cfg->l2.def       = button_list.at(MMJoyKeyCodes::Button5);
	cfg->l3.def       = button_list.at(MMJoyKeyCodes::Button11);

	// Set default misc variables
	cfg->lstickdeadzone.def    = 40; // between 0 and 255
	cfg->rstickdeadzone.def    = 40; // between 0 and 255
	cfg->ltriggerthreshold.def = 0; // between 0 and 255
	cfg->rtriggerthreshold.def = 0; // between 0 and 255
	cfg->padsquircling.def     = 8000;

	// apply defaults
	cfg->from_default();
}

bool mm_joystick_handler::Init()
{
	if (is_init)
		return true;

	m_devices.clear();
	u32 supported_joysticks = joyGetNumDevs();

	if (supported_joysticks <= 0)
	{
		LOG_ERROR(GENERAL, "mmjoy: Driver doesn't support Joysticks");
		return false;
	}

	LOG_NOTICE(GENERAL, "mmjoy: Driver supports %u joysticks", supported_joysticks);

	for (u32 i = 0; i < supported_joysticks; i++)
	{
		std::unique_ptr<MMJOYDevice> dev = std::make_unique<MMJOYDevice>();

		if (GetMMJOYDevice(i, *dev) == false)
			continue;

		m_devices.emplace(i, std::move(dev));
	}

	is_init = true;
	return true;
}

std::vector<std::string> mm_joystick_handler::ListDevices()
{
	std::lock_guard<std::mutex> lock(handlerLock);
	std::vector<std::string> devices;

	if (!Init())
		return devices;

	devices.reserve(m_devices.size());
	for (auto& dev : m_devices)
	{
		if (dev.second->last_conn_status)
			devices.emplace_back(dev.second->device_name);
	}

	return devices;
}

void mm_joystick_handler::ThreadProc()
{
	std::lock_guard<std::mutex> lock(handlerLock);

	for (auto& binding : bindings)
	{
		auto dev = binding.first;
		auto pad = binding.second.get();
		MMRESULT status = joyGetPosEx(dev->device_id, &dev->device_info);
		std::lock_guard<std::mutex> lock2(pad->pad_lock);

		if (status != JOYERR_NOERROR)
		{
			if (dev->last_conn_status == true)
			{
				LOG_ERROR(HLE, "MMJOY Device %d disconnected.", dev->device_id);
				pad->connected = false;
				dev->last_conn_status = false;
			}
			continue;
		}

		if (dev->last_conn_status == false)
		{
			if (GetMMJOYDevice(dev->device_id, *dev) == false)
				continue;
			LOG_SUCCESS(HLE, "MMJOY Device %d reconnected.", dev->device_id);
			pad->connected = true;
			dev->last_conn_status = true;
			
			// reset values in case the stick changed 
			for (auto& btn : pad->m_buttons)
				btn.m_value = 0;
			for (auto& stick : pad->m_sticks) {
				stick.m_valueLow = 0.f;
				stick.m_valueHigh = 0.f;
			}
		}

		const JOYCAPS& js_caps = dev->device_caps;
		const JOYINFOEX& js_info = dev->device_info;

		for (u32 i = 0; i < 32; ++i)
			pad->m_buttons[i].m_value = (js_info.dwButtons & (1 << i)) ? 255 : 0;

		if (js_caps.wCaps & JOYCAPS_HASPOV)
		{
			if ((js_caps.wCaps & JOYCAPS_POVCTS) && (js_info.dwPOV != JOY_POVCENTERED))
			{
				auto emplacePOVs = [&](float val, u32 pov)
				{
					if (val < 0)
					{
						pad->m_sticks[pov].m_valueLow = std::abs(val);
					}
					else
					{
						pad->m_sticks[pov].m_valueHigh = val;
					}
				};

				float rad = static_cast<float>(js_info.dwPOV / 100 * acos(-1) / 180);
				emplacePOVs(std::cosf(rad), 13);
				emplacePOVs(std::sinf(rad), 12);
			}
			else if (js_caps.wCaps & JOYCAPS_POV4DIR)
			{
				int val = static_cast<int>(js_info.dwPOV);

				auto emplacePOV = [&](int pov, int idx)
				{
					int cw = pov + 4500, ccw = pov - 4500;
					bool pressed = (val == pov) || (val == cw) || (ccw < 0 ? val == 36000 - std::abs(ccw) : val == ccw);
					pad->m_buttons[idx].m_value = pressed ? 255 : 0;
				};

				emplacePOV(JOY_POVFORWARD, MMJoyKeyCodes::Pov_up);
				emplacePOV(JOY_POVRIGHT, MMJoyKeyCodes::Pov_right);
				emplacePOV(JOY_POVBACKWARD, MMJoyKeyCodes::Pov_down);
				emplacePOV(JOY_POVLEFT, MMJoyKeyCodes::Pov_left);
			}
		}

		auto add_axis_value = [&](DWORD axis, UINT min, UINT max, u32 idx)
		{
			const UINT half = (max - min) / 2;

			if (axis < half)
			{
				pad->m_sticks[idx].m_valueHigh = 0;
				pad->m_sticks[idx].m_valueLow = std::abs((half - axis) / float(half));
			}
			else
			{
				pad->m_sticks[idx].m_valueHigh = (axis - half) / float(half);
				pad->m_sticks[idx].m_valueLow = 0;
			}
		};

		add_axis_value(js_info.dwXpos, js_caps.wXmin, js_caps.wXmax, 0);
		add_axis_value(js_info.dwYpos, js_caps.wYmin, js_caps.wYmax, 1);

		if (js_caps.wCaps & JOYCAPS_HASZ)
			add_axis_value(js_info.dwZpos, js_caps.wZmin, js_caps.wZmax, 2);

		if (js_caps.wCaps & JOYCAPS_HASR)
			add_axis_value(js_info.dwRpos, js_caps.wRmin, js_caps.wRmax, 3);

		if (js_caps.wCaps & JOYCAPS_HASU)
			add_axis_value(js_info.dwUpos, js_caps.wUmin, js_caps.wUmax, 4);

		if (js_caps.wCaps & JOYCAPS_HASV)
			add_axis_value(js_info.dwVpos, js_caps.wVmin, js_caps.wVmax, 5);
	}
}

bool mm_joystick_handler::GetMMJOYDevice(int index, MMJOYDevice& dev)
{
	JOYINFOEX js_info;
	JOYCAPS js_caps;
	js_info.dwSize = sizeof(js_info);
	js_info.dwFlags = JOY_RETURNALL;
	joyGetDevCaps(index, &js_caps, sizeof(js_caps));

	if (joyGetPosEx(index, &js_info) != JOYERR_NOERROR)
		return false;

	char drv[32];
	wcstombs(drv, js_caps.szPname, 31);

	LOG_NOTICE(GENERAL, "Joystick nr.%d found. Driver: %s", index, drv);

	dev.device_id = index;
	dev.device_name = m_device_name_prefix + std::to_string(index);
	dev.device_info = js_info;
	dev.device_caps = js_caps;

	return true;
}

s32 mm_joystick_handler::EnableGetDevice(const std::string& deviceName) {
	std::lock_guard<std::mutex> lock(handlerLock);
	auto it = std::find_if(m_devices.begin(), m_devices.end(), [&](const auto& d) {return d.second->device_name == deviceName; });
	if (it == m_devices.end())
		return -1;

	u32 device_number = it->first;
	// todo: refresh or add device to watch list if not found

	auto bit = std::find_if(bindings.begin(), bindings.end(), [device_number](const auto& d) {return d.first->device_id == device_number; });
	if (bit != bindings.end())
		return device_number;

	std::shared_ptr<Pad> pad = std::make_shared<Pad>();

	pad->m_buttons.reserve(button_list.size());
	for (u32 i = 0; i < MMJoyKeyCodes::joy_x; ++i)
		pad->m_buttons.emplace_back(i, button_list.at(i));

	pad->m_buttons.reserve(axis_list.size());
	for (u32 i = MMJoyKeyCodes::joy_x; i < MMJoyKeyCodes::KeyCodeCount; ++i)
		pad->m_sticks.emplace_back(i, axis_list.at(i));

	bindings.emplace_back(it->second.get(), std::move(pad));
	return device_number;
}

std::shared_ptr<Pad> mm_joystick_handler::GetDeviceData(u32 device_number) {
	std::lock_guard<std::mutex> lock(handlerLock);
    auto it = std::find_if(bindings.begin(), bindings.end(), [device_number](const auto& d) {return d.first->device_id == device_number; });
    if (it == bindings.end())
        return nullptr;
    else return it->second;
}

void mm_joystick_handler::DisableDevice(u32 deviceNumber) {
	std::lock_guard<std::mutex> lock(handlerLock);
    auto it = std::find_if(bindings.begin(), bindings.end(), [deviceNumber](const auto& d) {return d.first->device_id == deviceNumber; });
    if (it == bindings.end())
        return;

    bindings.erase(it);
}

bool mm_joystick_handler::IsDeviceConnected(u32 deviceNumber) {
	std::lock_guard<std::mutex> lock(handlerLock);
	auto it = std::find_if(bindings.begin(), bindings.end(), [deviceNumber](const auto& d) {return d.first->device_id == deviceNumber; });
	if (it == bindings.end())
		return false;

	return it->first->last_conn_status;
}

#endif
