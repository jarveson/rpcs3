#include "stdafx.h"
#include "EmulatedPad.h"
#include "PadHandler.h"

#include <algorithm>
#define _USE_MATH_DEFINES
#include <math.h>

namespace {
    constexpr s32 thumb_min = 0;
    constexpr s32 thumb_max = 255;
    constexpr s32 vibration_min = 0;
    constexpr s32 vibration_max = 255;
	constexpr u16 axis_for_button_threshold = 45;
	constexpr u16 trigger_min = 0;
	constexpr u16 trigger_max = 255;
}

EmulatedPad::keycode_cfg EmulatedPad::FindKeyCode(const std::unordered_map<std::string, u32>& btns, const std::unordered_map<std::string, u32>& axis, const cfg::string& name) {
    std::string def = name.def;
    std::string nam = name.to_string();

	keycode_type type = keycode_type::button;
	size_t sfind;

	if ((sfind = nam.find("-+")) != std::string::npos) {
		nam = nam.erase(sfind, 2);
		type = keycode_type::axis_neg_pos;
	}
	else if ((sfind = nam.find("+-"))!= std::string::npos) {
		nam = nam.erase(sfind, 2);
		type = keycode_type::axis_pos_neg;
	}
	else if ((sfind = nam.find("+")) != std::string::npos) {
		nam = nam.erase(sfind, 1);
		type = keycode_type::axis_pos;
	}
	else if ((sfind = nam.find("-")) != std::string::npos) {
		nam = nam.erase(sfind, 1);
		type = keycode_type::axis_neg;
	}

	switch (type) {
	case keycode_type::axis_neg:
	case keycode_type::axis_pos:
	case keycode_type::axis_pos_neg:
	case keycode_type::axis_neg_pos: {
		auto ait = axis.find(nam);
		if (ait != axis.end())
			return{ ait->second, type };

		auto fait = axis.find(def);
		if (fait != axis.end()) {
			return{ fait->second, type };
		}
	}

	case keycode_type::button: 
	default: {
		auto it = btns.find(nam);
		if (it != btns.end())
			return{ it->second, keycode_type::button };

		auto fit = btns.find(def);
		if (fit != btns.end()) {
			return{ fit->second, keycode_type::button };
		}
	}
	}

	u32 def_code = 0;
	LOG_ERROR(HLE, "FindKeyCode failed for [name = %s] returned with [def_code = %d] for [def = %s]", nam, def_code, def);
	return{ def_code, keycode_type::button };
}

std::tuple<u16, u16> EmulatedPad::NormalizeStickDeadzone(s32 inX, s32 inY, u32 deadzone) {
    const f32 dzRange = deadzone / f32((std::abs(thumb_max) + std::abs(thumb_min)));

    f32 X = inX / 255.0f;
    f32 Y = inY / 255.0f;

    if (dzRange > 0.f) {
        const f32 mag = std::min(sqrtf(X*X + Y*Y), 1.f);

        if (mag <= 0) {
            return std::tuple<u16, u16>(ConvertAxis(X), ConvertAxis(Y));
        }

        if (mag > dzRange) {
            f32 pos = lerp(0.13f, 1.f, (mag - dzRange) / (1 - dzRange));
            f32 scale = pos / mag;
            X = X * scale;
            Y = Y * scale;
        }
        else {
            f32 pos = lerp(0.f, 0.13f, mag / dzRange);
            f32 scale = pos / mag;
            X = X * scale;
            Y = Y * scale;
        }
    }
    return std::tuple<u16, u16>(ConvertAxis(X), ConvertAxis(Y));
}

std::tuple<u16, u16> EmulatedPad::ConvertToSquirclePoint(u16 inX, u16 inY, int squircle_factor) {
    // convert inX and Y to a (-1, 1) vector;
    const f32 x = ((f32)inX - 127.5f) / 127.5f;
    const f32 y = ((f32)inY - 127.5f) / 127.5f;

    // compute angle and len of given point to be used for squircle radius
    const f32 angle = std::atan2(y, x);
    const f32 r = std::sqrt(std::pow(x, 2.f) + std::pow(y, 2.f));

    // now find len/point on the given squircle from our current angle and radius in polar coords
    // https://thatsmaths.com/2016/07/14/squircles/
    const f32 newLen = (1 + std::pow(std::sin(2 * angle), 2.f) / (f32(squircle_factor) / 1000.f)) * r;

    // we now have len and angle, convert to cartisian
    const int newX = Clamp0To255(((newLen * std::cos(angle)) + 1) * 127.5f);
    const int newY = Clamp0To255(((newLen * std::sin(angle)) + 1) * 127.5f);
    return std::tuple<u16, u16>(newX, newY);
}

