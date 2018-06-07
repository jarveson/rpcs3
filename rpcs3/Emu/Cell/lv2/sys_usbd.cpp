#include "stdafx.h"
#include "Emu/Memory/vm.h"
#include "Emu/System.h"

#include "Emu/Cell/ErrorCodes.h"
#include "sys_usbd.h"
#include "sys_ppu_thread.h"
#include "sys_event.h"
#include "sys_timer.h"

#include "libusb.h"


LOG_CHANNEL(sys_usbd);

atomic_t<u32> trigger_probe{0};
std::vector<usbDevice> devices = {
	// System devices
    /*usbDevice{SysUsbdDeviceInfo{0x102, 0x2, 0x44}, 50, deviceDescriptor{0x12, 0x1, 0x0200, 0, 0, 0, 0x40, 0x054C, 0x0250, 0x0009, 3, 4, 5, 1}},
    usbDevice{SysUsbdDeviceInfo{0x103, 0x2, 0xAD}, 326, deviceDescriptor{0x12, 0x1, 0x0200, 0xE0, 0x1, 0x1, 0x40, 0x054C, 0x0267, 0x0001, 1, 2, 0, 1}},
	// USB Drive
    usbDevice{SysUsbdDeviceInfo{0x004, 0x2, 0x0}, 50, deviceDescriptor{0x12, 0x1, 0x0200, 0, 0, 0, 0x40, 0x1516, 0x1226, 0x0100, 1, 2, 3, 1}},
	// Skylanders Portal
    usbDevice{SysUsbdDeviceInfo{0x005, 0x2, 0xF8}, 59, deviceDescriptor{0x12, 0x1, 0x0200, 0, 0, 0, 0x20, 0x1430, 0x0150, 0x0100, 1, 2, 0, 1}},

	// G502 mouse
    usbDevice{SysUsbdDeviceInfo{0x006, 0x2, 0xF8}, 77, deviceDescriptor{0x12, 0x1, 0x200, 0,0,0, 0x40, 0x046D, 0xC07D, 0x8802, 1,2,3,1}}*/
};

// Helper function, cellusb likes to shove bits around when sending to some sys_usbd calls
u16 convert_device_handle(u32 device_handle)
{
	return ((device_handle >> 8) & 0xFF00) | (device_handle & 0xFF);
}

/*
 * sys_usbd_initialize changes handle to a semi-unique identifier.
 * TODO: try to get hardware to return not CELL_OK (perhaps set handle to NULL).
 */
s32 sys_usbd_initialize(vm::ptr<u32> handle)
{
	sys_usbd.warning("sys_usbd_initialize(handle=*0x%x)", handle);
	*handle = 0x30003F00;

	int r = libusb_init(NULL);
	if (r < 0)
	{
		sys_usbd.error("libusb init failed, 0x%x", r);
		return CELL_OK;
	}

	libusb_device** devs;

	ssize_t cnt = libusb_get_device_list(NULL, &devs);
	if (cnt < 0)
	{
		sys_usbd.error("libusb init failed, 0x%x", cnt);
		return CELL_OK;
	}


	for (int i = 0; i < cnt; ++i)
	{
		libusb_device* dev = devs[i];
		struct libusb_device_descriptor desc;
		int r = libusb_get_device_descriptor(dev, &desc);
		if (r < 0)
		{
			sys_usbd.error("failed to get device descriptor");
			continue;
		}

		if (desc.idVendor != 0x046D && desc.idProduct != 0xC07D)
			continue;

		devices.emplace_back();
		auto& usbDev = devices.back();
		usbDev.basicDevice.deviceID = (libusb_get_bus_number(dev) << 8) | libusb_get_device_address(dev); 
		usbDev.basicDevice.status   = 2;
		usbDev.basicDevice.unk4     = 0;

		memcpy(&usbDev.descriptor, &desc, sizeof(deviceDescriptor));
	}

	libusb_free_device_list(devs, 1);

	return CELL_OK;
}

s32 sys_usbd_finalize()
{
	sys_usbd.todo("sys_usbd_finalize()");
	libusb_exit(NULL);
	return CELL_OK;
}

