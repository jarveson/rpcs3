#include "stdafx.h"
#include "Emu/System.h"
#include "Emu/IdManager.h"
#include "Emu/Cell/PPUModule.h"
#include "Emu/Io/PadThread.h"

#include "cellPad.h"

#include <array>

extern logs::channel sys_io;

struct cellPadConfig
{
	u32 max_connect{0};
	std::array<bool, CELL_PAD_MAX_PORT_NUM> buffer_cleared{{false}};
	std::array<bool, CELL_PAD_MAX_PORT_NUM> last_conn_status{{false}};
	std::array<PadDataBuffer, CELL_PAD_MAX_PORT_NUM> pad_buffer;
	std::chrono::time_point<steady_clock> last_update[CELL_PAD_MAX_PORT_NUM]{};
	// this is used to force the game to see all controllers as disconnected to begin with
	bool info_called{false};
};

s32 cellPadInit(u32 max_connect)
{
	sys_io.warning("cellPadInit(max_connect=%d)", max_connect);

	const auto config = fxm::make<cellPadConfig>();
	if (!config)
		return CELL_PAD_ERROR_ALREADY_INITIALIZED;

	const auto handler = fxm::get<PadThread>();
	if (!handler)
		return CELL_PAD_ERROR_FATAL;

	config->max_connect = std::min(max_connect, CELL_PAD_MAX_PORT_NUM);

	return CELL_OK;
}

s32 cellPadEnd()
{
	sys_io.notice("cellPadEnd()");

	if (!fxm::remove<cellPadConfig>())
		return CELL_PAD_ERROR_UNINITIALIZED;

	return CELL_OK;
}

s32 cellPadClearBuf(u32 port_no)
{
	sys_io.trace("cellPadClearBuf(port_no=%d)", port_no);

	const auto config = fxm::get<cellPadConfig>();
	if (!config)
		return CELL_PAD_ERROR_UNINITIALIZED;

	const auto handler = fxm::get<PadThread>();
	if (!handler)
		return CELL_PAD_ERROR_FATAL;

	if (port_no >= config->max_connect)
		return CELL_PAD_ERROR_INVALID_PARAMETER;

	config->pad_buffer[port_no] = PadDataBuffer();

	return CELL_OK;
}