std::tuple<u16, u16> EmulatedPad::ConvertToSquarePoint(u16 inX, u16 inY, u32 innerRoundness) {
	// convert inX and Y to a (-1, 1) vector;
	const f32 x = (inX - 127) / 127.f;
	const f32 y = ((inY - 127) / 127.f) * -1;

	f32 outX, outY;
	const f32 piOver4 = static_cast<f32>(M_PI / 4);
	const f32 angle = static_cast<f32>(std::atan2(y, x) + M_PI);
	// x+ wall
	if (angle <= piOver4 || angle > 7 * piOver4) {
		outX = x * (f32)(1 / std::cos(angle));
		outY = y * (f32)(1 / std::cos(angle));
	}
	// y+ wall
	else if (angle > piOver4 && angle <= 3 * piOver4) {
		outX = x * (f32)(1 / std::sin(angle));
		outY = y * (f32)(1 / std::sin(angle));
	}
	// x- wall
	else if (angle > 3 * piOver4 && angle <= 5 * piOver4) {
		outX = x * (f32)(-1 / std::cos(angle));
		outY = y * (f32)(-1 / std::cos(angle));
	}
	// y- wall
	else if (angle > 5 * piOver4 && angle <= 7 * piOver4) {
		outX = x * (f32)(-1 / std::sin(angle));
		outY = y * (f32)(-1 / std::sin(angle));
	}

	if (innerRoundness == 0)
		return std::tuple<u16, u16>(Clamp0To255((outX + 1) * 127.f), Clamp0To255(((outY * -1) + 1) * 127.f));

	const f32 len = std::sqrt(std::pow(x, 2) + std::pow(y, 2));
	const f32 factor = static_cast<f32>(std::pow(len, innerRoundness));

	outX = (1 - factor) * x + factor * outX;
	outY = (1 - factor) * y + factor * outY;

	return std::tuple<u16, u16>(Clamp0To255((outX + 1) * 127.f), Clamp0To255(((outY * -1) + 1) * 127.f));
}

EmulatedPad::EmulatedPad(u32 devid, const pad_config& cfg, std::shared_ptr<PadHandlerBase> padHandle) : handler(std::move(padHandle)), device_id(devid)
{
    // grab first data to create mapping
	pad = handler->GetDeviceData(devid);
	if (pad == nullptr) {
		//LOG_ERROR(HLE,"null pad data from GetDeviceData %d", devid);
		return;
	}

	std::lock_guard<std::mutex> lock(pad->pad_lock);

	// create full keycode map
	for (const auto& btn : pad->m_buttons) {
		pad_button_map.emplace(btn.m_name, btn.m_keyCode);
	}

	for (const auto& axi : pad->m_sticks) {
		pad_axis_map.emplace(axi.m_name, axi.m_keyCode);
	}

	RebindController(cfg);
}

u16 EmulatedPad::TranslateKeycode(const Pad& pad, const EmulatedPad::keycode_cfg& keyCode) {
	switch (keyCode.type) {
	case keycode_type::axis_neg: {
		auto it = std::find_if(pad.m_sticks.cbegin(), pad.m_sticks.cend(), [=](const AnalogStick& s) {return s.m_keyCode == keyCode.keycode; });
		if (it == pad.m_sticks.cend())
			return 0;
		return static_cast<u16>(it->m_valueLow * 255);
	}
	case keycode_type::axis_pos: {
		auto it = std::find_if(pad.m_sticks.cbegin(), pad.m_sticks.cend(), [=](const AnalogStick& s) {return s.m_keyCode == keyCode.keycode; });
		if (it == pad.m_sticks.cend())
			return 0;
		return static_cast<u16>(it->m_valueHigh * 255);
	}
	case keycode_type::axis_neg_pos: {
		auto it = std::find_if(pad.m_sticks.cbegin(), pad.m_sticks.cend(), [=](const AnalogStick& s) {return s.m_keyCode == keyCode.keycode; });
		if (it == pad.m_sticks.cend())
			return 0;
		return static_cast<u16>(((1.f - it->m_valueLow) * 127.5f) + (it->m_valueHigh * 127.5f));
	}
	case keycode_type::axis_pos_neg: {
		auto it = std::find_if(pad.m_sticks.cbegin(), pad.m_sticks.cend(), [=](const AnalogStick& s) {return s.m_keyCode == keyCode.keycode; });
		if (it == pad.m_sticks.cend())
			return 0;
		return static_cast<u16>(((1.f - it->m_valueHigh) * 127.5f) + (it->m_valueLow * 127.5f));
	}
	case keycode_type::button:
	default: {
		auto it = std::find_if(pad.m_buttons.cbegin(), pad.m_buttons.cend(), [=](const Button& b) {return b.m_keyCode == keyCode.keycode; });
		if (it == pad.m_buttons.cend())
			return 0;
		return it->m_value;
	}
	}

	// cant happen
	return 0;
}

