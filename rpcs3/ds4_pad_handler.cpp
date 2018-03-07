#include "ds4_pad_handler.h"

#include <thread>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace
{
	const u32 DS4_ACC_RES_PER_G = 8192;
	const u32 DS4_GYRO_RES_PER_DEG_S = 16; // todo: technically this could be 1024, but keeping it at 16 keeps us within 16 bits of precision, allowing us to calibrate the buffer 'inline'
	const u32 DS4_FEATURE_REPORT_0x02_SIZE = 37;
	const u32 DS4_FEATURE_REPORT_0x05_SIZE = 41;
	const u32 DS4_FEATURE_REPORT_0x81_SIZE = 7;
	const u32 DS4_INPUT_REPORT_0x11_SIZE = 78;
	const u32 DS4_OUTPUT_REPORT_0x05_SIZE = 32;
	const u32 DS4_OUTPUT_REPORT_0x11_SIZE = 78;
	const u32 DS4_INPUT_REPORT_GYRO_X_OFFSET = 13;
	const u32 DS4_INPUT_REPORT_BATTERY_OFFSET = 30;

	inline s16 GetS16LEData(const u8* buf)
	{
		return (s16)(((u16)buf[0] << 0) + ((u16)buf[1] << 8));
	}

	inline u32 GetU32LEData(const u8* buf)
	{
		return (u32)(((u32)buf[0] << 0) + ((u32)buf[1] << 8) + ((u32)buf[2] << 16) + ((u32)buf[3] << 24));
	}
}

ds4_pad_handler::ds4_pad_handler() : PadHandlerBase(pad_handler::ds4, "DS4", "Ds4 Pad #")
{
	Init();
}

void ds4_pad_handler::init_config(pad_config* cfg, const std::string& name)
{
	// Set this profile's save location
	cfg->cfg_name = name;

	// Set default button mapping
	cfg->ls_left.def  = axis_list.at(DS4KeyCodes::LSX) + "-";
	cfg->ls_down.def  = axis_list.at(DS4KeyCodes::LSY) + "-";
	cfg->ls_right.def = axis_list.at(DS4KeyCodes::LSX) + "+";
	cfg->ls_up.def    = axis_list.at(DS4KeyCodes::LSY) + "+";
	cfg->rs_left.def  = axis_list.at(DS4KeyCodes::RSX) + "-";
	cfg->rs_down.def  = axis_list.at(DS4KeyCodes::RSY) + "-";
	cfg->rs_right.def = axis_list.at(DS4KeyCodes::RSX) + "+";
	cfg->rs_up.def    = axis_list.at(DS4KeyCodes::RSY) + "+";
	cfg->start.def    = button_list.at(DS4KeyCodes::Options);
	cfg->select.def   = button_list.at(DS4KeyCodes::Share);
	cfg->ps.def       = button_list.at(DS4KeyCodes::PSButton);
	cfg->square.def   = button_list.at(DS4KeyCodes::Square);
	cfg->cross.def    = button_list.at(DS4KeyCodes::Cross);
	cfg->circle.def   = button_list.at(DS4KeyCodes::Circle);
	cfg->triangle.def = button_list.at(DS4KeyCodes::Triangle);
	cfg->left.def     = button_list.at(DS4KeyCodes::Left);
	cfg->down.def     = button_list.at(DS4KeyCodes::Down);
	cfg->right.def    = button_list.at(DS4KeyCodes::Right);
	cfg->up.def       = button_list.at(DS4KeyCodes::Up);
	cfg->r1.def       = button_list.at(DS4KeyCodes::R1);
	cfg->r2.def       = button_list.at(DS4KeyCodes::R2);
	cfg->r3.def       = button_list.at(DS4KeyCodes::R3);
	cfg->l1.def       = button_list.at(DS4KeyCodes::L1);
	cfg->l2.def       = button_list.at(DS4KeyCodes::L2);
	cfg->l3.def       = button_list.at(DS4KeyCodes::L3);

	// Set default misc variables
	cfg->lstickdeadzone.def    = 40; // between 0 and 255
	cfg->rstickdeadzone.def    = 40; // between 0 and 255
	cfg->ltriggerthreshold.def = 0;  // between 0 and 255
	cfg->rtriggerthreshold.def = 0;  // between 0 and 255
	cfg->padsquircling.def     = 8000;

	// Set color value
	cfg->colorR.def = 0;
	cfg->colorG.def = 0;
	cfg->colorB.def = 20;

	// apply defaults
	cfg->from_default();
}