s32 sys_usbd_get_device_list(u32 handle, vm::ptr<SysUsbdDeviceInfo> device_list, u32 max_devices)
{
	sys_usbd.warning("sys_usbd_get_device_list(handle=0x%x, device_list=0x%x, max_devices=0x%x)", handle, device_list, max_devices);

	if (device_list == vm::null)
		return CELL_EINVAL;

	if (max_devices < devices.size())
		fmt::throw_exception("fix me");

	for (int i = 0; i < devices.size(); i++)
	{
		device_list[i] = devices[i].basicDevice;
	}
	return devices.size();
}

s32 sys_usbd_register_extra_ldd(u32 handle, vm::ptr<char> name, u32 strLen, u16 vid, u16 pid_min, u16 pid_max)
{
	sys_usbd.warning("sys_usbd_register_extra_ldd(handle=0x%x, name=*0x%x, strLen=%u, vid=0x%04x, pid_min=0x%04x, pid_max=0x%04x)", handle, name, strLen, vid, pid_min, pid_max);

	// this returns lddhandle >0 on success, less than 0 on error
	if (vid == 0x046D)
		trigger_probe++;
	return 9;
}

s32 sys_usbd_get_descriptor_size(u32 handle, u32 device_handle)
{
	sys_usbd.warning("sys_usbd_get_descriptor_size(handle=0x%x, device_handle=0x%x)", handle, device_handle);

	u16 devId = convert_device_handle(device_handle);
	auto check = std::find_if(devices.begin(), devices.end(), [&devId](const auto& dev) { return dev.basicDevice.deviceID == devId; });
	if (check == devices.end())
		return 0;

	libusb_device** devs;

	ssize_t cnt = libusb_get_device_list(NULL, &devs);
	if (cnt < 0)
	{
		sys_usbd.error("libusb init failed, 0x%x", cnt);
		return CELL_OK;
	}

	s32 total = 0;
	for (int i = 0; i < cnt; ++i)
	{
		libusb_device* dev = devs[i];
		struct libusb_device_descriptor desc;
		int r = libusb_get_device_descriptor(dev, &desc);
		if (r < 0)
		{
			sys_usbd.error("failed to get device descriptor");
			continue;
		}

		if (devId != ((libusb_get_bus_number(dev) << 8) | libusb_get_device_address(dev)))
			continue;

		total = desc.bLength;

		for (int j = 0; j < desc.bNumConfigurations; j++)
		{
			struct libusb_config_descriptor* config;
			s32 ret = libusb_get_config_descriptor(dev, j, &config);
			if (LIBUSB_SUCCESS != ret)
			{
				sys_usbd.error("Couldn't retrieve descriptor, 0x%x", ret);
				continue;
			}

			total += config->wTotalLength;

			libusb_free_config_descriptor(config);
		}
		break;
	}

	libusb_free_device_list(devs, 1);
	return total;
}