void EmulatedPad::RebindController(const pad_config& cfg) {
	lstickdeadzone = cfg.lstickdeadzone;
	rstickdeadzone = cfg.rstickdeadzone;
	ltriggerthreshold = cfg.ltriggerthreshold;
	rtriggerthreshold = cfg.rtriggerthreshold;
	padsquircling = cfg.padsquircling;

	enable_vibrate_large = (bool)cfg.enable_vibration_motor_large;
	enable_vibrate_small = (bool)cfg.enable_vibration_motor_small;
	swap_vibrate = (bool)cfg.switch_vibration_motors;


	// todo: sensor/vibrate mapping

	// now create our 'master' mapping for btns/axis
	button_config.clear();
	button_config.reserve(16);
	button_config.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL1, FindKeyCode(pad_button_map, pad_axis_map, cfg.up), CELL_PAD_CTRL_UP);
	button_config.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL1, FindKeyCode(pad_button_map, pad_axis_map, cfg.down), CELL_PAD_CTRL_DOWN);
	button_config.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL1, FindKeyCode(pad_button_map, pad_axis_map, cfg.left), CELL_PAD_CTRL_LEFT);
	button_config.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL1, FindKeyCode(pad_button_map, pad_axis_map, cfg.right), CELL_PAD_CTRL_RIGHT);
	button_config.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL1, FindKeyCode(pad_button_map, pad_axis_map, cfg.start), CELL_PAD_CTRL_START);
	button_config.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL1, FindKeyCode(pad_button_map, pad_axis_map, cfg.select), CELL_PAD_CTRL_SELECT);
	button_config.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL1, FindKeyCode(pad_button_map, pad_axis_map, cfg.l3), CELL_PAD_CTRL_L3);
	button_config.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL1, FindKeyCode(pad_button_map, pad_axis_map, cfg.r3), CELL_PAD_CTRL_R3);
	button_config.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, FindKeyCode(pad_button_map, pad_axis_map, cfg.l1), CELL_PAD_CTRL_L1);
	button_config.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, FindKeyCode(pad_button_map, pad_axis_map, cfg.r1), CELL_PAD_CTRL_R1);
	button_config.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, FindKeyCode(pad_button_map, pad_axis_map, cfg.ps), CELL_PAD_CTRL_PS);
	button_config.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, FindKeyCode(pad_button_map, pad_axis_map, cfg.cross), CELL_PAD_CTRL_CROSS);
	button_config.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, FindKeyCode(pad_button_map, pad_axis_map, cfg.circle), CELL_PAD_CTRL_CIRCLE);
	button_config.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, FindKeyCode(pad_button_map, pad_axis_map, cfg.square), CELL_PAD_CTRL_SQUARE);
	button_config.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, FindKeyCode(pad_button_map, pad_axis_map, cfg.triangle), CELL_PAD_CTRL_TRIANGLE);
	button_config.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, FindKeyCode(pad_button_map, pad_axis_map, cfg.l2), CELL_PAD_CTRL_L2);
	button_config.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, FindKeyCode(pad_button_map, pad_axis_map, cfg.r2), CELL_PAD_CTRL_R2);
	button_config.emplace_back(CELL_PAD_BTN_OFFSET_DIGITAL2, keycode_cfg{}, 0x0); // Reserved as fail safe for any not found keys

	// order of this array matters for ease of calculations later
	axis_config[0] = { CELL_PAD_BTN_OFFSET_ANALOG_LEFT_X,  FindKeyCode(pad_button_map, pad_axis_map, cfg.ls_left), FindKeyCode(pad_button_map, pad_axis_map, cfg.ls_right) };
	axis_config[1] = { CELL_PAD_BTN_OFFSET_ANALOG_LEFT_Y,  FindKeyCode(pad_button_map, pad_axis_map, cfg.ls_down), FindKeyCode(pad_button_map, pad_axis_map, cfg.ls_up) };
	axis_config[2] = { CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_X, FindKeyCode(pad_button_map, pad_axis_map, cfg.rs_left), FindKeyCode(pad_button_map, pad_axis_map, cfg.rs_right) };
	axis_config[3] = { CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_Y, FindKeyCode(pad_button_map, pad_axis_map, cfg.rs_down), FindKeyCode(pad_button_map, pad_axis_map, cfg.rs_up) };
}