std::string ds4_pad_handler::GetDeviceId(const std::string& padId) {
    size_t pos = padId.find(m_device_name_prefix);
    if (pos == std::string::npos || (padId.size() < 10))
        return "";

    return padId.substr(pos + m_device_name_prefix.size());
}

std::array<u16, ds4_pad_handler::DS4KeyCodes::KeyCodeCount> ds4_pad_handler::GetButtonValues(const DS4Device& device)
{
	std::array<u16, DS4KeyCodes::KeyCodeCount> keyBuffer;
	const auto buf = device.padData;

	// Left Stick X Axis
	keyBuffer[DS4KeyCodes::LSX] = buf[1];

	// Left Stick Y Axis (Up is the negative for some reason)
	keyBuffer[DS4KeyCodes::LSY] = buf[2];

	// Right Stick X Axis
	keyBuffer[DS4KeyCodes::RSX] = buf[3];

	// Right Stick Y Axis (Up is the negative for some reason)
	keyBuffer[DS4KeyCodes::RSY] = buf[4];

	// bleh, dpad in buffer is stored in a different state
	u8 dpadState = buf[5] & 0xf;
	switch (dpadState)
	{
	case 0x08: // none pressed
		keyBuffer[DS4KeyCodes::Up] = 0;
		keyBuffer[DS4KeyCodes::Down] = 0;
		keyBuffer[DS4KeyCodes::Left] = 0;
		keyBuffer[DS4KeyCodes::Right] = 0;
		break;
	case 0x07: // NW...left and up
		keyBuffer[DS4KeyCodes::Up] = 255;
		keyBuffer[DS4KeyCodes::Down] = 0;
		keyBuffer[DS4KeyCodes::Left] = 255;
		keyBuffer[DS4KeyCodes::Right] = 0;
		break;
	case 0x06: // W..left
		keyBuffer[DS4KeyCodes::Up] = 0;
		keyBuffer[DS4KeyCodes::Down] = 0;
		keyBuffer[DS4KeyCodes::Left] = 255;
		keyBuffer[DS4KeyCodes::Right] = 0;
		break;
	case 0x05: // SW..left down
		keyBuffer[DS4KeyCodes::Up] = 0;
		keyBuffer[DS4KeyCodes::Down] = 255;
		keyBuffer[DS4KeyCodes::Left] = 255;
		keyBuffer[DS4KeyCodes::Right] = 0;
		break;
	case 0x04: // S..down
		keyBuffer[DS4KeyCodes::Up] = 0;
		keyBuffer[DS4KeyCodes::Down] = 255;
		keyBuffer[DS4KeyCodes::Left] = 0;
		keyBuffer[DS4KeyCodes::Right] = 0;
		break;
	case 0x03: // SE..down and right
		keyBuffer[DS4KeyCodes::Up] = 0;
		keyBuffer[DS4KeyCodes::Down] = 255;
		keyBuffer[DS4KeyCodes::Left] = 0;
		keyBuffer[DS4KeyCodes::Right] = 255;
		break;
	case 0x02: // E... right
		keyBuffer[DS4KeyCodes::Up] = 0;
		keyBuffer[DS4KeyCodes::Down] = 0;
		keyBuffer[DS4KeyCodes::Left] = 0;
		keyBuffer[DS4KeyCodes::Right] = 255;
		break;
	case 0x01: // NE.. up right
		keyBuffer[DS4KeyCodes::Up] = 255;
		keyBuffer[DS4KeyCodes::Down] = 0;
		keyBuffer[DS4KeyCodes::Left] = 0;
		keyBuffer[DS4KeyCodes::Right] = 255;
		break;
	case 0x00: // n.. up
		keyBuffer[DS4KeyCodes::Up] = 255;
		keyBuffer[DS4KeyCodes::Down] = 0;
		keyBuffer[DS4KeyCodes::Left] = 0;
		keyBuffer[DS4KeyCodes::Right] = 0;
		break;
	default:
		fmt::throw_exception("ds4 dpad state encountered unexpected input");
	}

	// square, cross, circle, triangle
	keyBuffer[DS4KeyCodes::Square] =   ((buf[5] & (1 << 4)) != 0) ? 255 : 0;
	keyBuffer[DS4KeyCodes::Cross] =    ((buf[5] & (1 << 5)) != 0) ? 255 : 0;
	keyBuffer[DS4KeyCodes::Circle] =   ((buf[5] & (1 << 6)) != 0) ? 255 : 0;
	keyBuffer[DS4KeyCodes::Triangle] = ((buf[5] & (1 << 7)) != 0) ? 255 : 0;

	// L1, R1, L2, L3, select, start, L3, L3
	keyBuffer[DS4KeyCodes::L1]      = ((buf[6] & (1 << 0)) != 0) ? 255 : 0;
	keyBuffer[DS4KeyCodes::R1]      = ((buf[6] & (1 << 1)) != 0) ? 255 : 0;
	//keyBuffer[DS4KeyCodes::L2But]   = ((buf[6] & (1 << 2)) != 0) ? 255 : 0;
	//keyBuffer[DS4KeyCodes::R2But]   = ((buf[6] & (1 << 3)) != 0) ? 255 : 0;
	keyBuffer[DS4KeyCodes::Share]   = ((buf[6] & (1 << 4)) != 0) ? 255 : 0;
	keyBuffer[DS4KeyCodes::Options] = ((buf[6] & (1 << 5)) != 0) ? 255 : 0;
	keyBuffer[DS4KeyCodes::L3]      = ((buf[6] & (1 << 6)) != 0) ? 255 : 0;
	keyBuffer[DS4KeyCodes::R3]      = ((buf[6] & (1 << 7)) != 0) ? 255 : 0;

	// PS Button, Touch Button
	keyBuffer[DS4KeyCodes::PSButton] = ((buf[7] & (1 << 0)) != 0) ? 255 : 0;
	keyBuffer[DS4KeyCodes::TouchPad] = ((buf[7] & (1 << 1)) != 0) ? 255 : 0;

	// L2, R2
	keyBuffer[DS4KeyCodes::L2] = buf[8];
	keyBuffer[DS4KeyCodes::R2] = buf[9];

	return keyBuffer;
}

