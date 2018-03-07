#pragma once

#include <cmath>
#include <vector>
#include <memory>
#include <mutex>
#include "stdafx.h"
#include "../../Utilities/Config.h"
#include "../../Utilities/types.h"
#include "Emu/System.h"

// TODO: HLE info (constants, structs, etc.) should not be available here

enum PortStatus
{
	CELL_PAD_STATUS_DISCONNECTED   = 0x00000000,
	CELL_PAD_STATUS_CONNECTED      = 0x00000001,
	CELL_PAD_STATUS_ASSIGN_CHANGES = 0x00000002,
};

enum PortSettings
{
	CELL_PAD_SETTING_PRESS_ON      = 0x00000002,
	CELL_PAD_SETTING_SENSOR_ON     = 0x00000004,
	CELL_PAD_SETTING_PRESS_OFF     = 0x00000000,
	CELL_PAD_SETTING_SENSOR_OFF    = 0x00000000,
};

enum Digital1Flags
{
	CELL_PAD_CTRL_LEFT     = 0x00000080,
	CELL_PAD_CTRL_DOWN     = 0x00000040,
	CELL_PAD_CTRL_RIGHT    = 0x00000020,
	CELL_PAD_CTRL_UP       = 0x00000010,
	CELL_PAD_CTRL_START    = 0x00000008,
	CELL_PAD_CTRL_R3       = 0x00000004,
	CELL_PAD_CTRL_L3       = 0x00000002,
	CELL_PAD_CTRL_SELECT   = 0x00000001,
};

enum Digital2Flags
{
	CELL_PAD_CTRL_PS       = 0x00000100,
	CELL_PAD_CTRL_SQUARE   = 0x00000080,
	CELL_PAD_CTRL_CROSS    = 0x00000040,
	CELL_PAD_CTRL_CIRCLE   = 0x00000020,
	CELL_PAD_CTRL_TRIANGLE = 0x00000010,
	CELL_PAD_CTRL_R1       = 0x00000008,
	CELL_PAD_CTRL_L1       = 0x00000004,
	CELL_PAD_CTRL_R2       = 0x00000002,
	CELL_PAD_CTRL_L2       = 0x00000001,
};

enum DeviceCapability
{
	CELL_PAD_CAPABILITY_PS3_CONFORMITY  = 0x00000001, //PS3 Conformity Controller
	CELL_PAD_CAPABILITY_PRESS_MODE      = 0x00000002, //Press mode supported
	CELL_PAD_CAPABILITY_SENSOR_MODE     = 0x00000004, //Sensor mode supported
	CELL_PAD_CAPABILITY_HP_ANALOG_STICK = 0x00000008, //High Precision analog stick
	CELL_PAD_CAPABILITY_ACTUATOR        = 0x00000010, //Motor supported
};

enum DeviceType
{
	CELL_PAD_DEV_TYPE_STANDARD   = 0,
	CELL_PAD_DEV_TYPE_BD_REMOCON = 4,
	CELL_PAD_DEV_TYPE_LDD        = 5,
};

enum ButtonDataOffset
{
	CELL_PAD_BTN_OFFSET_DIGITAL1       = 2,
	CELL_PAD_BTN_OFFSET_DIGITAL2       = 3,
	CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_X = 4,
	CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_Y = 5,
	CELL_PAD_BTN_OFFSET_ANALOG_LEFT_X  = 6,
	CELL_PAD_BTN_OFFSET_ANALOG_LEFT_Y  = 7,
	CELL_PAD_BTN_OFFSET_PRESS_RIGHT    = 8,
	CELL_PAD_BTN_OFFSET_PRESS_LEFT     = 9,
	CELL_PAD_BTN_OFFSET_PRESS_UP       = 10,
	CELL_PAD_BTN_OFFSET_PRESS_DOWN     = 11,
	CELL_PAD_BTN_OFFSET_PRESS_TRIANGLE = 12,
	CELL_PAD_BTN_OFFSET_PRESS_CIRCLE   = 13,
	CELL_PAD_BTN_OFFSET_PRESS_CROSS    = 14,
	CELL_PAD_BTN_OFFSET_PRESS_SQUARE   = 15,
	CELL_PAD_BTN_OFFSET_PRESS_L1       = 16,
	CELL_PAD_BTN_OFFSET_PRESS_R1       = 17,
	CELL_PAD_BTN_OFFSET_PRESS_L2       = 18,
	CELL_PAD_BTN_OFFSET_PRESS_R2       = 19,
	CELL_PAD_BTN_OFFSET_SENSOR_X       = 20,
	CELL_PAD_BTN_OFFSET_SENSOR_Y       = 21,
	CELL_PAD_BTN_OFFSET_SENSOR_Z       = 22,
	CELL_PAD_BTN_OFFSET_SENSOR_G       = 23,
};

static const u32 CELL_MAX_PADS = 127;
static const u32 CELL_PAD_MAX_PORT_NUM = 7;
static const u32 CELL_PAD_MAX_CODES = 64;
static const u32 CELL_PAD_MAX_CAPABILITY_INFO = 32;
static const u32 CELL_PAD_ACTUATOR_MAX = 2;