s32 sys_usbd_get_descriptor(u32 handle, u32 device_handle, vm::ptr<void> descriptor, s64 descSize)
{
	sys_usbd.warning("sys_usbd_get_descriptor(handle=0x%x, device_handle=0x%x, descriptor=*0x%x, descSize=%u)", handle, device_handle, descriptor, descSize);

	u16 devId  = convert_device_handle(device_handle);
	auto check = std::find_if(devices.begin(), devices.end(), [&devId](const auto& dev) { return dev.basicDevice.deviceID == devId; });
	if (check == devices.end())
		return 0;

	libusb_device** devs;

	ssize_t cnt = libusb_get_device_list(NULL, &devs);
	if (cnt < 0)
	{
		sys_usbd.error("libusb init failed, 0x%x", cnt);
		return CELL_OK;
	}

	for (int i = 0; i < cnt; ++i)
	{
		libusb_device* dev = devs[i];
		struct libusb_device_descriptor desc;
		int r = libusb_get_device_descriptor(dev, &desc);
		if (r < 0)
		{
			sys_usbd.error("failed to get device descriptor");
			continue;
		}

		if (devId != ((libusb_get_bus_number(dev) << 8) | libusb_get_device_address(dev)))
			continue;

		if (desc.bLength > descSize)
		{
			sys_usbd.error("blength > descsize");
			break;
		}

		for (int c = 0; c < desc.bNumConfigurations; c++)
		{
			struct libusb_config_descriptor* config;
			s32 ret = libusb_get_config_descriptor(dev, c, &config);
			if (LIBUSB_SUCCESS != ret)
			{
				sys_usbd.error("Couldn't retrieve descriptors");
				break;
			}

			if (descSize != desc.bLength + config->wTotalLength)
			{
				sys_usbd.error("size mismatch, aborting");
				libusb_free_config_descriptor(config);
				break;
			}

			s32 offset = 0;
			std::unique_ptr<u8[]> buf = std::make_unique<u8[]>(descSize);
			memset(buf.get(), 0, descSize);

			memcpy(buf.get(), &desc, desc.bLength);
			offset += desc.bLength;

			memcpy(buf.get() + offset, config, config->bLength);
			offset += config->bLength;

			for (int i = 0; i < config->bNumInterfaces; ++i)
			{
				auto& inf = config->interface[i];
				for (int j = 0; j < inf.num_altsetting; ++j)
				{
					auto& alt = inf.altsetting[j];
					memcpy(buf.get() + offset, &alt, alt.bLength);
					offset += alt.bLength;

					for (int k = 0; k < alt.bNumEndpoints; ++k)
					{
						auto& ep = alt.endpoint[k];
						memcpy(buf.get() + offset, &ep, ep.bLength);
						offset += ep.bLength;
					}

					if (alt.extra_length)
					{
						memcpy(buf.get() + offset, alt.extra, alt.extra_length);
						offset += alt.extra_length;
					}
				}
			}

			// todo: find missing data if needed
			if (offset != descSize)
				sys_usbd.error("ending offset and descSize don't match");

			memcpy(descriptor.get_ptr(), buf.get(), descSize);

			libusb_free_config_descriptor(config);
		}

		break;
	}

	libusb_free_device_list(devs, 1);

	// Just gonna have to hack it for now
	/*unsigned char* desc = (unsigned char*)descriptor.get_ptr();
	if (devId == 0x102)
	{
		unsigned char bytes[] = {0x12, 0x1, 0x0, 0x2, 0x0, 0x0, 0x0, 0x40, 0x4c, 0x5, 0x50, 0x2, 0x9, 0x0, 0x3, 0x4, 0x5, 0x1,
								 0x9, 0x2, 0x20, 0x0, 0x1, 0x1, 0x0, 0x80, 0xfa,
								 0x9, 0x4, 0x0, 0x0, 0x2, 0x8, 0x5, 0x50, 0x0,
								 0x7, 0x5, 0x81, 0x2, 0x0, 0x2, 0x0,
								 0x7, 0x5, 0x2, 0x2, 0x0, 0x2, 0x0};
		memcpy(desc, bytes, descSize);
	}
	else if (devId == 0x103)
	{
		unsigned char bytes[] = {0x12, 0x1, 0x0, 0x2, 0xe0, 0x1, 0x1, 0x40, 0x4c, 0x5, 0x67, 0x2, 0x0, 0x1, 0x1, 0x2, 0x0, 0x1,
								 0x9, 0x2, 0x34, 0x1, 0x4, 0x1, 0x0, 0xc0, 0x0,
								 0x9, 0x4, 0x0, 0x0, 0x3, 0xe0, 0x1, 0x1, 0x0,
								 0x7, 0x5, 0x81, 0x3, 0x40, 0x0, 0x1,
								 0x7, 0x5, 0x2, 0x2, 0x0, 0x2, 0x0,
								 0x7, 0x5, 0x82, 0x2, 0x0, 0x2, 0x0,
								 0x9, 0x4, 0x1, 0x0, 0x2, 0xe0, 0x1, 0x1, 0x0,
								 0x7, 0x5, 0x3, 0x1, 0x0, 0x0, 0x4,
								 0x7, 0x5, 0x83, 0x1, 0x0, 0x0, 0x4,
								 0x9, 0x4, 0x1, 0x1, 0x2, 0xe0, 0x1, 0x1, 0x0,
								 0x7, 0x5, 0x3, 0x1, 0xb, 0x0, 0x4,
								 0x7, 0x5, 0x83, 0x1, 0xb, 0x0, 0x4,
								 0x9, 0x4, 0x1, 0x2, 0x2, 0xe0, 0x1, 0x1, 0x0,
								 0x7, 0x5, 0x3, 0x1, 0x13, 0x0, 0x4,
								 0x7, 0x5, 0x83, 0x1, 0x13, 0x0, 0x4,
								 0x9, 0x4, 0x1, 0x3, 0x2, 0xe0, 0x1, 0x1, 0x0,
								 0x7, 0x5, 0x3, 0x1, 0x1b, 0x0, 0x4,
								 0x7, 0x5, 0x83, 0x1, 0x1b, 0x0, 0x4,
								 0x9, 0x4, 0x1, 0x4, 0x2, 0xe0, 0x1, 0x1, 0x0,
								 0x7, 0x5, 0x3, 0x1, 0x23, 0x0, 0x4,
								 0x7, 0x5, 0x83, 0x1, 0x23, 0x0, 0x4,
								 0x9, 0x4, 0x1, 0x5, 0x2, 0xe0, 0x1, 0x1, 0x0,
								 0x7, 0x5, 0x3, 0x1, 0x33, 0x0, 0x4,
								 0x7, 0x5, 0x83, 0x1, 0x33, 0x0, 0x4,
								 0x9, 0x4, 0x1, 0x6, 0x2, 0xe0, 0x1, 0x1, 0x0,
								 0x7, 0x5, 0x3, 0x1, 0x40, 0x0, 0x4,
								 0x7, 0x5, 0x83, 0x1, 0x40, 0x0, 0x4,
								 0x9, 0x4, 0x2, 0x0, 0x2, 0xe0, 0x1, 0x1, 0x0,
								 0x7, 0x5, 0x4, 0x3, 0x40, 0x0, 0x1,
								 0x7, 0x5, 0x85, 0x3, 0x40, 0x0, 0x1,
								 0x9, 0x4, 0x2, 0x1, 0x2, 0xe0, 0x1, 0x1, 0x0,
								 0x7, 0x5, 0x4, 0x3, 0x80, 0x0, 0x1,
								 0x7, 0x5, 0x85, 0x3, 0x80, 0x0, 0x1,
								 0x9, 0x4, 0x2, 0x2, 0x2, 0xe0, 0x1, 0x1, 0x0,
								 0x7, 0x5, 0x4, 0x3, 0x0, 0x1, 0x1,
								 0x7, 0x5, 0x85, 0x3, 0x0, 0x1, 0x1,
								 0x9, 0x4, 0x2, 0x3, 0x2, 0xe0, 0x1, 0x1, 0x0,
								 0x7, 0x5, 0x4, 0x3, 0x0, 0x4, 0x1,
								 0x7, 0x5, 0x85, 0x3, 0x0, 0x4, 0x1,
								 0x9, 0x4, 0x3, 0x0, 0x0, 0xfe, 0x1, 0x0, 0x0,
								 0x7, 0x21, 0x7, 0x88, 0x13, 0xff, 0x3};

		memcpy(desc, bytes, descSize);
	}
	else if (devId == 0x4)
	{
		// USB Drive
		unsigned char bytes[] = {0x12, 0x1, 0x0, 0x2, 0x0, 0x0, 0x0, 0x40, 0x16, 0x15, 0x26, 0x12, 0x0, 0x1, 0x1, 0x2, 0x3, 0x1,
								 0x9, 0x2, 0x20, 0x0, 0x1, 0x1, 0x0, 0x80, 0xfa,
								 0x9, 0x4, 0x0, 0x0, 0x2, 0x8, 0x6, 0x50, 0x0,
								 0x7, 0x5, 0x81, 0x2, 0x0, 0x2, 0x0,
								 0x7, 0x5, 0x2, 0x2, 0x0, 0x2, 0x0};
		memcpy(desc, bytes, descSize);
	}
	else if (devId == 5)
	{
		// Skylanders Portal
		unsigned char bytes[] = {0x12, 0x1, 0x0, 0x2, 0x0, 0x0, 0x0, 0x20, 0x30, 0x14, 0x50, 0x1, 0x0, 0x1, 0x1, 0x2, 0x0, 0x1,
								 0x9, 0x2, 0x29, 0x0, 0x1, 0x1, 0x0, 0x80, 0x96,
								 0x9, 0x4, 0x0, 0x0, 0x2, 0x3, 0x0, 0x0, 0x0,
								 0x9, 0x21, 0x11, 0x1, 0x0, 0x1, 0x22, 0x1d, 0x0,
								 0x7, 0x5, 0x81, 0x3, 0x20, 0x0, 0x1,
								 0x7, 0x5, 0x1, 0x3, 0x20, 0x0, 0x1};
		memcpy(desc, bytes, descSize);
	}
	else if (devId == 0x6)
	{
		// G502
		
		unsigned char bytes[] = {0x12, 0x1, 0x0, 0x2, 0, 0, 0, 0x40, 0x6D, 0x04, 0x7D, 0xC0, 0x02, 0x88, 1, 2, 3, 1,
			0x9, 0x2, 0x3b, 0x0, 0x2, 0x1, 0x4, 0xA0, 0x96,
			0x9, 0x4, 0x0, 0x0, 1, 3, 1, 2, 0,
			0x9, 0x21, 0x11, 0x1, 0, 1, 0x22, 0x43, 0x0,
			0x7, 0x5, 0x81, 0x3, 0x8, 0x0, 0x1,
			0x9, 0x4, 0x1, 0x0, 0x1, 0x3, 0x0, 0, 0,
			0x9, 0x21, 0x11, 0x1, 0x0, 0x1, 0x22, 0x97, 0,
			0x7, 0x5, 0x82, 0x3, 0x14, 0x0, 0x1};
								
		memcpy(desc, bytes, descSize);
		trigger_probe++;
	}
	else
		return -1;*/
	return CELL_OK;
}