void ds4_pad_handler::ProcessDataToPad(DS4Device& device, Pad& pad)
{
	pad.m_battery_level = device.batteryLevel;
	pad.m_cable_state = device.cableState;

	auto buf = device.padData;
	auto button_values = GetButtonValues(device);

	for (const auto& btn : button_list)
		pad.m_buttons[btn.first].m_value = button_values[btn.first];

	for (const auto& axi : axis_list) {
		// sticks are added with previous value and divided to 'smooth' out the readings
		// the ds4 seems to rapidly flicker sometimes between two values and this seems to stop that
		const u32 idx = axi.first - DS4KeyCodes::LSX;
		u16 val = (button_values[axi.first] + device.prevStickValues[idx]) / 2;
		device.prevStickValues[idx] = button_values[axi.first];

		bool neg = val < 128;
		f32 convertedVal = Clamp(neg ? (128 - val) / 127.5f : (val - 128) / 127.5f, 0.f, 1.f);
		pad.m_sticks[idx].m_valueLow = neg ? convertedVal : 0.f;
		pad.m_sticks[idx].m_valueHigh = neg ? 0.f : convertedVal;
	}

	// these values come already calibrated from our ds4Thread,
	// all we need to do is convert to ds3 range

	// todo: dont hardcode these when sensors are actually supported for bindings

	// accel
	f32 accelX = (((s16)((u16)(buf[20] << 8) | buf[19])) / static_cast<f32>(DS4_ACC_RES_PER_G)) * -1;
	f32 accelY = (((s16)((u16)(buf[22] << 8) | buf[21])) / static_cast<f32>(DS4_ACC_RES_PER_G)) * -1;
	f32 accelZ = (((s16)((u16)(buf[24] << 8) | buf[23])) / static_cast<f32>(DS4_ACC_RES_PER_G)) * -1;

	// now just use formula from ds3
	accelX = accelX * 113 + 512;
	accelY = accelY * 113 + 512;
	accelZ = accelZ * 113 + 512;

	pad.m_sensors[0].m_value = Clamp0To1023(accelX);
	pad.m_sensors[1].m_value = Clamp0To1023(accelY);
	pad.m_sensors[2].m_value = Clamp0To1023(accelZ);

	// gyroX is yaw, which is all that we need
	f32 gyroX = (((s16)((u16)(buf[16] << 8) | buf[15])) / static_cast<f32>(DS4_GYRO_RES_PER_DEG_S)) * -1;
	//const int gyroY = ((u16)(buf[14] << 8) | buf[13]) / 256;
	//const int gyroZ = ((u16)(buf[18] << 8) | buf[17]) / 256;

	// convert to ds3
	gyroX = gyroX * (123.f / 90.f) + 512;

	pad.m_sensors[3].m_value = Clamp0To1023(gyroX);
}

