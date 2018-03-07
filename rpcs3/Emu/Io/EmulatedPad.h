#pragma once

#include "stdafx.h"
#include "PadHandler.h"
#include <memory>

// Helper class for use in the UI for use in binding
class EmulatedPadRawPressWatcher {
	std::vector<u16> m_prevBtnState;
	std::vector<std::pair<f32, f32>> m_prevStickState;
	std::shared_ptr<Pad> m_pad;
public:
	EmulatedPadRawPressWatcher(std::shared_ptr<Pad> pad);
	void Reset();
	std::pair<bool, std::string> CheckForRawPress();

	EmulatedPadRawPressWatcher(const EmulatedPadRawPressWatcher&) = delete;
};

class EmulatedPad {
private:
	enum class keycode_type {
		button,
		axis_neg,
		axis_pos,
		axis_neg_pos,
		axis_pos_neg,
	};

	// todo: rework these to hold ptr/reference rather than this
	struct keycode_cfg {
		u32 keycode{0};
		keycode_type type{ keycode_type::button };
		keycode_cfg() { };
		keycode_cfg(u32 k, keycode_type t) : keycode(k), type(t) {};
	};

	struct btn_cfg {
		u32 offset{0};
		keycode_cfg keycode;
		u32 flag{0};
		btn_cfg() {};
		btn_cfg(u32 o, keycode_cfg k, u32 f)
			: offset(o), keycode(k), flag(f) {};
	};

	struct stick_cfg {
		u32 offset{0};
		keycode_cfg keycode_min;
		keycode_cfg keycode_max;
		stick_cfg() {};
		stick_cfg(u32 o, keycode_cfg min, keycode_cfg max) 
			: offset(o), keycode_min(min), keycode_max(max) {};
	};

	const std::shared_ptr<PadHandlerBase> handler;
	const u32 device_id;

	std::shared_ptr<Pad> pad;

	std::unordered_map<std::string, u32> pad_button_map;
	std::unordered_map<std::string, u32> pad_axis_map;

	std::vector<btn_cfg> button_config;
	std::array<stick_cfg, 4> axis_config;

	u32 lstickdeadzone{0};
	u32 rstickdeadzone{0};
	u32 ltriggerthreshold{0};
	u32 rtriggerthreshold{0};
	u32 padsquircling{0};

    bool enable_vibrate_small{ true };
    bool enable_vibrate_large{ true };
    bool swap_vibrate{ false };

public:
	const u32 pad_capabilities = CELL_PAD_CAPABILITY_PS3_CONFORMITY | CELL_PAD_CAPABILITY_PRESS_MODE | CELL_PAD_CAPABILITY_HP_ANALOG_STICK | CELL_PAD_CAPABILITY_ACTUATOR | CELL_PAD_CAPABILITY_SENSOR_MODE;
	const u32 device_type = CELL_PAD_DEV_TYPE_STANDARD;

	EmulatedPad(u32 devid, const pad_config& cfg, std::shared_ptr<PadHandlerBase> padHandler);
	void RebindController(const pad_config& cfg);
    PadDataBuffer GetPadData();
    void SetRumble(u8 largeMotor, bool smallMotor);

	std::unique_ptr<EmulatedPadRawPressWatcher> GetRawPressWatcher();
	// Returns names of underlying device vibrate motors
	std::vector<std::string> GetVibrateMotors();

private:

    template <typename T>
    T lerp(T v0, T v1, T t) {
        return std::fma(t, v1, std::fma(-t, v0, v0));
    }

    // Search an unordered map for a string value and return found keycode
	keycode_cfg FindKeyCode(const std::unordered_map<std::string, u32>& btns, const std::unordered_map<std::string, u32>& axis, const cfg::string& name);

	inline u16 ApplyThresholdLerp(u16 val, u16 threshold) {
		if (val < threshold)
			return 0;
		return static_cast<u16>(lerp(0.f, 255.f, (val - threshold) / (255.f - threshold)));
	}

    // This function normalizes stick deadzone based on the DS3's deadzone, which is ~13%
    // X and Y is expected to be in (-255) to 255 range, deadzone should be in terms of thumb stick range
    // return is new x and y values in 0-255 range
    std::tuple<u16, u16> NormalizeStickDeadzone(s32 inX, s32 inY, u32 deadzone);

	u16 NormalizeTriggerInput(u16 value, int threshold);

    // get clamped value between min and max
    s32 Clamp(f32 input, s32 min, s32 max) {
		if (input > max)
			return max;
		else if (input < min)
			return min;
		else return static_cast<s32>(input);
	}

    // get clamped value between 0 and 255
    u16 Clamp0To255(f32 input) {
		return static_cast<u16>(Clamp(input, 0, 255));
	}

	u16 Clamp0To255(int input) {
		return static_cast<u16>(std::min(0, std::max(input, 255)));
	}

    // input has to be [-1,1]. result will be [0,255]
    u16 ConvertAxis(float value) {
		return static_cast<u16>((value + 1.0)*(255.0 / 2.0));
	}

    // The DS3, (and i think xbox controllers) give a 'square-ish' type response, so that the corners will give (almost)max x/y instead of the ~30x30 from a perfect circle
    // using a simple scale/sensitivity increase would *work* although it eats a chunk of our usable range in exchange
    // this might be the best for now, in practice it seems to push the corners to max of 20x20, with a squircle_factor of 8000
    // This function assumes inX and inY is already in 0-255
    std::tuple<u16, u16> ConvertToSquirclePoint(u16 inX, u16 inY, int squircle_factor);

	// This tries to convert axis to give us the max even in the corners,
	// this actually might work 'too' well, we end up actually getting diagonals of actual max/min, we need the corners still a bit rounded to match ds3
	// im leaving it here for now, and future reference as it probably can be used later
	//taken from http://theinstructionlimit.com/squaring-the-thumbsticks
	std::tuple<u16, u16> ConvertToSquarePoint(u16 inX, u16 inY, u32 innerRoundness = 0);

	u16 TranslateKeycode(const Pad& pad, const keycode_cfg& keyCode);
};