s32 sys_usbd_register_ldd(u32 handle, vm::ptr<char> name, u32 namelen)
{
	sys_usbd.todo("sys_usbd_register_ldd(handle=0x%x, name=0x%x, namelen=0x%x)", handle, name, namelen);
	// return less than 0 for error, otherwise its new handle
	return CELL_OK;
}

s32 sys_usbd_unregister_ldd()
{
	sys_usbd.todo("sys_usbd_unregister_ldd()");
	return CELL_OK;
}

s32 sys_usbd_open_pipe(u32 handle, u32 device_handle, u32 unk1, u32 unk2, u32 unk3, u32 endpoint_address, u32 type)
{
	sys_usbd.todo("sys_usbd_open_pipe(handle=0x%x, device_handle=0x%x, unk1=0x%x, unk2=0x%x, unk3=0x%x, endpoint_address=0x%x, type=0x%x)", handle, device_handle, unk1, unk2, unk3, endpoint_address, type);
	// unk1/2/3 may be configuration/interface/endpoint 'path'
	// type is the 'transfer type' of endpoint
	// returns pipe handle/number
	return CELL_OK;
}

s32 sys_usbd_open_default_pipe(u32 handle, u32 device_handle)
{
	sys_usbd.todo("sys_usbd_open_default_pipe(handle=0x%x, device_handle=0x%x)", handle, device_handle);
	// this opens default control pipe
	u32 device_id = convert_device_handle(device_handle);
	// retrns pipe handle/number
	return CELL_OK;
}

