// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "hid/hid.h"

#include <IOKit/hid/IOHIDManager.h>
#include <IOKit/hid/IOHIDKeys.h>

struct hid {
	uint32_t id;
	MTY_Hash *devices;
	MTY_Hash *devices_rev;
	IOHIDManagerRef mgr;
	HID_CONNECT connect;
	HID_DISCONNECT disconnect;
	HID_REPORT report;
	void *opaque;
};

struct hid_dev {
	IOHIDDeviceRef device;
	void *state;
	uint32_t id;
	uint32_t input_size;
	uint16_t vid;
	uint16_t pid;
};

static int32_t hid_device_get_prop_int(IOHIDDeviceRef device, CFStringRef key)
{
	CFNumberRef num = IOHIDDeviceGetProperty(device, key);
	if (!num)
		return 0;

	int32_t i = 0;
	if (!CFNumberGetValue(num, kCFNumberSInt32Type, &i))
		return 0;

	return i;
}

static struct hid_dev *hid_device_create(void *native_device)
{
	struct hid_dev *ctx = MTY_Alloc(1, sizeof(struct hid_dev));
	ctx->device = (IOHIDDeviceRef) native_device;
	ctx->vid = hid_device_get_prop_int(ctx->device, CFSTR(kIOHIDVendorIDKey));
	ctx->pid = hid_device_get_prop_int(ctx->device, CFSTR(kIOHIDProductIDKey));
	ctx->input_size = hid_device_get_prop_int(ctx->device, CFSTR(kIOHIDMaxInputReportSizeKey));
	ctx->state = MTY_Alloc(HID_STATE_MAX, 1);

	return ctx;
}

static void hid_device_destroy(void *hdevice)
{
	if (!hdevice)
		return;

	struct hid_dev *ctx = hdevice;

	MTY_Free(ctx->state);
	MTY_Free(ctx);
}

static void hid_report(void *context, IOReturn result, void *sender, IOHIDReportType type,
	uint32_t reportID, uint8_t *report, CFIndex reportLength)
{
	struct hid *ctx = context;

	struct hid_dev *dev = MTY_HashGetInt(ctx->devices, (intptr_t) sender);
	if (!dev) {
		dev = hid_device_create(sender);
		dev->id = ++ctx->id;
		MTY_HashSetInt(ctx->devices, (intptr_t) sender, dev);
		MTY_HashSetInt(ctx->devices_rev, dev->id, dev);

		ctx->connect(dev, ctx->opaque);
	}

	ctx->report(dev, report, reportLength, ctx->opaque);
}

static void hid_disconnect(void *context, IOReturn result, void *sender, IOHIDDeviceRef device)
{
	struct hid *ctx = context;

	struct hid_dev *dev = MTY_HashPopInt(ctx->devices, (intptr_t) device);
	if (dev) {
		ctx->disconnect(dev, ctx->opaque);

		MTY_HashPopInt(ctx->devices_rev, dev->id);
		hid_device_destroy(dev);
	}
}

static void hid_dict_set_int(CFMutableDictionaryRef dict, CFStringRef key, int32_t val)
{
	CFNumberRef v = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &val);
	CFDictionarySetValue(dict, key, v);
	CFRelease(v);
}