u16 EmulatedPad::NormalizeTriggerInput(u16 value, int threshold)
{
	if (value <= threshold || threshold >= trigger_max)
	{
		return static_cast<u16>(0);
	}
	else if (threshold <= trigger_min)
	{
		return value;
	}
	else
	{
		return (u16)(f32(trigger_max) * f32(value - threshold) / f32(trigger_max - threshold));
	}
}

PadDataBuffer EmulatedPad::GetPadData() {
	PadDataBuffer padData;
	if (pad == nullptr)
		return padData;

	std::lock_guard<std::mutex> lock(pad->pad_lock);

	if (!pad->connected)
		return padData;

	for (const btn_cfg& btn : button_config) {
		u16 val = TranslateKeycode(*pad, btn.keycode);
		if (val == 0) continue;
		
		// btnVal is converted value for everything except triggers
		u16 btnVal = val;
		if (btn.keycode.type != keycode_type::button)
			btnVal = ApplyThresholdLerp(val, axis_for_button_threshold);

		if (btn.offset == CELL_PAD_BTN_OFFSET_DIGITAL1) {
			if (btnVal == 0) continue;
			padData.m_digital_1 |= btn.flag;
			switch (btn.flag) {
			case CELL_PAD_CTRL_LEFT:  padData.m_press_left = btnVal; break;
			case CELL_PAD_CTRL_DOWN:  padData.m_press_down = btnVal; break;
			case CELL_PAD_CTRL_RIGHT: padData.m_press_right = btnVal; break;
			case CELL_PAD_CTRL_UP:    padData.m_press_up = btnVal; break;
			//These arent pressure btns
			case CELL_PAD_CTRL_R3:
			case CELL_PAD_CTRL_L3:
			case CELL_PAD_CTRL_START:
			case CELL_PAD_CTRL_SELECT:
			default: break;
			}
		}
		else if (btn.offset == CELL_PAD_BTN_OFFSET_DIGITAL2) {
			switch (btn.flag) {
			case CELL_PAD_CTRL_SQUARE:   padData.m_press_square = val = btnVal; break;
			case CELL_PAD_CTRL_CROSS:    padData.m_press_cross = val = btnVal; break;
			case CELL_PAD_CTRL_CIRCLE:   padData.m_press_circle = val = btnVal; break;
			case CELL_PAD_CTRL_TRIANGLE: padData.m_press_triangle = val = btnVal; break;
			case CELL_PAD_CTRL_R1:	     padData.m_press_R1 = val = btnVal; break;
			case CELL_PAD_CTRL_L1:       padData.m_press_L1 = val = btnVal; break;
			case CELL_PAD_CTRL_R2:       padData.m_press_R2 = val = NormalizeTriggerInput(val, ltriggerthreshold); break;
			case CELL_PAD_CTRL_L2:       padData.m_press_L2 = val = NormalizeTriggerInput(val, rtriggerthreshold); break;
			default: break;
			}
			if (val != 0)
				padData.m_digital_2 |= btn.flag;
		}
	}

	s32 stick_val[4];

	for (int i = 0; i < axis_config.size(); ++i) {
		const auto& axis = axis_config[i];
		stick_val[i] = TranslateKeycode(*pad, axis.keycode_max) - (TranslateKeycode(*pad, axis.keycode_min));
	}

	u16 lx, ly, rx, ry;

	// Normalize our two stick's axis based on the thresholds
	std::tie(lx, ly) = NormalizeStickDeadzone(stick_val[0], stick_val[1], lstickdeadzone);
	std::tie(rx, ry) = NormalizeStickDeadzone(stick_val[2], stick_val[3], rstickdeadzone);

	if (padsquircling != 0)
	{
		std::tie(lx, ly) = ConvertToSquirclePoint(lx, ly, padsquircling);
		std::tie(rx, ry) = ConvertToSquirclePoint(rx, ry, padsquircling);
	}

	padData.m_analog_left_x = lx;
	padData.m_analog_left_y = 255 - ly;
	padData.m_analog_right_x = rx;
	padData.m_analog_right_y = 255 - ry;

	// todo: dont do this and map these, currently only ds4 uses and they arent in the config so its just hardcoded
	// assume x,y,z,g order
	if (pad->m_sensors.size() == 4) {
		padData.m_sensor_x = pad->m_sensors[0].m_value;
		padData.m_sensor_y = pad->m_sensors[1].m_value;
		padData.m_sensor_z = pad->m_sensors[2].m_value;
		padData.m_sensor_g = pad->m_sensors[3].m_value;
	}

	return padData;
}

