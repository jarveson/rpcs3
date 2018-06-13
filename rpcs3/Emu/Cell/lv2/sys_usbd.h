#pragma once

#include "libusb.h"

enum : u8
{
	SYS_USBD_DEVICE_STATUS_FREE = 0,
	SYS_USBD_DEVICE_STATUS_UNK, // Unknown what this status state represents
	SYS_USBD_DEVICE_STATUS_CLAIMED,
};

struct SysUsbdDeviceInfo
{
	// Low byte seems to be *actual* device Id that counts up
	// High byte may be host controller? it seems only system devices have it set
	be_t<u16> deviceID;
	u8 status;
	u8 unk4; // looks to be unused by cellusbd
} ;

/* 0x01 device descriptor */
struct deviceDescriptor
{
	u8 bLength;              /* descriptor size in bytes */
	u8 bDescriptorType;      /* constant USB_DESCRIPTOR_TYPE_DEVICE */
	u16 bcdUSB;              /* USB spec release compliance number */
	u8 bDeviceClass;         /* class code */
	u8 bDeviceSubClass;      /* subclass code */
	u8 bDeviceProtocol;      /* protocol code */
	u8 bMaxPacketSize0;      /* max packet size for endpoint 0 */
	u16 idVendor;            /* USB-IF Vendor ID (VID) */
	u16 idProduct;           /* Product ID (PID) */
	u16 bcdDevice;           /* device release number */
	u8 iManufacturer;        /* manufacturer string descriptor index */
	u8 iProduct;             /* product string desccriptor index */
	u8 iSerialNumber;        /* serial number string descriptor index */
	u8 bNumConfigurations;   /* number of configurations */
};

struct usbDevice
{
	SysUsbdDeviceInfo basicDevice;
	deviceDescriptor descriptor;
};

struct SysUsbdDeviceRequest
{
	u8 bmRequestType;
	u8 bRequest;
	be_t<u16> wValue;
	be_t<u16> wIndex;
	be_t<u16> wLength;
};

s32 sys_usbd_initialize(vm::ptr<u32> handle);
s32 sys_usbd_finalize();
s32 sys_usbd_get_device_list(u32 handle, vm::ptr<SysUsbdDeviceInfo> device_list, u32 max_devices);
s32 sys_usbd_get_descriptor_size(u32 handle, u32 device_handle);
s32 sys_usbd_get_descriptor(u32 handle, u32 device_handle, vm::ptr<void> descriptor, s64 descSize);
s32 sys_usbd_register_ldd(u32 handle, vm::ptr<char> name, u32 namelen);
s32 sys_usbd_unregister_ldd();
s32 sys_usbd_open_pipe(u32 handle, u32 device_handle, u32 unk1, u32 unk2, u32 unk3, u32 endpoint_address, u32 type);
s32 sys_usbd_open_default_pipe(u32 handle, u32 device_handle);
s32 sys_usbd_close_pipe();
s32 sys_usbd_receive_event(ppu_thread& ppu, u32 handle, vm::ptr<u64> arg1, vm::ptr<u64> arg2, vm::ptr<u64> arg3);
s32 sys_usbd_detect_event();
s32 sys_usbd_attach(u32 handle, u32 lddhandle, u32 device_id_high, u32 device_id_low);
s64 sys_usbd_transfer_data(u32 handle, u32 pipe, vm::ptr<void> in_buf, u32 in_len, vm::ptr<void> out_buf, u32 out_len);
s32 sys_usbd_isochronous_transfer_data();
s32 sys_usbd_get_transfer_status(u32 handle, u32 pipe, u32 a3, u32 a4, u32 a5);
s32 sys_usbd_get_isochronous_transfer_status();
s32 sys_usbd_get_device_location();
s32 sys_usbd_send_event();
s32 sys_usbd_event_port_send();
s32 sys_usbd_allocate_memory();
s32 sys_usbd_free_memory();
s32 sys_usbd_get_device_speed();
s32 sys_usbd_register_extra_ldd(u32 handle, vm::ptr<char> name, u32 strLen, u16 vid, u16 pid_min, u16 pid_max);