bool ds4_pad_handler::GetCalibrationData(DS4Device& ds4Dev)
{
	std::array<u8, 64> buf;
	if (ds4Dev.btCon)
	{
		for (int tries = 0; tries < 3; ++tries)
		{
			buf[0] = 0x05;
			if (hid_get_feature_report(ds4Dev.hidDevice, buf.data(), DS4_FEATURE_REPORT_0x05_SIZE) <= 0)
				return false;

			const u8 btHdr = 0xA3;
			const u32 crcHdr = CRCPP::CRC::Calculate(&btHdr, 1, crcTable);
			const u32 crcCalc = CRCPP::CRC::Calculate(buf.data(), (DS4_FEATURE_REPORT_0x05_SIZE - 4), crcTable, crcHdr);
			const u32 crcReported = GetU32LEData(&buf[DS4_FEATURE_REPORT_0x05_SIZE - 4]);
			if (crcCalc != crcReported)
				LOG_WARNING(HLE, "[DS4] Calibration CRC check failed! Will retry up to 3 times. Received 0x%x, Expected 0x%x", crcReported, crcCalc);
			else break;
			if (tries == 2)
				return false;
		}
	}
	else
	{
		buf[0] = 0x02;
		if (hid_get_feature_report(ds4Dev.hidDevice, buf.data(), DS4_FEATURE_REPORT_0x02_SIZE) <= 0)
		{
			LOG_ERROR(HLE, "[DS4] Failed getting calibration data report!");
			return false;
		}
	}

	ds4Dev.calibData[DS4CalibIndex::PITCH].bias = GetS16LEData(&buf[1]);
	ds4Dev.calibData[DS4CalibIndex::YAW].bias = GetS16LEData(&buf[3]);
	ds4Dev.calibData[DS4CalibIndex::ROLL].bias = GetS16LEData(&buf[5]);

	s16 pitchPlus, pitchNeg, rollPlus, rollNeg, yawPlus, yawNeg;
	if (ds4Dev.btCon)
	{
		pitchPlus = GetS16LEData(&buf[7]);
		yawPlus   = GetS16LEData(&buf[9]);
		rollPlus  = GetS16LEData(&buf[11]);
		pitchNeg  = GetS16LEData(&buf[13]);
		yawNeg    = GetS16LEData(&buf[15]);
		rollNeg   = GetS16LEData(&buf[17]);
	}
	else
	{
		pitchPlus = GetS16LEData(&buf[7]);
		pitchNeg  = GetS16LEData(&buf[9]);
		yawPlus   = GetS16LEData(&buf[11]);
		yawNeg    = GetS16LEData(&buf[13]);
		rollPlus  = GetS16LEData(&buf[15]);
		rollNeg   = GetS16LEData(&buf[17]);
	}

	const s32 gyroSpeedScale = GetS16LEData(&buf[19]) + GetS16LEData(&buf[21]);

	ds4Dev.calibData[DS4CalibIndex::PITCH].sensNumer = gyroSpeedScale * DS4_GYRO_RES_PER_DEG_S;
	ds4Dev.calibData[DS4CalibIndex::PITCH].sensDenom = pitchPlus - pitchNeg;

	ds4Dev.calibData[DS4CalibIndex::YAW].sensNumer = gyroSpeedScale * DS4_GYRO_RES_PER_DEG_S;
	ds4Dev.calibData[DS4CalibIndex::YAW].sensDenom = yawPlus - yawNeg;

	ds4Dev.calibData[DS4CalibIndex::ROLL].sensNumer = gyroSpeedScale * DS4_GYRO_RES_PER_DEG_S;
	ds4Dev.calibData[DS4CalibIndex::ROLL].sensDenom = rollPlus - rollNeg;

	const s16 accelXPlus = GetS16LEData(&buf[23]);
	const s16 accelXNeg  = GetS16LEData(&buf[25]);
	const s16 accelYPlus = GetS16LEData(&buf[27]);
	const s16 accelYNeg  = GetS16LEData(&buf[29]);
	const s16 accelZPlus = GetS16LEData(&buf[31]);
	const s16 accelZNeg  = GetS16LEData(&buf[33]);

	const s32 accelXRange = accelXPlus - accelXNeg;
	ds4Dev.calibData[DS4CalibIndex::X].bias = accelXPlus - accelXRange / 2;
	ds4Dev.calibData[DS4CalibIndex::X].sensNumer = 2 * DS4_ACC_RES_PER_G;
	ds4Dev.calibData[DS4CalibIndex::X].sensDenom = accelXRange;

	const s32 accelYRange = accelYPlus - accelYNeg;
	ds4Dev.calibData[DS4CalibIndex::Y].bias = accelYPlus - accelYRange / 2;
	ds4Dev.calibData[DS4CalibIndex::Y].sensNumer = 2 * DS4_ACC_RES_PER_G;
	ds4Dev.calibData[DS4CalibIndex::Y].sensDenom = accelYRange;

	const s32 accelZRange = accelZPlus - accelZNeg;
	ds4Dev.calibData[DS4CalibIndex::Z].bias = accelZPlus - accelZRange / 2;
	ds4Dev.calibData[DS4CalibIndex::Z].sensNumer = 2 * DS4_ACC_RES_PER_G;
	ds4Dev.calibData[DS4CalibIndex::Z].sensDenom = accelZRange;

	// Make sure data 'looks' valid, dongle will report invalid calibration data with no controller connected

	for (const auto& data : ds4Dev.calibData)
	{
		if (data.sensDenom == 0)
			return false;
	}

	return true;
}