s32 cellPadGetData(u32 port_no, vm::ptr<CellPadData> data)
{
	sys_io.trace("cellPadGetData(port_no=%d, data=*0x%x)", port_no, data);

	const auto config = fxm::get<cellPadConfig>();
	if (!config)
		return CELL_PAD_ERROR_UNINITIALIZED;

	const auto handler = fxm::get<PadThread>();
	if (!handler)
		return CELL_PAD_ERROR_FATAL;

	if (port_no >= config->max_connect)
		return CELL_PAD_ERROR_INVALID_PARAMETER;

	if (handler->IsOverlayInUse()) {
		data->len = CELL_PAD_LEN_NO_CHANGE;
		return CELL_OK;
	}

	const PadStatus padStatus = handler->GetPadStatus(port_no);
	const PadDataBuffer padData = handler->GetPadData(port_no);

	//We have a choice here of NO_DEVICE or READ_FAILED...lets try no device for now
	if (!(padStatus.m_port_status & CELL_PAD_STATUS_CONNECTED))
		return CELL_PAD_ERROR_NO_DEVICE;

	// compare new with old data, then copy it over
	const bool btnChanged = memcmp(&padData, &config->pad_buffer, sizeof(PadDataBuffer)) != 0;
	memcpy(&config->pad_buffer, &padData, sizeof(PadDataBuffer));

	const bool buffer_cleared = config->buffer_cleared[port_no];
	
	// the real hardware only fills the buffer up to "len" elements (16 bit each)
	if (padStatus.m_port_setting & CELL_PAD_SETTING_SENSOR_ON)
	{
		// todo: move me somewhere else
		// report back new data every ~10 ms even if the input doesn't change
		// this is observed behaviour when using a Dualshock 3 controller
		const std::chrono::time_point<steady_clock> now = steady_clock::now();

		if (btnChanged || buffer_cleared || (std::chrono::duration_cast<std::chrono::milliseconds>(now - config->last_update[port_no]).count() >= 10))
		{
			data->len = CELL_PAD_LEN_CHANGE_SENSOR_ON;
			config->last_update[port_no] = now;
		}
		else
		{
			data->len = CELL_PAD_LEN_NO_CHANGE;
		}
	}
	else if (btnChanged || buffer_cleared)
	{
		// only give back valid data if a controller state changed
		data->len = (padStatus.m_port_setting & CELL_PAD_SETTING_PRESS_ON) ? CELL_PAD_LEN_CHANGE_PRESS_ON : CELL_PAD_LEN_CHANGE_DEFAULT;
	}
	else
	{
		// report no state changes
		data->len = CELL_PAD_LEN_NO_CHANGE;
	}

	config->buffer_cleared[port_no] = false;

	// only update parts of the output struct depending on the controller setting
	if (data->len > CELL_PAD_LEN_NO_CHANGE)
	{
		memset(data->button, 0, sizeof(data->button));

		data->button[0] = 0x0; // always 0
		// bits 15-8 reserved, 7-4 = 0x7, 3-0: data->len/2;
		data->button[1] = (0x7 << 4) | std::min(data->len / 2, 15);

		data->button[CELL_PAD_BTN_OFFSET_DIGITAL1] = padData.m_digital_1;
		data->button[CELL_PAD_BTN_OFFSET_DIGITAL2] = padData.m_digital_2;
		data->button[CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_X] = padData.m_analog_right_x;
		data->button[CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_Y] = padData.m_analog_right_y;
		data->button[CELL_PAD_BTN_OFFSET_ANALOG_LEFT_X] = padData.m_analog_left_x;
		data->button[CELL_PAD_BTN_OFFSET_ANALOG_LEFT_Y] = padData.m_analog_left_y;
		data->button[CELL_PAD_BTN_OFFSET_PRESS_RIGHT] = padData.m_press_right;
		data->button[CELL_PAD_BTN_OFFSET_PRESS_LEFT] = padData.m_press_left;
		data->button[CELL_PAD_BTN_OFFSET_PRESS_UP] = padData.m_press_up;
		data->button[CELL_PAD_BTN_OFFSET_PRESS_DOWN] = padData.m_press_down;
	}

	if (data->len >= CELL_PAD_LEN_CHANGE_PRESS_ON)
	{
		data->button[CELL_PAD_BTN_OFFSET_PRESS_TRIANGLE] = padData.m_press_triangle;
		data->button[CELL_PAD_BTN_OFFSET_PRESS_CIRCLE] = padData.m_press_circle;
		data->button[CELL_PAD_BTN_OFFSET_PRESS_CROSS] = padData.m_press_cross;
		data->button[CELL_PAD_BTN_OFFSET_PRESS_SQUARE] = padData.m_press_square;
		data->button[CELL_PAD_BTN_OFFSET_PRESS_L1] = padData.m_press_L1;
		data->button[CELL_PAD_BTN_OFFSET_PRESS_L2] = padData.m_press_L2;
		data->button[CELL_PAD_BTN_OFFSET_PRESS_R1] = padData.m_press_R1;
		data->button[CELL_PAD_BTN_OFFSET_PRESS_R2] = padData.m_press_R2;
	}

	if (data->len == CELL_PAD_LEN_CHANGE_SENSOR_ON)
	{
		data->button[CELL_PAD_BTN_OFFSET_SENSOR_X] = padData.m_sensor_x;
		data->button[CELL_PAD_BTN_OFFSET_SENSOR_Y] = padData.m_sensor_y;
		data->button[CELL_PAD_BTN_OFFSET_SENSOR_Z] = padData.m_sensor_z;
		data->button[CELL_PAD_BTN_OFFSET_SENSOR_G] = padData.m_sensor_g;
	}

	return CELL_OK;
}

