// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "hid/hid.h"

#include "hidpi.h"

struct hid {
	uint32_t id;
	MTY_Hash *devices;
	MTY_Hash *devices_rev;
	HID_CONNECT connect;
	HID_DISCONNECT disconnect;
	HID_REPORT report;
	void *opaque;
};

struct hid_dev {
	RID_DEVICE_INFO di;
	PHIDP_PREPARSED_DATA ppd;
	HIDP_CAPS caps;
	HIDP_BUTTON_CAPS *bcaps;
	HIDP_VALUE_CAPS *vcaps;
	wchar_t *name;
	bool is_xinput;
	void *state;
	uint32_t id;
};

static void hid_device_destroy(void *opaque)
{
	if (!opaque)
		return;

	struct hid_dev *ctx = opaque;

	MTY_Free(ctx->bcaps);
	MTY_Free(ctx->vcaps);
	MTY_Free(ctx->ppd);
	MTY_Free(ctx->name);
	MTY_Free(ctx->state);
	MTY_Free(ctx);
}

static struct hid_dev *hid_device_create(HANDLE device)
{
	struct hid_dev *ctx = MTY_Alloc(1, sizeof(struct hid_dev));

	bool r = true;

	UINT size = 0;
	UINT e = GetRawInputDeviceInfo(device, RIDI_PREPARSEDDATA, NULL, &size);
	if (e != 0) {
		r = false;
		MTY_Log("'GetRawInputDeviceInfo' failed with error 0x%X", GetLastError());
		goto except;
	}

	ctx->ppd = MTY_Alloc(size, 1);
	e = GetRawInputDeviceInfo(device, RIDI_PREPARSEDDATA, ctx->ppd, &size);
	if (e == 0xFFFFFFFF || e == 0) {
		r = false;
		MTY_Log("'GetRawInputDeviceInfo' failed with error 0x%X", GetLastError());
		goto except;
	}

	NTSTATUS ne = HidP_GetCaps(ctx->ppd, &ctx->caps);
	if (ne != HIDP_STATUS_SUCCESS) {
		r = false;
		MTY_Log("'HidP_GetCaps' failed with NTSTATUS 0x%X", ne);
		goto except;
	}

	ctx->bcaps = MTY_Alloc(ctx->caps.NumberInputButtonCaps, sizeof(HIDP_BUTTON_CAPS));
	ne = HidP_GetButtonCaps(HidP_Input, ctx->bcaps, &ctx->caps.NumberInputButtonCaps, ctx->ppd);
	if (ne != HIDP_STATUS_SUCCESS) {
		r = false;
		MTY_Log("'HidP_GetButtonCaps' failed with NTSTATUS 0x%X", ne);
		goto except;
	}

	ctx->vcaps = MTY_Alloc(ctx->caps.NumberInputValueCaps, sizeof(HIDP_VALUE_CAPS));
	ne = HidP_GetValueCaps(HidP_Input, ctx->vcaps, &ctx->caps.NumberInputValueCaps, ctx->ppd);
	if (ne != HIDP_STATUS_SUCCESS) {
		r = false;
		MTY_Log("'HidP_GetValueCaps' failed with NTSTATUS 0x%X", ne);
		goto except;
	}

	ctx->di.cbSize = size = sizeof(RID_DEVICE_INFO);
	e = GetRawInputDeviceInfo(device, RIDI_DEVICEINFO, &ctx->di, &size);
	if (e == 0xFFFFFFFF || e == 0) {
		r = false;
		MTY_Log("'GetRawInputDeviceInfo' failed with error 0x%X", GetLastError());
		goto except;
	}

	size = 0;
	e = GetRawInputDeviceInfo(device, RIDI_DEVICENAME, NULL, &size);
	if (e != 0) {
		r = false;
		MTY_Log("'GetRawInputDeviceInfo' failed with error 0x%X", GetLastError());
		goto except;
	}

	ctx->name = MTY_Alloc(size, sizeof(wchar_t));
	e = GetRawInputDeviceInfo(device, RIDI_DEVICENAME, ctx->name, &size);
	if (e == 0xFFFFFFFF) {
		r = false;
		MTY_Log("'GetRawInputDeviceInfo' failed with error 0x%X", GetLastError());
		goto except;
	}

	ctx->is_xinput = wcsstr(ctx->name, L"IG_") ? true : false;
	ctx->state = MTY_Alloc(HID_STATE_MAX, 1);

	except:

	if (!r) {
		hid_device_destroy(ctx);
		ctx = NULL;
	}

