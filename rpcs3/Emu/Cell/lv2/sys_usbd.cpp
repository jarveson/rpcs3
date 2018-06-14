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

atomic_t<bool> trigger_probe{false};
libusb_device_handle* libusb_dev_handle = nullptr;
std::vector<u32> pipe_mapping;
u64 devid;
u32 len = 0;
u16 d_vid = 0x046D;
u16 d_pid = 0xC01E;
struct ProbeData
{
	u64 arg1;
	u64 arg2;
	u64 arg3;
} probe_data;

std::vector<usbDevice> devices          = {
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

	// todo: use a libusb context to avoid issues with other libs
	int r = libusb_init(NULL);
	if (r != LIBUSB_SUCCESS)
	{
		sys_usbd.error("libusb init failed, %d", r);
		return CELL_OK;
	}

	libusb_set_option(NULL, LIBUSB_OPTION_USE_USBDK);

	libusb_device** devs;

	ssize_t cnt = libusb_get_device_list(NULL, &devs);
	if (cnt < 0)
	{
		sys_usbd.error("libusb get_dev_list failed, %d", cnt);
		return CELL_OK;
	}

	for (int i = 0; i < cnt; ++i)
	{
		libusb_device* dev = devs[i];
		struct libusb_device_descriptor desc;
		int r = libusb_get_device_descriptor(dev, &desc);
		if (r != LIBUSB_SUCCESS)
		{
			sys_usbd.error("failed to get device descriptor, %d", r);
			continue;
		}

		if (desc.idVendor != d_vid || desc.idProduct != d_pid)
			continue;
		devid = (static_cast<u64>(libusb_get_bus_number(dev)) << 32) | libusb_get_device_address(dev);

		devices.emplace_back();
		auto& usbDev                = devices.back();
		usbDev.basicDevice.deviceID = (libusb_get_bus_number(dev) << 8) | libusb_get_device_address(dev);
		usbDev.basicDevice.status   = 2;
		usbDev.basicDevice.unk4     = 0;

		memcpy(&usbDev.descriptor, &desc, desc.bLength);
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
	if (vid == d_vid && pid_max == d_pid)
	{
		probe_data.arg1 = 1;
		probe_data.arg2 = devid;
		probe_data.arg3 = 0;
		trigger_probe = true;
	}
	return 9;
}

s32 sys_usbd_get_descriptor_size(u32 handle, u32 device_handle)
{
	sys_usbd.warning("sys_usbd_get_descriptor_size(handle=0x%x, device_handle=0x%x)", handle, device_handle);

	u16 devId  = convert_device_handle(device_handle);
	auto check = std::find_if(devices.begin(), devices.end(), [&devId](const auto& dev) { return dev.basicDevice.deviceID == devId; });
	if (check == devices.end())
		return 0;

	libusb_device** devs;

	ssize_t cnt = libusb_get_device_list(NULL, &devs);
	if (cnt < 0)
	{
		sys_usbd.error("libusb get_dev_list failed, %d", cnt);
		return CELL_OK;
	}

	s32 total = 0;
	for (int i = 0; i < cnt; ++i)
	{
		libusb_device* dev = devs[i];
		struct libusb_device_descriptor desc;
		int r = libusb_get_device_descriptor(dev, &desc);
		if (r != LIBUSB_SUCCESS)
		{
			sys_usbd.error("failed to get device descriptor %d", r);
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
				sys_usbd.error("Couldn't retrieve descriptor, %d", ret);
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
		sys_usbd.error("libusb get_dev_list failed, %d", cnt);
		return CELL_OK;
	}

	for (int i = 0; i < cnt; ++i)
	{
		libusb_device* dev = devs[i];
		struct libusb_device_descriptor desc;
		int r = libusb_get_device_descriptor(dev, &desc);
		if (r != LIBUSB_SUCCESS)
		{
			sys_usbd.error("failed to get device descriptor %d", r);
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
				sys_usbd.error("Couldn't retrieve descriptors. %d", ret);
				break;
			}

			if (descSize != desc.bLength + config->wTotalLength)
			{
				sys_usbd.error("size mismatch, aborting");
				libusb_free_config_descriptor(config);
				break;
			}

			s32 offset                = 0;
			std::unique_ptr<u8[]> buf = std::make_unique<u8[]>(descSize);

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
	sys_usbd.todo(
	    "sys_usbd_open_pipe(handle=0x%x, device_handle=0x%x, unk1=0x%x, unk2=0x%x, unk3=0x%x, endpoint_address=0x%x, type=0x%x)", handle, device_handle, unk1, unk2, unk3, endpoint_address, type);
	// unk1/2/3 may be configuration/interface/endpoint 'path'
	// type is the 'transfer type' of endpoint
	// returns pipe handle/number

	pipe_mapping.push_back(endpoint_address);

	return pipe_mapping.size() - 1;
}

s32 sys_usbd_open_default_pipe(u32 handle, u32 device_handle)
{
	sys_usbd.todo("sys_usbd_open_default_pipe(handle=0x%x, device_handle=0x%x)", handle, device_handle);
	// this opens default control pipe
	u32 device_id = convert_device_handle(device_handle);
	// retrns pipe handle/number
	pipe_mapping.push_back(0);
	return pipe_mapping.size() - 1;
}

s32 sys_usbd_close_pipe()
{
	sys_usbd.todo("sys_usbd_close_pipe()");
	return CELL_OK;
}

s32 sys_usbd_receive_event(ppu_thread& ppu, u32 handle, vm::ptr<u64> arg1, vm::ptr<u64> arg2, vm::ptr<u64> arg3)
{
	sys_usbd.todo("sys_usbd_receive_event(handle=0x%x, *arg1=0x%x, *arg2=0x%x, *arg3=0x%x)", handle, arg1, arg2, arg3);

	while (!trigger_probe)
	{
		sys_timer_sleep(ppu, 1);
	}

	// arg1 can be 1, 2, 3 or 4
	// 4 signifies exit for calling thread
	// 3 signals a status transfer check,
	// 2 is probly disconnect
	// 1 is connection

	// arg2 is deviceid, low byte low, with high byte shifted << 32
	// arg3 is 'status' byte in SysUsbdDeviceInfo, looks to be ignored for arg1 == 3

	// fake connection
	/*arg1 = 1;
	*arg2 = 0x100000004;
	*arg3 = 0;*/
	*arg1         = probe_data.arg1;
	*arg2         = probe_data.arg2;
	*arg3         = probe_data.arg3;
	trigger_probe = false;

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

	const u16 device_id = (device_id_high & 0xFF) << 8 | (device_id_low & 0xFF);

	libusb_device** devs;

	ssize_t cnt = libusb_get_device_list(NULL, &devs);
	if (cnt < 0)
	{
		sys_usbd.error("libusb get_dev_list failed, %d", cnt);
		return CELL_OK;
	}

	for (int i = 0; i < cnt; ++i)
	{
		libusb_device* dev = devs[i];
		struct libusb_device_descriptor desc;
		int r = libusb_get_device_descriptor(dev, &desc);
		if (r != LIBUSB_SUCCESS)
		{
			sys_usbd.error("failed to get device descriptor, %d", r);
			continue;
		}

		if (((libusb_get_bus_number(dev) << 8) | libusb_get_device_address(dev)) != device_id)
			continue;

		r = libusb_open(dev, &libusb_dev_handle);
		if (r != LIBUSB_SUCCESS)
		{
			sys_usbd.error("open dev failed %d", r);
			break;
		}
		else
			sys_usbd.success("opened");

		libusb_set_auto_detach_kernel_driver(libusb_dev_handle, 1);

		// check if os has already set a config
		// if it has, claim interfaces on it

		struct libusb_config_descriptor* config;
		r = libusb_get_active_config_descriptor(dev, &config);
		if (r == LIBUSB_ERROR_NOT_FOUND)
		{
			// purposely empty, no config set, wait to claim till set_config
		}
		else if (r == 0)
		{
			for (int i = 0; i < config->bNumInterfaces; ++i)
			{
				auto& inf = config->interface[i];
				for (int j = 0; j < inf.num_altsetting; ++j)
				{
					auto& alt = inf.altsetting[j];
					r         = libusb_claim_interface(libusb_dev_handle, alt.bInterfaceNumber);
					if (r != LIBUSB_SUCCESS)
					{
						sys_usbd.error("libusb_claim_interface failed %d", r);
					}
				}
			}
		}
		else
		{
			sys_usbd.error("libusb_get_active_config_descriptor failed %d", r);
			break;
		}

		libusb_free_config_descriptor(config);
		break;
	}

	libusb_free_device_list(devs, 1);

	// todo: return code
	return libusb_dev_handle == nullptr ? -1 : 0;
}

s64 sys_usbd_transfer_data(u32 handle, u32 pipe, vm::ptr<void> in_buf, u32 in_len, vm::ptr<void> out_buf, u32 out_len)
{
	sys_usbd.todo("sys_usbd_transfer_data(handle=0x%x, pipe=0x%x, in_buf=*0x%x, in_len=0x%x, out_buf=*0x%x, out_len=0x%x)", handle, pipe, in_buf, in_len, out_buf, out_len);

	// todo: turn this into libusb async calls
	// also save pipe 'type' to use correct libusb call instead of this
	if (pipe >= pipe_mapping.size())
		return -1; // todo: error code

	if (out_buf != vm::null)
	{
		auto device_request = vm::static_ptr_cast<SysUsbdDeviceRequest>(out_buf);

		sys_usbd.error(
		    "dr: bmrt: 0x%x br: 0x%x, wv: 0x%x, wi: 0x%x, wl: 0x%x", device_request->bmRequestType, device_request->bRequest, device_request->wValue, device_request->wIndex, device_request->wLength);

		if (device_request->bmRequestType != 0x0 || device_request->bRequest != LIBUSB_REQUEST_SET_CONFIGURATION)
			fmt::throw_exception("unhandled device request type");

		// check current config with requested
		// note/todo: according to libusb, set_configuration should use the libusb call instead of making the packet manually
		// in order to tell the OS also in case it cares
		struct libusb_config_descriptor* config;
		int r = libusb_get_active_config_descriptor(libusb_get_device(libusb_dev_handle), &config);
		if (r == LIBUSB_ERROR_NOT_FOUND)
		{
			// todo: send set_configuration packet and claim interfaces after it is successful
			fmt::throw_exception("todo: sys_usbd_transfer_data, set_configuration");
		}
		else if (r != LIBUSB_SUCCESS)
		{
			// todo: return value
			fmt::throw_exception("libusb_get_active_config_descriptor failed %d", r);
		}
		else if (config->bConfigurationValue == device_request->wValue)
		{
			// do nothing, we could technically send this to possibly cause a softreset of usb device
			// but for now we'll just ignore it
			probe_data.arg1 = 3;
			probe_data.arg2 = devid;
			probe_data.arg3 = 0;
			trigger_probe   = true;
		}
		else
		{
			// in this case we will have to release our interface claims with libusb
			// care must be taken on linux(usbdk windows probly fine) to make sure kernel driver doesnt snag it after release
			// after, set config and reclaim interfaces for new configuration
			fmt::throw_exception("game wants different active configuration");
		}
		libusb_free_config_descriptor(config);
	}
	else
	{
		u8 end_addr = ::narrow<u8>(pipe_mapping[pipe]);
		std::unique_ptr<u8[]> data = std::make_unique<u8[]>(in_len);
		int trans;
		int r                      = libusb_interrupt_transfer(libusb_dev_handle, end_addr, data.get(), in_len, &trans, 0);
		if (r < 0)
		{
			sys_usbd.error("libusb_interrupt_transfer error %d", r);
			return -1;
		}

		if (in_len != trans)
			fmt::throw_exception("transfered != in_len");
		memcpy(in_buf.get_ptr(), data.get(), in_len);

		probe_data.arg1 = 3;
		probe_data.arg2 = devid;
		probe_data.arg3 = 0;
		len             = trans;
		trigger_probe   = true;
	}

	// this returns back the odd 'arg2' formatted deviceid from receive event
	return devid;
}

s32 sys_usbd_isochronous_transfer_data()
{
	sys_usbd.todo("sys_usbd_isochronous_transfer_data()");
	return CELL_OK;
}

s32 sys_usbd_get_transfer_status(u32 handle, u32 a2, u32 a3, vm::ptr<u32> result, vm::ptr<u32> count)
{
	sys_usbd.todo("sys_usbd_get_transfer_status(handle=0x%x, a2=0x%x, a3=0x%x, result=*0x%x, count=*0x%x)", handle, a2, a3, result, count);

	if (result)
		*result = 0;
	if (count)
		*count = len;
	len = 0;
	
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