s32 cellPadPeriphGetInfo(vm::ptr<CellPadPeriphInfo> info)
{
	sys_io.trace("cellPadPeriphGetInfo(info=*0x%x)", info);

	const auto config = fxm::get<cellPadConfig>();
	if (!config)
		return CELL_PAD_ERROR_UNINITIALIZED;

	const auto handler = fxm::get<PadThread>();
	if (!handler)
		return CELL_PAD_ERROR_FATAL;

	std::memset(info.get_ptr(), 0, sizeof(CellPadPeriphInfo));

	u32 now_connected = 0;

	// TODO: Support other types of controllers
	for (u32 i = 0; i < CELL_PAD_MAX_PORT_NUM; ++i)
	{
		const PadStatus padStatus = handler->GetPadStatus(i);

		info->port_setting[i] = padStatus.m_port_setting;

		if (config->info_called) {
			info->port_status[i] = padStatus.m_port_status;
			info->device_capability[i] = padStatus.m_device_capability;
			info->device_type[i] = padStatus.m_device_type;

			info->pclass_type[i] = CELL_PAD_PCLASS_TYPE_STANDARD;
			info->pclass_profile[i] = 0x0;

			bool connected = padStatus.m_port_status & CELL_PAD_STATUS_CONNECTED;
			if (config->last_conn_status[i] != connected)
				info->port_status[i] &= CELL_PAD_STATUS_ASSIGN_CHANGES;

			config->last_conn_status[i] = connected;
			now_connected += connected ? 1 : 0;

			if (config->max_connect == now_connected) break;
		}
	}

	info->max_connect = config->max_connect;
	info->now_connect = now_connected;
	info->system_info = handler->IsOverlayInUse();

	config->info_called = true;

	return CELL_OK;
}

s32 cellPadPeriphGetData(u32 port_no, vm::ptr<CellPadPeriphData> data)
{
	sys_io.trace("cellPadPeriphGetData(port_no=%d, data=*0x%x)", port_no, data);

	const auto config = fxm::get<cellPadConfig>();
	if (!config)
		return CELL_PAD_ERROR_UNINITIALIZED;

	const auto handler = fxm::get<PadThread>();
	if (!handler)
		return CELL_PAD_ERROR_FATAL;

	if (port_no >= config->max_connect)
		return CELL_PAD_ERROR_INVALID_PARAMETER;

	const PadStatus padStatus = handler->GetPadStatus(port_no);

	if (!(padStatus.m_port_status & CELL_PAD_STATUS_CONNECTED))
		return CELL_PAD_ERROR_NO_DEVICE;

	// todo: support for 'unique' controllers, which goes in offsets 24+ in padData
	data->pclass_type = CELL_PAD_PCLASS_TYPE_STANDARD;
	data->pclass_profile = 0x0;
	return cellPadGetData(port_no, vm::get_addr(&data->cellpad_data));
}

s32 cellPadGetRawData(u32 port_no, vm::ptr<CellPadData> data)
{
	fmt::throw_exception("Unimplemented" HERE);
}

s32 cellPadGetDataExtra(u32 port_no, vm::ptr<u32> device_type, vm::ptr<CellPadData> data)
{
	sys_io.trace("cellPadGetDataExtra(port_no=%d, device_type=*0x%x, device_type=*0x%x)", port_no, device_type, data);

	const auto config = fxm::get<cellPadConfig>();
	if (!config)
		return CELL_PAD_ERROR_UNINITIALIZED;

	const auto handler = fxm::get<PadThread>();
	if (!handler)
		return CELL_PAD_ERROR_FATAL;

	if (port_no >= config->max_connect)
		return CELL_PAD_ERROR_INVALID_PARAMETER;

	const PadStatus padStatus = handler->GetPadStatus(port_no);

	if (!(padStatus.m_port_status & CELL_PAD_STATUS_CONNECTED))
		return CELL_PAD_ERROR_NO_DEVICE;

	// TODO: This is used just to get data from a BD/CEC remote,
	// but if the port isnt a remote, device type is set to 0 and just regular cellPadGetData is returned

	*device_type = 0;

	// set BD data before just incase
	data->button[24] = 0x0;
	data->button[25] = 0x0;

	return cellPadGetData(port_no, data);
}

