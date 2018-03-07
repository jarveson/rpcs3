#pragma once

#include "Emu/Io/PadHandler.h"

class NullPadHandler final : public PadHandlerBase
{
public:
	NullPadHandler() : PadHandlerBase(pad_handler::null, "Null", "") {};
	void init_config(pad_config* /*cfg*/, const std::string& /*name*/) override
	{
	}

	std::vector<std::string> ListDevices() override
	{
		std::vector<std::string> nulllist;
		nulllist.push_back("Default Null Device");
		return nulllist;
	}

	void ThreadProc() override
	{
	}

	bool IsDeviceConnected(u32 deviceNumber) override { return false; }
	std::shared_ptr<Pad> GetDeviceData(u32) override { return nullptr; }

    void SetVibrate(u32, u32, u32) override { return; }

	u32 GetNumPads() override { return 0; }

	s32 EnableGetDevice(const std::string&) override { return 0; }
	void DisableDevice(u32) override {};
    void SetRGBData(u32, u8, u8, u8) override {};
};