void ds4_pad_handler::CheckAddDevice(hid_device* hidDevice, hid_device_info* hidDevInfo)
{
	std::string serial = "";
    bool btCon = false;
	// There isnt a nice 'portable' way with hidapi to detect bt vs wired as the pid/vid's are the same
	// Let's try getting 0x81 feature report, which should will return mac address on wired, and should error on bluetooth
	std::array<u8, 64> buf{};
	buf[0] = 0x81;
	if (hid_get_feature_report(hidDevice, buf.data(), DS4_FEATURE_REPORT_0x81_SIZE) > 0)
	{
		serial = fmt::format("%x%x%x%x%x%x", buf[6], buf[5], buf[4], buf[3], buf[2], buf[1]);
	}
	else
	{
        btCon = true;
		std::wstring wSerial(hidDevInfo->serial_number);
		serial = std::string(wSerial.begin(), wSerial.end());
	}

    // check if device already exists
    auto it = controllers.find(serial);
    if (it != controllers.end()) {
        // update if we dont have device nor path
        if (it->second->hidDevice != nullptr && it->second->path != "")
            return;

        it->second->hidDevice = hidDevice;
        if (!GetCalibrationData(*it->second)) {
            hid_close(hidDevice);
            return;
        }

        it->second->btCon = btCon;
        it->second->path = hidDevInfo->path;
        it->second->hasCalibData = true;
		it->second->last_conn_status = false;
    }
    else {
        std::unique_ptr<DS4Device> ds4Dev = std::make_unique<DS4Device>();
        ds4Dev->hidDevice = hidDevice;

        if (!GetCalibrationData(*ds4Dev)) {
            hid_close(hidDevice);
            return;
        }

        ds4Dev->btCon = btCon;
        ds4Dev->hasCalibData = true;
        ds4Dev->path = hidDevInfo->path;
        ds4Dev->device_id = static_cast<u32>(controllers.size());
		ds4Dev->last_conn_status = true;

        hid_set_nonblocking(hidDevice, 1);
        controllers.emplace(serial, std::move(ds4Dev));
    }
}

void ds4_pad_handler::RefreshControllers() {
    for (auto pid : ds4Pids) {
        hid_device_info* devInfo = hid_enumerate(DS4_VID, pid);
        hid_device_info* head = devInfo;
        while (devInfo) {
            hid_device* dev = hid_open_path(devInfo->path);
            if (dev)
                CheckAddDevice(dev, devInfo);
            else
                LOG_ERROR(HLE, "[DS4] hid_open_path failed! Reason: %s", hid_error(dev));
            devInfo = devInfo->next;
        }
        hid_free_enumeration(head);
    }
}

ds4_pad_handler::~ds4_pad_handler()
{
	for (auto& controller : controllers)
	{
		if (controller.second->hidDevice)
		{
			// Disable blinking and vibration
			controller.second->smallVibrate = 0;
			controller.second->largeVibrate = 0;
			controller.second->led_delay_on = 0;
			controller.second->led_delay_off = 0;
			SendVibrateData(*controller.second);

			hid_close(controller.second->hidDevice);
		}
	}
	hid_exit();
}