s32 sys_usbd_close_pipe()
{
	sys_usbd.todo("sys_usbd_close_pipe()");
	return CELL_OK;
}

s32 sys_usbd_receive_event(ppu_thread& ppu, u32 handle, vm::ptr<u64> arg1, vm::ptr<u64> arg2, vm::ptr<u64> arg3)
{
	sys_usbd.todo("sys_usbd_receive_event(handle=0x%x, *arg1=0x%x, *arg2=0x%x, *arg3=0x%x)", handle, arg1, arg2, arg3);

	while (trigger_probe != 1)
	{
		sys_timer_sleep(ppu, 1);
	}

	// arg1 can be 1, 2, 3 or 4
	// 4 signifies exit for calling thread
	// 3 signals a status transfer check,
	// 2 is probly disconnect
	// 1 is connection

	/**arg1 = 3;
	*arg2 = 9;
	*arg3 = 7;*/

	// fake connection
	*arg1 = 1;
	// this is device, low byte with high byte shifted << 32
	//*arg2 = 0x100000006;
	*arg2 = 0x000000006;
    // 'status' byte in SysUsbdDeviceInfo
	*arg3 = 0;

	trigger_probe++;

	//_sys_ppu_thread_exit(ppu, CELL_OK);

	// TODO
	/*if (receive_event_called_count == 0)
	{
		*arg1 = 2;
		*arg2 = 5;
		*arg3 = 0;
		receive_event_called_count++;
	}
	else if (receive_event_called_count == 1)
	{
		*arg1 = 1;
		*arg2 = 6;
		*arg3 = 0;
		receive_event_called_count++;
	}
	else
	{
		_sys_ppu_thread_exit(ppu, CELL_OK);
	}*/
	return CELL_OK;
}