// 'Button' for the purposes of the emulator is basically a 'one way axis', because basically every 'button' on a ds3 has a 'one way axis' associated with it
// so true 'buttons' from a padhandler will be just 0 or 255, but any known 'triggers' can be set with a variable value
// this makes translating known 'triggers' easier to the emulated controller, along with supporting actual ds3
struct Button
{
	const u32 m_keyCode;
	u16 m_value; // 0 - 255
    const std::string m_name;

	Button(u32 keyCode, const std::string& name)
		: m_keyCode(keyCode)
		, m_value(0)
		, m_name(name)
	{
	}
};

struct AnalogStick
{
	const u32 m_keyCode;
	f32 m_valueHigh; // 0 to 1 
	f32 m_valueLow;  // 0 to 1
    const std::string m_name;

    AnalogStick(u32 keyCode, const std::string& name)
        : m_keyCode(keyCode)
        , m_valueHigh(0)
		, m_valueLow(0)
        , m_name(name)
	{
	}
};

// todo: this should be changed to be float for maximum precision,
// the issue will be deciding on a 'standard' unit and type for all the various sensors
// only ds4 currently supports using these though so its not a big deal for now
struct AnalogSensor
{
	const u32 m_offset;
	u32 m_value; // 0-1023
	const std::string m_name;

	AnalogSensor(u32 offset, u16 value, const std::string& name)
		: m_offset(offset)
		, m_value(value)
		, m_name(name)
	{}
};

// todo: change to use offset when we support different binding
struct VibrateMotor
{
	const bool m_isLargeMotor;
	u32 m_value; // 0-255

	VibrateMotor(bool largeMotor, u16 value)
		: m_isLargeMotor(largeMotor)
		, m_value(value)
	{}
};

struct PadStatus
{
	u32 m_port_status{0};
	u32 m_port_setting{0};
	u32 m_device_capability{0};
	u32 m_device_type{0};

	PadStatus() {};

	PadStatus(u32 port_status, u32 port_setting, u32 device_capability, u32 device_type) {
		m_port_status = port_status;
		m_port_setting = port_setting;
		m_device_capability = device_capability;
		m_device_type = device_type;
	}
};

// 'Standardized' data that comes from input provider, like ds4/xinput
struct Pad
{
	std::mutex pad_lock;
	bool connected{false};
	// Cable State:   0 - 1  plugged in ?
	u8 m_cable_state{0};

	// todo: standardize this to some level or enum before its ever used
	// DS4: 0 - 9  while unplugged, 0 - 10 while plugged in, 11 charge complete
	// XInput: 0 = Empty, 1 = Low, 2 = Medium, 3 = Full
	u8 m_battery_level{0};

	// These should *always* return max amount of axes available from a specfied backend pad
	// even if the current device doesnt support the given axes
	// the size and layout of the vectors must also remain constant for the lifetime of the pad
	std::vector<Button> m_buttons;
	std::vector<AnalogStick> m_sticks;
	std::vector<AnalogSensor> m_sensors;
	std::vector<VibrateMotor> m_vibrateMotors;
};

// 'Buffer' for easy use with cellpad
struct PadDataBuffer
{
	//These hold bits for their respective buttons
	u16 m_digital_1{0};
	u16 m_digital_2{0};

	//All sensors go from 0-255
	u16 m_analog_left_x{128};
	u16 m_analog_left_y{128};
	u16 m_analog_right_x{128};
	u16 m_analog_right_y{128};

	u16 m_press_right{0};
	u16 m_press_left{0};
	u16 m_press_up{0};
	u16 m_press_down{0};
	u16 m_press_triangle{0};
	u16 m_press_circle{0};
	u16 m_press_cross{0};
	u16 m_press_square{0};
	u16 m_press_L1{0};
	u16 m_press_L2{0};
	u16 m_press_R1{0};
	u16 m_press_R2{0};

	//Except for these...0-1023
	//~399 on sensor y is a level non moving controller
	u16 m_sensor_x{512};
	u16 m_sensor_y{399};
	u16 m_sensor_z{512};
	u16 m_sensor_g{512};
};

struct cfg_player final : cfg::node
{
	pad_handler def_handler = pad_handler::null;
	cfg_player(node* owner, const std::string& name, pad_handler type) : cfg::node(owner, name), def_handler(type) {};

	cfg::_enum<pad_handler> handler{ this, "Handler", def_handler };
	cfg::string device{ this, "Device", handler.to_string() };
	cfg::string profile{ this, "Profile", "Default Profile" };
};

struct cfg_input final : cfg::node
{
	const std::string cfg_name = fs::get_config_dir() + "/config_input.yml";

	cfg_player player1{ this, "Player 1 Input", pad_handler::keyboard };
	cfg_player player2{ this, "Player 2 Input", pad_handler::null };
	cfg_player player3{ this, "Player 3 Input", pad_handler::null };
	cfg_player player4{ this, "Player 4 Input", pad_handler::null };
	cfg_player player5{ this, "Player 5 Input", pad_handler::null };
	cfg_player player6{ this, "Player 6 Input", pad_handler::null };
	cfg_player player7{ this, "Player 7 Input", pad_handler::null };