s32 cellPadSetActDirect(u32 port_no, vm::ptr<CellPadActParam> param)
{
	sys_io.trace("cellPadSetActDirect(port_no=%d, param=*0x%x)", port_no, param);

	const auto config = fxm::get<cellPadConfig>();
	if (!config)
		return CELL_PAD_ERROR_UNINITIALIZED;

	const auto handler = fxm::get<PadThread>();
	if (!handler)
		return CELL_PAD_ERROR_FATAL;

	if (port_no >= config->max_connect)
		return CELL_PAD_ERROR_INVALID_PARAMETER;

	const PadStatus padStatus = handler->GetPadStatus(port_no);

	if (!(padStatus.m_port_status & CELL_PAD_STATUS_CONNECTED))
		return CELL_PAD_ERROR_NO_DEVICE;

	handler->SetRumble(port_no, param->motor[1], param->motor[0] > 0);

	return CELL_OK;
}

s32 cellPadGetInfo(vm::ptr<CellPadInfo> info)
{
	sys_io.trace("cellPadGetInfo(info=*0x%x)", info);

	const auto config = fxm::get<cellPadConfig>();
	if (!config)
		return CELL_PAD_ERROR_UNINITIALIZED;

	const auto handler = fxm::get<PadThread>();
	if (!handler)
		return CELL_PAD_ERROR_FATAL;

	std::memset(info.get_ptr(), 0, sizeof(CellPadInfo));

	u32 now_connected = 0;
	if (config->info_called) {
		// This function technically should return up to CELL_MAX_PADS, but im not sure how that is supposed to work
		// as internally the library only supports upto 7 devices
		for (u32 i = 0; i < CELL_PAD_MAX_PORT_NUM /*CELL_MAX_PADS*/; ++i) {
			const PadStatus padStatus = handler->GetPadStatus(i);

			info->status[i] = padStatus.m_port_status;
			info->product_id[i] = 0x0268;
			info->vendor_id[i] = 0x054C;

			bool connected = padStatus.m_port_status & CELL_PAD_STATUS_CONNECTED;
			if (config->last_conn_status[i] != connected)
				info->status[i] &= CELL_PAD_STATUS_ASSIGN_CHANGES;

			config->last_conn_status[i] = connected;
			now_connected += connected ? 1 : 0;
			if (config->max_connect == now_connected) break;
		}
	}

	info->max_connect = config->max_connect;
	info->now_connect = now_connected;
	info->system_info = handler->IsOverlayInUse();
	config->info_called = true;

	return CELL_OK;
}

s32 cellPadGetInfo2(vm::ptr<CellPadInfo2> info)
{
	sys_io.trace("cellPadGetInfo2(info=*0x%x)", info);

	const auto config = fxm::get<cellPadConfig>();
	if (!config)
		return CELL_PAD_ERROR_UNINITIALIZED;

	const auto handler = fxm::get<PadThread>();
	if (!handler)
		return CELL_PAD_ERROR_FATAL;

	std::memset(info.get_ptr(), 0, sizeof(CellPadInfo2));

	u32 now_connected = 0;

	for (u32 i=0; i<CELL_PAD_MAX_PORT_NUM; ++i)
	{
		const PadStatus padStatus = handler->GetPadStatus(i);
		info->port_setting[i] = padStatus.m_port_setting;

		if (config->info_called) {
			info->port_status[i] = padStatus.m_port_status;

			info->device_capability[i] = padStatus.m_device_capability;
			info->device_type[i] = padStatus.m_device_type;

			bool connected = padStatus.m_port_status & CELL_PAD_STATUS_CONNECTED;
			if (config->last_conn_status[i] != connected)
				info->port_status[i] &= CELL_PAD_STATUS_ASSIGN_CHANGES;

			config->last_conn_status[i] = connected;
			now_connected += connected ? 1 : 0;
			if (config->max_connect == now_connected) break;
		}
	}

	info->max_connect = config->max_connect;
	info->now_connect = now_connected;
	info->system_info = handler->IsOverlayInUse();
	config->info_called = true;

	return CELL_OK;
}

s32 cellPadGetCapabilityInfo(u32 port_no, vm::ptr<CellCapabilityInfo> info)
{
	sys_io.trace("cellPadGetCapabilityInfo(port_no=%d, data_addr:=0x%x)", port_no, info.addr());

	const auto config = fxm::get<cellPadConfig>();
	if (!config)
		return CELL_PAD_ERROR_UNINITIALIZED;

	const auto handler = fxm::get<PadThread>();
	if (!handler)
		return CELL_PAD_ERROR_FATAL;

	if (port_no >= config->max_connect)
		return CELL_PAD_ERROR_INVALID_PARAMETER;

	const PadStatus padStatus = handler->GetPadStatus(port_no);

	if (!(padStatus.m_port_status & CELL_PAD_STATUS_CONNECTED))
		return CELL_PAD_ERROR_NO_DEVICE;

	//Should return the same as device capability mask, psl1ght has it backwards in pad.h
	info->info[0] = padStatus.m_device_capability;

	return CELL_OK;
}