int ds4_pad_handler::SendVibrateData(const DS4Device& device)
{
	std::array<u8, 78> outputBuf{0};
	// write rumble state
	if (device.btCon)
	{
		outputBuf[0] = 0x11;
		outputBuf[1] = 0xC4;
		outputBuf[3] = 0x07;
		outputBuf[6] = device.smallVibrate;
		outputBuf[7] = device.largeVibrate;
		outputBuf[8] = device.colorR; // red
		outputBuf[9] = device.colorG; // green
		outputBuf[10] = device.colorB; // blue

		// alternating blink states with values 0-255: only setting both to zero disables blinking
		// 255 is roughly 2 seconds, so setting both values to 255 results in a 4 second interval
		// using something like (0,10) will heavily blink, while using (0, 255) will be slow. you catch the drift
		outputBuf[11] = device.led_delay_on;
		outputBuf[12] = device.led_delay_off;

		const u8 btHdr = 0xA2;
		const u32 crcHdr = CRCPP::CRC::Calculate(&btHdr, 1, crcTable);
		const u32 crcCalc = CRCPP::CRC::Calculate(outputBuf.data(), (DS4_OUTPUT_REPORT_0x11_SIZE - 4), crcTable, crcHdr);

		outputBuf[74] = (crcCalc >> 0) & 0xFF;
		outputBuf[75] = (crcCalc >> 8) & 0xFF;
		outputBuf[76] = (crcCalc >> 16) & 0xFF;
		outputBuf[77] = (crcCalc >> 24) & 0xFF;

		return hid_write_control(device.hidDevice, outputBuf.data(), DS4_OUTPUT_REPORT_0x11_SIZE);
	}
	else
	{
		outputBuf[0] = 0x05;
		outputBuf[1] = 0x07;
		outputBuf[4] = device.smallVibrate;
		outputBuf[5] = device.largeVibrate;
		outputBuf[6] = device.colorR; // red
		outputBuf[7] = device.colorG; // green
		outputBuf[8] = device.colorB; // blue
		outputBuf[9] = device.led_delay_on;
		outputBuf[10] = device.led_delay_off;

		return hid_write(device.hidDevice, outputBuf.data(), DS4_OUTPUT_REPORT_0x05_SIZE);
	}
}

bool ds4_pad_handler::Init()
{
	const int res = hid_init();
	if (res != 0) {
		LOG_ERROR(HLE, "hidapi-init error.init, %d", res);
		return false;
	}

	// get all the possible controllers at start
    RefreshControllers();

	if (controllers.size() == 0)
		LOG_WARNING(HLE, "[DS4] No controllers found!");
	else
		LOG_SUCCESS(HLE, "[DS4] Controllers found: %d", controllers.size());

	return true;
}

std::vector<std::string> ds4_pad_handler::ListDevices()
{
	std::lock_guard<std::mutex> lock(handlerLock);

	std::vector<std::string> ds4_pads_list;

	if (!Init())
		return ds4_pads_list;

	ds4_pads_list.reserve(controllers.size());
	for (auto& pad : controllers) {
		if (pad.second->last_conn_status)
			ds4_pads_list.emplace_back(m_device_name_prefix + pad.first);
	}

	return ds4_pads_list;
}

void ds4_pad_handler::SetVibrate(u32 deviceNumber, u32 keycode, u32 value) {
	std::lock_guard<std::mutex> lock(handlerLock);
    auto it = std::find_if(bindings.begin(), bindings.end(), [deviceNumber](const auto& d) { return d.first->device_id == deviceNumber; });
    if (it == bindings.end())
        return;

    auto &motors = it->second->m_vibrateMotors;
    if (keycode > motors.size())
        return;

    motors[keycode].m_value = value;
}

void ds4_pad_handler::SetRGBData(u32 deviceNumber, u8 r, u8 g, u8 b) {
	std::lock_guard<std::mutex> lock(handlerLock);
    auto it = std::find_if(bindings.begin(), bindings.end(), [deviceNumber](const auto& d) { return d.first->device_id == deviceNumber; });
    if (it == bindings.end())
        return;

    // dumbest feature ever 
    it->first->colorB = b;
    it->first->colorR = r;
    it->first->colorG = g;
}

std::shared_ptr<Pad> ds4_pad_handler::GetDeviceData(u32 deviceNumber) {
	std::lock_guard<std::mutex> lock(handlerLock);
    auto it = std::find_if(bindings.begin(), bindings.end(), [deviceNumber](const auto& d) { return d.first->device_id == deviceNumber; });
    if (it == bindings.end())
        return nullptr;

    return it->second;
}