	std::array<cfg_player*, 7> player{{ &player1, &player2, &player3, &player4, &player5, &player6, &player7 }}; // Thanks gcc! 

	bool load()
	{
		if (fs::file cfg_file{ cfg_name, fs::read })
		{
			return from_string(cfg_file.to_string());
		}

		return false;
	};

	void save()
	{
		fs::file(cfg_name, fs::rewrite).write(to_string());
	};
};

extern cfg_input g_cfg_input;

struct pad_config final : cfg::node
{
	std::string cfg_name = "";

	cfg::string ls_left { this, "Left Stick Left", "" };
	cfg::string ls_down { this, "Left Stick Down", "" };
	cfg::string ls_right{ this, "Left Stick Right", "" };
	cfg::string ls_up   { this, "Left Stick Up", "" };
	cfg::string rs_left { this, "Right Stick Left", "" };
	cfg::string rs_down { this, "Right Stick Down", "" };
	cfg::string rs_right{ this, "Right Stick Right", "" };
	cfg::string rs_up   { this, "Right Stick Up", "" };
	cfg::string start   { this, "Start", "" };
	cfg::string select  { this, "Select", "" };
	cfg::string ps      { this, "PS Button", "" };
	cfg::string square  { this, "Square", "" };
	cfg::string cross   { this, "Cross", "" };
	cfg::string circle  { this, "Circle", "" };
	cfg::string triangle{ this, "Triangle", "" };
	cfg::string left    { this, "Left", "" };
	cfg::string down    { this, "Down", "" };
	cfg::string right   { this, "Right", "" };
	cfg::string up      { this, "Up", "" };
	cfg::string r1      { this, "R1", "" };
	cfg::string r2      { this, "R2", "" };
	cfg::string r3      { this, "R3", "" };
	cfg::string l1      { this, "L1", "" };
	cfg::string l2      { this, "L2", "" };
	cfg::string l3      { this, "L3", "" };

	cfg::_int<0, 255> lstickdeadzone{ this, "Left Stick Deadzone", 0 };
	cfg::_int<0, 255> rstickdeadzone{ this, "Right Stick Deadzone", 0 };
	cfg::_int<0, 255> ltriggerthreshold{ this, "Left Trigger Threshold", 0 };
	cfg::_int<0, 255> rtriggerthreshold{ this, "Right Trigger Threshold", 0 };
	cfg::_int<0, 1000000> padsquircling{ this, "Pad Squircling Factor", 0 };

	cfg::_int<0, 255> colorR{ this, "Color Value R", 0 };
	cfg::_int<0, 255> colorG{ this, "Color Value G", 0 };
	cfg::_int<0, 255> colorB{ this, "Color Value B", 0 };

	cfg::_bool enable_vibration_motor_large{ this, "Enable Large Vibration Motor", true };
	cfg::_bool enable_vibration_motor_small{ this, "Enable Small Vibration Motor", true };
	cfg::_bool switch_vibration_motors{ this, "Switch Vibration Motors", false };

	bool load()
	{
		if (fs::file cfg_file{ cfg_name, fs::read })
		{
			return from_string(cfg_file.to_string());
		}

		return false;
	}

	void save()
	{
		fs::file(cfg_name, fs::rewrite).write(to_string());
	}

	bool exist()
	{
		return fs::is_file(cfg_name);
	}
};

class PadHandlerBase
{
protected:
	const std::string m_device_name_prefix;
public:
	const std::string m_handler_name;
	const pad_handler m_type;

	PadHandlerBase(pad_handler type, const std::string& handler_name, const std::string& device_prefix) 
		: m_type(type), m_handler_name(handler_name), m_device_name_prefix(device_prefix) {};
	virtual ~PadHandlerBase() = default;
	//Return list of devices for that handler
	virtual std::vector<std::string> ListDevices() = 0;
	//Callback called during pad_thread::ThreadFunc
	virtual void ThreadProc() = 0;
	//todo: figure out what to do with this config call, i dont want it here as is, and its specific to emulating ds3
	virtual void init_config(pad_config* /*cfg*/, const std::string& /*name*/) = 0;

	// returns 'device handle' or -1 if failed, handle is used in rest of these calls from then on
	virtual s32 EnableGetDevice(const std::string& deviceName) = 0;
	virtual bool IsDeviceConnected(u32 deviceNumber) = 0;
    virtual void DisableDevice(u32 deviceNumber) = 0;
	// null if no device exists or is enabled
	// shared_ptr is updated as new data comes in, pad_lock allows multithread usage
	virtual std::shared_ptr<Pad> GetDeviceData(u32 deviceNumber) = 0;
	virtual void SetVibrate(u32 deviceNumber, u32 keycode, u32 value) = 0;
    virtual void SetRGBData(u32 deviceNumber, u8 r, u8 g, u8 b) = 0;

	// This returns how many pads the handler is 'handling', even if disconnected
	virtual u32 GetNumPads() = 0;
};