s32 cellPadSetPortSetting(u32 port_no, u32 port_setting)
{
	sys_io.trace("cellPadSetPortSetting(port_no=%d, port_setting=0x%x)", port_no, port_setting);

	const auto config = fxm::get<cellPadConfig>();
	if (!config)
		return CELL_PAD_ERROR_UNINITIALIZED;

	const auto handler = fxm::get<PadThread>();
	if (!handler)
		return CELL_PAD_ERROR_FATAL;

	if (port_no >= config->max_connect)
		return CELL_PAD_ERROR_INVALID_PARAMETER;

	const PadStatus padStatus = handler->GetPadStatus(port_no);

	handler->SetPortSetting(port_no, port_setting);

	return CELL_OK;
}

s32 cellPadInfoPressMode(u32 port_no)
{
	sys_io.trace("cellPadInfoPressMode(port_no=%d)", port_no);

	const auto config = fxm::get<cellPadConfig>();
	if (!config)
		return CELL_PAD_ERROR_UNINITIALIZED;

	const auto handler = fxm::get<PadThread>();
	if (!handler)
		return CELL_PAD_ERROR_FATAL;

	if (port_no >= config->max_connect)
		return CELL_PAD_ERROR_INVALID_PARAMETER;

	const PadStatus padStatus = handler->GetPadStatus(port_no);

	if (!(padStatus.m_port_status & CELL_PAD_STATUS_CONNECTED))
		return CELL_PAD_ERROR_NO_DEVICE;

	return (padStatus.m_device_capability & CELL_PAD_CAPABILITY_PRESS_MODE) > 0;
}

s32 cellPadInfoSensorMode(u32 port_no)
{
	sys_io.trace("cellPadInfoSensorMode(port_no=%d)", port_no);

	const auto config = fxm::get<cellPadConfig>();
	if (!config)
		return CELL_PAD_ERROR_UNINITIALIZED;

	const auto handler = fxm::get<PadThread>();
	if (!handler)
		return CELL_PAD_ERROR_FATAL;

	if (port_no >= config->max_connect)
		return CELL_PAD_ERROR_INVALID_PARAMETER;

	const PadStatus padStatus = handler->GetPadStatus(port_no);

	if (!(padStatus.m_port_status & CELL_PAD_STATUS_CONNECTED))
		return CELL_PAD_ERROR_NO_DEVICE;

	return (padStatus.m_device_capability & CELL_PAD_CAPABILITY_SENSOR_MODE) > 0;
}

s32 cellPadSetPressMode(u32 port_no, u32 mode)
{
	sys_io.trace("cellPadSetPressMode(port_no=%d, mode=%d)", port_no, mode);

	const auto config = fxm::get<cellPadConfig>();
	if (!config)
		return CELL_PAD_ERROR_UNINITIALIZED;

	const auto handler = fxm::get<PadThread>();
	if (!handler)
		return CELL_PAD_ERROR_FATAL;

	if (port_no >= config->max_connect)
		return CELL_PAD_ERROR_INVALID_PARAMETER;

	const PadStatus padStatus = handler->GetPadStatus(port_no);
	u32 port_setting = padStatus.m_port_setting;
	if (mode)
		port_setting |= CELL_PAD_SETTING_PRESS_ON;
	else
		port_setting &= ~CELL_PAD_SETTING_PRESS_ON;

	handler->SetPortSetting(port_no, port_setting);

	return CELL_OK;
}