static CFMutableDictionaryRef hid_match_dict(int32_t usage_page, int32_t usage)
{
	CFMutableDictionaryRef dict = CFDictionaryCreateMutable(kCFAllocatorDefault, kIOHIDOptionsTypeNone,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	hid_dict_set_int(dict, CFSTR(kIOHIDDeviceUsagePageKey), usage_page);
	hid_dict_set_int(dict, CFSTR(kIOHIDDeviceUsageKey), usage);

	return dict;
}

struct hid *mty_hid_create(HID_CONNECT connect, HID_DISCONNECT disconnect, HID_REPORT report, void *opaque)
{
	bool r = true;

	struct hid *ctx = MTY_Alloc(1, sizeof(struct hid));
	ctx->connect = connect;
	ctx->disconnect = disconnect;
	ctx->report = report;
	ctx->opaque = opaque;
	ctx->devices = MTY_HashCreate(0);
	ctx->devices_rev = MTY_HashCreate(0);

	ctx->mgr = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
	if (!ctx->mgr) {
		r = false;
		MTY_Log("'IOHIDManagerCreate' failed");
		goto except;
	}

	CFMutableDictionaryRef d0 = hid_match_dict(kHIDPage_GenericDesktop, kHIDUsage_GD_Joystick);
	CFMutableDictionaryRef d1 = hid_match_dict(kHIDPage_GenericDesktop, kHIDUsage_GD_GamePad);
	CFMutableDictionaryRef d2 = hid_match_dict(kHIDPage_GenericDesktop, kHIDUsage_GD_MultiAxisController);
	CFMutableDictionaryRef dict_list[] = {d0, d1, d2};

	CFArrayRef matches = CFArrayCreate(kCFAllocatorDefault, (const void **) dict_list, 3, NULL);
	IOHIDManagerSetDeviceMatchingMultiple(ctx->mgr, matches);
	CFRelease(matches);

	IOHIDManagerScheduleWithRunLoop(ctx->mgr, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);

	IOReturn e = IOHIDManagerOpen(ctx->mgr, kIOHIDOptionsTypeNone);
	if (e != kIOReturnSuccess) {
		r = false;
		MTY_Log("'IOHIDManagerOpen' failed with error 0x%X", e);
		goto except;
	}

	IOHIDManagerRegisterInputReportCallback(ctx->mgr, hid_report, ctx);
	IOHIDManagerRegisterDeviceRemovalCallback(ctx->mgr, hid_disconnect, ctx);

	except:

	if (!r)
		mty_hid_destroy(&ctx);

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

	if (ctx->mgr) {
		IOReturn e = IOHIDManagerClose(ctx->mgr, kIOHIDOptionsTypeNone);
		if (e != kIOReturnSuccess)
			MTY_Log("'IOHIDManagerClose' failed with error 0x%X", e);

		CFRelease(ctx->mgr);
	}

	MTY_HashDestroy(&ctx->devices, hid_device_destroy);
	MTY_HashDestroy(&ctx->devices_rev, NULL);

	MTY_Free(ctx);
	*hid = NULL;
}

void mty_hid_device_write(struct hid_dev *ctx, const void *buf, size_t size)
{
	const uint8_t *buf8 = buf;

	if (buf8[0] == 0) {
		buf += 1;
		size -= 1;
	}

	IOReturn e = IOHIDDeviceSetReport(ctx->device, kIOHIDReportTypeOutput, buf8[0], buf, size);
	if (e != kIOReturnSuccess)
		MTY_Log("'IOHIDDeviceSetReport' failed with error 0x%X", e);
}

bool mty_hid_device_feature(struct hid_dev *ctx, void *buf, size_t size, size_t *size_out)
{
	const uint8_t *buf8 = buf;

	if (buf8[0] == 0) {
		buf += 1;
		size -= 1;
	}

	CFIndex out = size;
	IOReturn e = IOHIDDeviceGetReport(ctx->device, kIOHIDReportTypeFeature, buf8[0], buf, &out);
	if (e == kIOReturnSuccess) {
		*size_out = out;

		if (buf8[0] == 0)
			(*size_out)++;

		return true;

	} else {
		MTY_Log("'IOHIDDeviceGetReport' failed with error 0x%X", e);
	}

	return false;
}

void mty_hid_default_state(struct hid_dev *ctx, const void *buf, size_t size, MTY_ControllerEvent *c)
{
	CFArrayRef elements = IOHIDDeviceCopyMatchingElements(ctx->device, NULL, kIOHIDOptionsTypeNone);

	for (CFIndex x = 0; x < CFArrayGetCount(elements); x++) {
		IOHIDElementRef el = (IOHIDElementRef) CFArrayGetValueAtIndex(elements, x);

		IOHIDElementType type = IOHIDElementGetType(el);
		uint16_t usage = IOHIDElementGetUsage(el);
		uint16_t usage_page = IOHIDElementGetUsagePage(el);

		bool is_button = type == kIOHIDElementTypeInput_Button && usage_page == 0x09;
		bool is_value = (type == kIOHIDElementTypeInput_Misc || type == kIOHIDElementTypeInput_Axis) &&
			usage >= 0x30 && usage <= 0x39;

		if (is_button || is_value) {
			IOHIDValueRef v = NULL;
			IOReturn e = IOHIDDeviceGetValue(ctx->device, el, &v);
			if (e == kIOReturnSuccess) {
				int32_t vi = IOHIDValueGetIntegerValue(v);

				if (is_button) {
					if (c->numButtons < MTY_CBUTTON_MAX && usage <= MTY_CBUTTON_MAX) {
						c->buttons[usage - 1] = vi;
						c->numButtons++;
					}

				} else {
					if (c->numAxes < MTY_CAXIS_MAX) {
						MTY_Axis *axis = &c->axes[c->numAxes++];
						axis->max = IOHIDElementGetLogicalMax(el);
						axis->min = IOHIDElementGetLogicalMin(el);
						axis->usage = usage;
						axis->value = vi;
					}
				}

			} else {
				MTY_Log("'IOHIDDeviceGetValue' failed with error 0x%X", e);
			}
		}
	}

	CFRelease(elements);

	c->type = MTY_CTYPE_DEFAULT;
	c->vid = ctx->vid;
	c->pid = ctx->pid;
	c->id = ctx->id;
}

void mty_hid_default_rumble(struct hid *ctx, uint32_t id, uint16_t low, uint16_t high)
{
}

void *mty_hid_device_get_state(struct hid_dev *ctx)
{
	return ctx->state;
}

const char *mty_hid_device_get_name(struct hid_dev *ctx)
{
	return NULL;
}

uint16_t mty_hid_device_get_vid(struct hid_dev *ctx)
{
	return ctx->vid;
}

uint16_t mty_hid_device_get_pid(struct hid_dev *ctx)
{
	return ctx->pid;
}

uint32_t mty_hid_device_get_id(struct hid_dev *ctx)
{
	return ctx->id;
}

uint32_t mty_hid_device_get_input_report_size(struct hid_dev *ctx)
{
	return ctx->input_size;
}