	return ctx;
}

struct hid *mty_hid_create(HID_CONNECT connect, HID_DISCONNECT disconnect, HID_REPORT report, void *opaque)
{
	struct hid *ctx = MTY_Alloc(1, sizeof(struct hid));
	ctx->devices = MTY_HashCreate(0);
	ctx->devices_rev = MTY_HashCreate(0);
	ctx->connect = connect;
	ctx->disconnect = disconnect;
	ctx->report = report;
	ctx->opaque = opaque;
	ctx->id = 32;

	return ctx;
}

struct hid_dev *mty_hid_get_device_by_id(struct hid *ctx, uint32_t id)
{
	return MTY_HashGetInt(ctx->devices_rev, id);
}

void mty_hid_destroy(struct hid **hid)
{
	if (!hid || !*hid)
		return;

	struct hid *ctx = *hid;

	MTY_HashDestroy(&ctx->devices, hid_device_destroy);
	MTY_HashDestroy(&ctx->devices_rev, NULL);

	MTY_Free(ctx);
	*hid = NULL;
}

void mty_hid_device_write(struct hid_dev *ctx, const void *buf, size_t size)
{
	OVERLAPPED ov = {0};
	ov.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	HANDLE device = CreateFile(ctx->name, GENERIC_WRITE | GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
		FILE_FLAG_OVERLAPPED, NULL);

	if (!device) {
		MTY_Log("'CreateFile' failed with error 0x%X", GetLastError());
		goto except;
	}

	if (!WriteFile(device, buf, (DWORD) size, NULL, &ov)) {
		DWORD e = GetLastError();
		if (e != ERROR_IO_PENDING) {
			MTY_Log("'WriteFile' failed with error 0x%X", e);
			goto except;
		}
	}

	if (WaitForSingleObject(ov.hEvent, 1000) == WAIT_OBJECT_0) {
		DWORD written = 0;
		if (!GetOverlappedResult(device, &ov, &written, FALSE)) {
			MTY_Log("'GetOverlappedResult' failed with error 0x%X", GetLastError());
			goto except;
		}
	}

	except:

	if (ov.hEvent)
		CloseHandle(ov.hEvent);

	if (device)
		CloseHandle(device);
}

bool mty_hid_device_feature(struct hid_dev *ctx, void *buf, size_t size, size_t *size_out)
{
	bool r = true;

	*size_out = 0;
	OVERLAPPED ov = {0};

	HANDLE device = CreateFile(ctx->name, GENERIC_WRITE | GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
		FILE_FLAG_OVERLAPPED, NULL);

	if (!device) {
		r = false;
		MTY_Log("'CreateFile' failed with error 0x%X", GetLastError());
		goto except;
	}

	DWORD out = 0;
	if (!DeviceIoControl(device, IOCTL_HID_GET_FEATURE, buf, (DWORD) size, buf, (DWORD) size, &out, &ov)) {
		DWORD e = GetLastError();
		if (e != ERROR_IO_PENDING) {
			r = false;
			MTY_Log("'DeviceIoControl' failed with error 0x%X", e);
			goto except;
		}
	}

	if (!GetOverlappedResult(device, &ov, &out, TRUE)) {
		r = false;
		MTY_Log("'GetOverlappedResult' failed with error 0x%X", GetLastError());
		goto except;
	}

	*size_out = out + 1;

	except:

	if (device)
		CloseHandle(device);

	return r;
}

void *mty_hid_device_get_state(struct hid_dev *ctx)
{
	return ctx->state;
}

uint16_t mty_hid_device_get_vid(struct hid_dev *ctx)
{
	return (uint16_t) ctx->di.hid.dwVendorId;
}

uint16_t mty_hid_device_get_pid(struct hid_dev *ctx)
{
	return (uint16_t) ctx->di.hid.dwProductId;
}

uint32_t mty_hid_device_get_id(struct hid_dev *ctx)
{
	return ctx->id;
}

uint32_t mty_hid_device_get_input_report_size(struct hid_dev *ctx)
{
	return ctx->caps.InputReportByteLength;
}