void ds4_pad_handler::ThreadProc()
{
	std::lock_guard<std::mutex> lock(handlerLock);
	for (int i = 0; i < static_cast<int>(bindings.size()); i++)
	{
		auto device = bindings[i].first;
		auto thepad = bindings[i].second.get();
		std::lock_guard<std::mutex> lock2(thepad->pad_lock);

		if (device->hidDevice == nullptr)
		{
            // force refresh if we have a desired 'serial' but not actual device
            // todo: maybe change this to only happen every so often rather than every loop...
            if (device->path == "")
                RefreshControllers();
			// try to reconnect
			hid_device* dev = hid_open_path(device->path.c_str());
			if (dev)
			{
				if (device->last_conn_status == false)
				{
					LOG_SUCCESS(HLE, "DS4 device %d reconnected", i);
					device->last_conn_status = true;
				}
				hid_set_nonblocking(dev, 1);
				device->hidDevice = dev;
				thepad->connected = true;
				if (!device->hasCalibData)
					device->hasCalibData = GetCalibrationData(*device);
			}
			else
			{
				// nope, not there
				if (device->last_conn_status == true)
				{
					LOG_ERROR(HLE, "DS4 device %d disconnected", i);
					device->last_conn_status = false;
				}
				thepad->connected = false;
				continue;
			}
		}
		else if (device->last_conn_status == false)
		{
			LOG_NOTICE(HLE, "DS4 device %d connected", i);
			thepad->connected = true;
			device->last_conn_status = true;
		}

		DS4DataStatus status = GetRawData(*device);

		if (status == DS4DataStatus::ReadError)
		{
			// this also can mean disconnected, either way deal with it on next loop and reconnect
			hid_close(device->hidDevice);
			device->hidDevice = nullptr;
			continue;
		}

		bool wireless = device->cableState < 1;
		bool lowBattery = device->batteryLevel < 2;
		bool isBlinking = device->led_delay_on > 0 || device->led_delay_off > 0;
		bool newBlinkData = false;

		// we are now wired or have okay battery level -> stop blinking
		if (isBlinking && !(wireless && lowBattery))
		{
			device->led_delay_on = 0;
			device->led_delay_off = 0;
			newBlinkData = true;
		}
		// we are now wireless and low on battery -> blink
		if (!isBlinking && wireless && lowBattery)
		{
			device->led_delay_on = 100;
			device->led_delay_off = 100;
			newBlinkData = true;
		}

		// Attempt to send rumble no matter what 
		int speed_large = thepad->m_vibrateMotors[0].m_value;
		int speed_small = thepad->m_vibrateMotors[1].m_value;

		device->newVibrateData = device->newVibrateData || device->largeVibrate != speed_large || device->smallVibrate != speed_small || newBlinkData;

		device->largeVibrate = speed_large;
		device->smallVibrate = speed_small;

		if (device->newVibrateData)
		{
			if (SendVibrateData(*device) >= 0)
			{
				device->newVibrateData = false;
			}
		}

		// no data? keep going
		if (status == DS4DataStatus::NoNewData)
			continue;

		else if (status == DS4DataStatus::NewData)
			ProcessDataToPad(*device, *thepad);
	}
}