s32 cellPadSetSensorMode(u32 port_no, u32 mode)
{
	sys_io.trace("cellPadSetSensorMode(port_no=%d, mode=%d)", port_no, mode);

	const auto config = fxm::get<cellPadConfig>();
	if (!config)
		return CELL_PAD_ERROR_UNINITIALIZED;

	const auto handler = fxm::get<PadThread>();
	if (!handler)
		return CELL_PAD_ERROR_FATAL;

	if (mode != 0 && mode != 1)
		return CELL_PAD_ERROR_INVALID_PARAMETER;

	if (port_no >= config->max_connect)
		return CELL_PAD_ERROR_INVALID_PARAMETER;

	const PadStatus padStatus = handler->GetPadStatus(port_no);
	u32 port_setting = padStatus.m_port_setting;

	if (mode)
		port_setting |= CELL_PAD_SETTING_SENSOR_ON;
	else
		port_setting &= ~CELL_PAD_SETTING_SENSOR_ON;

	handler->SetPortSetting(port_no, port_setting);

	return CELL_OK;
}

s32 cellPadLddRegisterController()
{
	sys_io.todo("cellPadLddRegisterController()");

	const auto config = fxm::get<cellPadConfig>();
	if (!config)
		return CELL_PAD_ERROR_UNINITIALIZED;

	const auto handler = fxm::get<PadThread>();
	if (!handler)
		return CELL_PAD_ERROR_FATAL;

	return CELL_OK;
}

s32 cellPadLddDataInsert(s32 handle, vm::ptr<CellPadData> data)
{
	sys_io.todo("cellPadLddDataInsert(handle=%d, data=*0x%x)", handle, data);

	const auto config = fxm::get<cellPadConfig>();
	if (!config)
		return CELL_PAD_ERROR_UNINITIALIZED;

	const auto handler = fxm::get<PadThread>();
	if (!handler)
		return CELL_PAD_ERROR_FATAL;

	return CELL_OK;
}

s32 cellPadLddGetPortNo(s32 handle)
{
	sys_io.todo("cellPadLddGetPortNo(handle=%d)", handle);

	if (handle < 0)
		return CELL_PAD_ERROR_INVALID_PARAMETER;

	const auto config = fxm::get<cellPadConfig>();
	if (!config)
		return CELL_PAD_ERROR_UNINITIALIZED;

	const auto handler = fxm::get<PadThread>();
	if (!handler)
		return CELL_PAD_ERROR_FATAL;

	// CELL_OK would return port 0 (Nascar [BLUS30932] stopped looking for custom controllers after a few seconds, fixing normal input)
	return CELL_PAD_ERROR_EBUSY;
}

s32 cellPadLddUnregisterController(s32 handle)
{
	sys_io.todo("cellPadLddUnregisterController(handle=%d)", handle);

	const auto config = fxm::get<cellPadConfig>();
	if (!config)
		return CELL_PAD_ERROR_UNINITIALIZED;

	const auto handler = fxm::get<PadThread>();
	if (!handler)
		return CELL_PAD_ERROR_FATAL;

	return CELL_OK;
}


void cellPad_init()
{
	REG_FUNC(sys_io, cellPadInit);
	REG_FUNC(sys_io, cellPadEnd);
	REG_FUNC(sys_io, cellPadClearBuf);
	REG_FUNC(sys_io, cellPadGetData);
	REG_FUNC(sys_io, cellPadGetRawData); //
	REG_FUNC(sys_io, cellPadGetDataExtra);
	REG_FUNC(sys_io, cellPadSetActDirect);
	REG_FUNC(sys_io, cellPadGetInfo); //
	REG_FUNC(sys_io, cellPadGetInfo2);
	REG_FUNC(sys_io, cellPadPeriphGetInfo);
	REG_FUNC(sys_io, cellPadPeriphGetData);
	REG_FUNC(sys_io, cellPadSetPortSetting);
	REG_FUNC(sys_io, cellPadInfoPressMode); //
	REG_FUNC(sys_io, cellPadInfoSensorMode); //
	REG_FUNC(sys_io, cellPadSetPressMode); //
	REG_FUNC(sys_io, cellPadSetSensorMode); //
	REG_FUNC(sys_io, cellPadGetCapabilityInfo); //
	REG_FUNC(sys_io, cellPadLddRegisterController);
	REG_FUNC(sys_io, cellPadLddDataInsert);
	REG_FUNC(sys_io, cellPadLddGetPortNo);
	REG_FUNC(sys_io, cellPadLddUnregisterController);
}