void mty_hid_default_state(struct hid_dev *ctx, const void *buf, size_t size, MTY_ControllerEvent *c)
{
	// Note: Some of these functions use PCHAR for a const buffer, thus the cast

	// Buttons
	for (uint32_t x = 0; x < ctx->caps.NumberInputButtonCaps; x++) {
		const HIDP_BUTTON_CAPS *bcap = &ctx->bcaps[x];

		// Should we consider usages other than 0x09 (Button)?
		if (bcap->UsagePage == 0x09) {
			c->numButtons += (uint8_t) (bcap->Range.UsageMax - bcap->Range.UsageMin + 1);

			ULONG n = MTY_CBUTTON_MAX;
			USAGE usages[MTY_CBUTTON_MAX];
			NTSTATUS e = HidP_GetUsages(HidP_Input, bcap->UsagePage, 0, usages, &n,
				ctx->ppd, (PCHAR) buf, (ULONG) size);

			if (e != HIDP_STATUS_SUCCESS && e != HIDP_STATUS_INCOMPATIBLE_REPORT_ID) {
				MTY_Log("'HidP_GetUsages' failed with NTSTATUS 0x%X", e);
				return;
			}

			if (e == HIDP_STATUS_SUCCESS) {
				for (ULONG y = 0; y < n; y++) {
					uint32_t i = usages[y] - bcap->Range.UsageMin;
					if (i < MTY_CBUTTON_MAX)
						c->buttons[i] = true;
				}
			}
		}
	}

	// Values
	for (uint32_t x = 0; x < ctx->caps.NumberInputValueCaps && c->numAxes < MTY_CAXIS_MAX; x++) {
		const HIDP_VALUE_CAPS *vcap = &ctx->vcaps[x];

		ULONG value = 0;
		NTSTATUS e = HidP_GetUsageValue(HidP_Input, vcap->UsagePage, 0, vcap->Range.UsageMin,
			&value, ctx->ppd, (PCHAR) buf, (ULONG) size);

		if (e != HIDP_STATUS_SUCCESS && e != HIDP_STATUS_INCOMPATIBLE_REPORT_ID) {
			MTY_Log("'HidP_GetUsageValue' failed with NTSTATUS 0x%X", e);
			return;
		}

		if (e == HIDP_STATUS_SUCCESS) {
			MTY_Axis *v = &c->axes[c->numAxes++];

			v->usage = vcap->Range.UsageMin;
			v->value = (int16_t) value;
			v->min = (int16_t) vcap->LogicalMin;
			v->max = (int16_t) vcap->LogicalMax;
		}
	}

	c->type = MTY_CTYPE_DEFAULT;
	c->vid = (uint16_t) ctx->di.hid.dwVendorId;
	c->pid = (uint16_t) ctx->di.hid.dwProductId;
	c->id = ctx->id;
}

void mty_hid_default_rumble(struct hid *ctx, uint32_t id, uint16_t low, uint16_t high)
{
}


// Win32 RAWINPUT interop

void mty_hid_win32_report(struct hid *ctx, intptr_t device, const void *buf, size_t size)
{
	struct hid_dev *dev = MTY_HashGetInt(ctx->devices, device);

	if (dev && !dev->is_xinput)
		ctx->report(dev, buf, size, ctx->opaque);
}

void mty_hid_win32_device_change(struct hid *ctx, intptr_t wparam, intptr_t lparam)
{
	if (wparam == GIDC_ARRIVAL) {
		struct hid_dev *dev = hid_device_create((HANDLE) lparam);
		if (dev) {
			dev->id = ctx->id++;
			mty_hid_destroy(MTY_HashSetInt(ctx->devices, lparam, dev));
			MTY_HashSetInt(ctx->devices_rev, dev->id, dev);

			if (!dev->is_xinput)
				ctx->connect(dev, ctx->opaque);
		}

	} else if (wparam == GIDC_REMOVAL) {
		struct hid_dev *dev = MTY_HashPopInt(ctx->devices, lparam);
		if (dev) {
			if (!dev->is_xinput)
				ctx->disconnect(dev, ctx->opaque);

			MTY_HashPopInt(ctx->devices_rev, dev->id);
			hid_device_destroy(dev);
		}
	}
}

void mty_hid_win32_listen(void *hwnd)
{
	RAWINPUTDEVICE rid[3] = {
		{0x01, 0x04, RIDEV_DEVNOTIFY | RIDEV_INPUTSINK, (HWND) hwnd},
		{0x01, 0x05, RIDEV_DEVNOTIFY | RIDEV_INPUTSINK, (HWND) hwnd},
		{0x01, 0x08, RIDEV_DEVNOTIFY | RIDEV_INPUTSINK, (HWND) hwnd},
	};

	if (!RegisterRawInputDevices(rid, 3, sizeof(RAWINPUTDEVICE)))
		MTY_Log("'RegisterRawInputDevices' failed with error 0x%X", GetLastError());
}