ds4_pad_handler::DS4DataStatus ds4_pad_handler::GetRawData(DS4Device& device)
{
	std::array<u8, 78> buf{};

	const int res = hid_read(device.hidDevice, buf.data(), device.btCon ? 78 : 64);
	if (res == -1)
	{
		// looks like controller disconnected or read error
		return DS4DataStatus::ReadError;
	}

	// no data? keep going
	if (res == 0)
		return DS4DataStatus::NoNewData;

	// bt controller sends this until 0x02 feature report is sent back (happens on controller init/restart)
	if (device.btCon && buf[0] == 0x1)
	{
		// tells controller to send 0x11 reports
		std::array<u8, 64> buf_error{};
		buf_error[0] = 0x2;
		hid_get_feature_report(device.hidDevice, buf_error.data(), buf_error.size());
		return DS4DataStatus::NoNewData;
	}

	int offset = 0;
	// check report and set offset
	if (device.btCon && buf[0] == 0x11 && res == 78)
	{
		offset = 2;

		const u8 btHdr = 0xA1;
		const u32 crcHdr = CRCPP::CRC::Calculate(&btHdr, 1, crcTable);
		const u32 crcCalc = CRCPP::CRC::Calculate(buf.data(), (DS4_INPUT_REPORT_0x11_SIZE - 4), crcTable, crcHdr);
		const u32 crcReported = GetU32LEData(&buf[DS4_INPUT_REPORT_0x11_SIZE - 4]);
		if (crcCalc != crcReported)
		{
			LOG_WARNING(HLE, "[DS4] Data packet CRC check failed, ignoring! Received 0x%x, Expected 0x%x", crcReported, crcCalc);
			return DS4DataStatus::NoNewData;
		}

	}
	else if (!device.btCon && buf[0] == 0x01 && res == 64)
	{
		// Ds4 Dongle uses this bit to actually report whether a controller is connected
		bool connected = (buf[31] & 0x04) ? false : true;
		if (connected && !device.hasCalibData)
			device.hasCalibData = GetCalibrationData(device);

		offset = 0;
	}
	else
		return DS4DataStatus::NoNewData;

	int battery_offset = offset + DS4_INPUT_REPORT_BATTERY_OFFSET;
	device.cableState = (buf[battery_offset] >> 4) & 0x01;
	device.batteryLevel = buf[battery_offset] & 0x0F;

	if (device.hasCalibData)
	{
		int calibOffset = offset + DS4_INPUT_REPORT_GYRO_X_OFFSET;
		for (int i = 0; i < DS4CalibIndex::COUNT; ++i)
		{
			const s16 rawValue = GetS16LEData(&buf[calibOffset]);
			const s16 calValue = ApplyCalibration(rawValue, device.calibData[i]);
			buf[calibOffset++] = ((u16)calValue >> 0) & 0xFF;
			buf[calibOffset++] = ((u16)calValue >> 8) & 0xFF;
		}
	}
	memcpy(device.padData.data(), &buf[offset], 64);

	return DS4DataStatus::NewData;
}

s32 ds4_pad_handler::EnableGetDevice(const std::string& device) {
	std::lock_guard<std::mutex> lock(handlerLock);
    if (!Init())
        return -1;

    const std::string& serial = GetDeviceId(device);
    if (serial == "")
        return -1;

    DS4Device* dev = nullptr;
    auto cont = controllers.find(serial);
    if (cont == controllers.end()) {
        // if not found, force emplace an 'invalid' device
        auto invalid = controllers.emplace(serial, std::make_unique<DS4Device>());
        invalid.first->second->device_id = static_cast<u32>(controllers.size() - 1);
        dev = invalid.first->second.get();
    }
    else 
        dev = cont->second.get();

    // check if device is already 'enabled'
	auto it = std::find_if(bindings.begin(), bindings.end(), [dev](const auto& d) {return d.first->device_id == dev->device_id; });
	if (it != bindings.end())
		return dev->device_id;

	std::shared_ptr<Pad> pad = std::make_shared<Pad>();
	pad->m_buttons.reserve(button_list.size());
	for (u32 i = 0; i < DS4KeyCodes::LSX; ++i)
		pad->m_buttons.emplace_back(i, button_list.at(i));

	pad->m_buttons.reserve(axis_list.size());
	for (u32 i = LSX; i < DS4KeyCodes::XAccel; ++i)
		pad->m_sticks.emplace_back(i, axis_list.at(i));

	pad->m_sensors.reserve(4);
	// todo: dont hard code this, but config needs sensor support to do this
	pad->m_sensors.emplace_back(0, 512, sensor_list.at(DS4KeyCodes::XAccel));
	pad->m_sensors.emplace_back(0, 399, sensor_list.at(DS4KeyCodes::YAccel));
	pad->m_sensors.emplace_back(0, 512, sensor_list.at(DS4KeyCodes::ZAccel));
	pad->m_sensors.emplace_back(0, 512, sensor_list.at(DS4KeyCodes::XGyro));

	pad->m_vibrateMotors.emplace_back(true, 0);
	pad->m_vibrateMotors.emplace_back(false, 0);

	bindings.emplace_back(dev, std::move(pad));
	return dev->device_id;
}

void ds4_pad_handler::DisableDevice(u32 deviceNumber) {
	std::lock_guard<std::mutex> lock(handlerLock);
    auto it = std::find_if(bindings.begin(), bindings.end(), [deviceNumber](const auto& d) {return d.first->device_id == deviceNumber; });
    if (it == bindings.end())
        return;

    bindings.erase(it);
}

bool ds4_pad_handler::IsDeviceConnected(u32 deviceNumber) {
	std::lock_guard<std::mutex> lock(handlerLock);
	auto it = std::find_if(bindings.begin(), bindings.end(), [deviceNumber](const auto& d) {return d.first->device_id == deviceNumber; });
	if (it == bindings.end())
		return false;

	return it->first->last_conn_status;
}