void EmulatedPad::SetRumble(u8 large, bool small) {
    // todo: use config mapping

    if (enable_vibrate_large)
        handler->SetVibrate(device_id, swap_vibrate ? 1 : 0, large);
    if (enable_vibrate_small)
        handler->SetVibrate(device_id, swap_vibrate ? 0 : 1, small ? 255 : 0);
}

std::vector<std::string> EmulatedPad::GetVibrateMotors() {
	std::vector<std::string> rtn;
	if (pad == nullptr)
		return rtn;

	std::lock_guard<std::mutex> lock(pad->pad_lock);

	for (const auto& motor : pad->m_vibrateMotors) {
		if (motor.m_isLargeMotor)
			rtn.push_back("Large");
		else
			rtn.push_back("Small");
	}
	return rtn;
}

std::unique_ptr<EmulatedPadRawPressWatcher> EmulatedPad::GetRawPressWatcher() {
	return std::make_unique<EmulatedPadRawPressWatcher>(pad);
}

EmulatedPadRawPressWatcher::EmulatedPadRawPressWatcher(std::shared_ptr<Pad> pad) : m_pad(pad) {
	if (pad == nullptr)
		return;

	Reset();
}

void EmulatedPadRawPressWatcher::Reset() {
	if (m_pad == nullptr)
		return;

	// get 'state' to use to base changes off of 
	std::lock_guard<std::mutex> lock(m_pad->pad_lock);

	// kick off size
	m_prevBtnState.clear();
	m_prevStickState.clear();
	m_prevBtnState.reserve(m_pad->m_buttons.size());
	m_prevStickState.reserve(m_pad->m_sticks.size());

	for (const auto& btn : m_pad->m_buttons)
		m_prevBtnState.emplace_back(btn.m_value);
	for (const auto& stick : m_pad->m_sticks)
		m_prevStickState.emplace_back(stick.m_valueLow, stick.m_valueHigh);
}

std::pair<bool, std::string> EmulatedPadRawPressWatcher::CheckForRawPress() {

	if (m_pad == nullptr)
		return{ false, "" };

	std::lock_guard<std::mutex> lock(m_pad->pad_lock);

	if (!m_pad->connected)
		return{ false, "" };

	// checking buttons is ez, we ignore any constantly 'pressed' button,
	// and only take buttons that have went from 0-255
	// this also allows for holding the key down intially and 'repressing' to trigger the bind
	// the range is skewed to support trigger or ds3
	for (size_t i = 0; i < m_pad->m_buttons.size(); ++i) {
		const auto& btn = m_pad->m_buttons[i];
		if (btn.m_value > 127 && (m_prevBtnState[i] < 90))
			return{ true, btn.m_name };
		else m_prevBtnState[i] = btn.m_value;
	}

	// sticks is a bit more complicated
	// but the general idea, is we assume the starting values for the sticks/axi' is the 'default'/resting position
	// in this case, theres no coming back from it like the buttons if they start with the axi pressed
	// they will have to rebind or call this again to get it right

	for (size_t i = 0; i < m_pad->m_sticks.size(); ++i) {
		const auto& stick = m_pad->m_sticks[i];
		const auto& prev = m_prevStickState[i];

		// if both axis started 'around 0' then 'trigger' if either value gets large
		if (prev.first < 0.35f && prev.second < 0.35f) {
			if (stick.m_valueHigh > 0.8f)
				return{ true, stick.m_name + "+" };
			if (stick.m_valueLow > 0.8f)
				return{ true, stick.m_name + "-" };
		}
		// if one axis started from a high standpoint, then wait until it crosses over 
		// into the other axis to trigger a 'full axis' bind
		else if (prev.first > 0.6f) {
			if (stick.m_valueLow == 0.f && stick.m_valueHigh > 0.f)
				return{ true, stick.m_name + "-+" };
		}
		else if (prev.second > 0.6f) {
			if (stick.m_valueHigh == 0.f && stick.m_valueLow > 0.f)
				return{ true, stick.m_name + "+-" };
		}
	}
	return{ false, "" };
}