s32 sys_usbd_detect_event()
{
	sys_usbd.todo("sys_usbd_detect_event()");
	return CELL_OK;
}

s32 sys_usbd_attach(u32 handle, u32 lddhandle, u32 device_id_high, u32 device_id_low)
{
	sys_usbd.todo("sys_usbd_attach(handle=0x%x, lddhandle=0x%x, device_id_high=0x%x, device_id_low=0x%x)", handle, lddhandle, device_id_high, device_id_low);

	u16 device_id = (device_id_high & 0xFF) << 8 | (device_id_low & 0xFF);


	return CELL_OK;
}

s32 sys_usbd_transfer_data(u32 handle, u32 pipe, vm::ptr<void> buf, u32 wlength, vm::ptr<void> device_request, u32 dr_size)
{
	sys_usbd.todo("sys_usbd_transfer_data(handle=0x%x, pipe=0x%x, buf=*0x%x, wlength=0x%x, device_request=*0x%x, size=0x%x)", handle, pipe, buf, wlength, device_request, dr_size);
	return CELL_OK;
}

s32 sys_usbd_isochronous_transfer_data()
{
	sys_usbd.todo("sys_usbd_isochronous_transfer_data()");
	return CELL_OK;
}

s32 sys_usbd_get_transfer_status(u32 handle, u32 a2, u32 a3, u32 a4, u32 a5)
{
	sys_usbd.todo("sys_usbd_get_transfer_status(handle=0x%x, a2=0x%x, a3=0x%x, a4=0x%x, a5=0x%x)", handle, a2, a3, a4, a5);
	return CELL_OK;
}

s32 sys_usbd_get_isochronous_transfer_status()
{
	sys_usbd.todo("sys_usbd_get_isochronous_transfer_status()");
	return CELL_OK;
}

s32 sys_usbd_get_device_location()
{
	sys_usbd.todo("sys_usbd_get_device_location()");
	return CELL_OK;
}

s32 sys_usbd_send_event()
{
	sys_usbd.todo("sys_usbd_send_event()");
	return CELL_OK;
}

s32 sys_usbd_event_port_send()
{
	sys_usbd.todo("sys_usbd_event_port_send()");
	return CELL_OK;
}

s32 sys_usbd_allocate_memory()
{
	sys_usbd.todo("sys_usbd_allocate_memory()");
	return CELL_OK;
}

s32 sys_usbd_free_memory()
{
	sys_usbd.todo("sys_usbd_free_memory()");
	return CELL_OK;
}

s32 sys_usbd_get_device_speed()
{
	sys_usbd.todo("sys_usbd_get_device_speed()");
	return CELL_OK;
}
