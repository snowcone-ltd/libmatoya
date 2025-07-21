// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "evdev.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <poll.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <linux/input.h>

#include "dl/libudev.h"

// https://github.com/torvalds/linux/blob/master/include/uapi/linux/input.h
// https://github.com/torvalds/linux/blob/master/include/uapi/linux/input-event-codes.h

#define EVDEV_FD_MAX 33
#define EVDEV_EVENTS_MAX 256 /* Max number of input events per read */

struct evdev {
	bool init_scan;
	struct udev *udev;
	struct udev_monitor *udev_monitor;
	MTY_Hash *devices;
	MTY_Hash *devices_rev;
	EVDEV_CONNECT connect;
	EVDEV_DISCONNECT disconnect;
	struct pollfd fds[EVDEV_FD_MAX];
	void *opaque;
};

struct evdev_dev {
	MTY_ControllerEvent state;
	struct ff_effect ff;
	bool gamepad; // PS4, XInput
	bool rumble; // Can rumble
	uint8_t slot;
	uint32_t id;

	struct {
		uint8_t slot;
		int32_t min;
		int32_t max;
	} ainfo[ABS_CNT];
};

static void evdev_device_destroy(void *edevice)
{
	if (!edevice)
		return;

	struct evdev_dev *ctx = edevice;

	MTY_Free(ctx);
}

static uint8_t evdev_find_slot(struct evdev *ctx)
{
	for (uint8_t x = 1; x < EVDEV_FD_MAX; x++)
		if (ctx->fds[x].fd == -1)
			return x;

	return 0;
}

static void evdev_device_add(struct evdev *ctx, const char *devnode, const char *syspath)
{
	struct evdev_dev *edev = MTY_HashGet(ctx->devices, devnode);
	if (edev)
		return;

	uint8_t slot = evdev_find_slot(ctx);
	if (slot == 0)
		return;

	int32_t fd = open(devnode, O_RDWR | O_NONBLOCK);

	if (fd >= 0) {
		uint64_t evs = 0;
		ioctl(fd, EVIOCGBIT(0, sizeof(uint64_t)), &evs);

		// There is no great way to tell what kind of device this is. Filter by
		// devices that have EV_KEY (keys and buttons) and EV_ABS (absolute axis)
		if ((evs & (1 << EV_KEY)) && (evs & (1 << EV_ABS))) {
			edev = MTY_Alloc(1, sizeof(struct evdev_dev));
			edev->id = fd;
			edev->slot = slot;

			edev->state.type = MTY_CTYPE_DEFAULT;
			edev->state.numAxes = 0; // Discovered
			edev->state.numButtons = 19; // There's no good way to know how many buttons the device has
			edev->state.id = edev->id;

			// VID/PID
			struct input_id ids = {0};
			ioctl(fd, EVIOCGID, &ids);
			edev->state.vid = ids.vendor;
			edev->state.pid = ids.product;

			// Set up rumble
			edev->ff.type = FF_RUMBLE;
			edev->ff.replay.length = 1000;
			edev->ff.id = -1;

			uint64_t features[2] = {0};
			if (ioctl(fd, EVIOCGBIT(EV_FF, 2 * sizeof(uint64_t)), features) != -1)
				edev->rumble = features[1] & (1 << (FF_RUMBLE - 64));

			// Discover axis
			for (uint8_t x = 0x00; x < ABS_CNT; x++) {
				if (x != ABS_HAT0X && x != ABS_HAT0Y) {
					struct input_absinfo info = {0};
					ioctl(fd, EVIOCGABS(x), &info);

					if (info.minimum != 0 || info.maximum != 0) {
						edev->ainfo[x].slot = edev->state.numAxes++;
						edev->ainfo[x].min = info.minimum;
						edev->ainfo[x].max = info.maximum;
					}
				}
			}

			ctx->fds[slot].fd = fd;
			MTY_HashSet(ctx->devices, devnode, edev);
			MTY_HashSetInt(ctx->devices_rev, edev->id, edev);

			ctx->connect(edev, ctx->opaque);

		} else {
			close(fd);
		}

	} else {
		// Many event devnodes are not readable by non-root (like mouse and keyboard)
		// Joysticks usually are
		if (errno != EACCES)
			MTY_Log("'open' failed with errno %d", errno);
	}
}

static void evdev_device_remove(struct evdev *ctx, const char *devnode)
{
	struct evdev_dev *edev = MTY_HashPop(ctx->devices, devnode);
	if (!edev)
		return;

	ctx->disconnect(edev, ctx->opaque);
	int32_t *fd = &ctx->fds[edev->slot].fd;

	if (*fd >= 0) {
		close(*fd);
		*fd = -1;
	}

	MTY_HashPopInt(ctx->devices_rev, edev->id);
	evdev_device_destroy(edev);
}

static void evdev_new_device(struct evdev *ctx)
{
	struct udev_device *dev = udev_monitor_receive_device(ctx->udev_monitor);
	if (!dev)
		return;

	const char *action = udev_device_get_action(dev);
	const char *devnode = udev_device_get_devnode(dev);
	const char *syspath = udev_device_get_syspath(dev);
	if (!action || !devnode)
		goto except;

	if (!strstr(devnode, "/event"))
		goto except;

	if (!strcmp(action, "add")) {
		evdev_device_add(ctx, devnode, syspath);

	} else if (!strcmp(action, "remove")) {
		evdev_device_remove(ctx, devnode);
	}

	except:

	udev_device_unref(dev);
}

static MTY_CButton evdev_button(uint16_t type)
{
	switch (type) {
		// Gamepad
		case BTN_A: return MTY_CBUTTON_A; // South
		case BTN_B: return MTY_CBUTTON_B; // East
		case BTN_X: return MTY_CBUTTON_Y; // North
		case BTN_Y: return MTY_CBUTTON_X; // West
		case BTN_TL: return MTY_CBUTTON_LEFT_SHOULDER;
		case BTN_TR: return MTY_CBUTTON_RIGHT_SHOULDER;
		case BTN_TL2: return MTY_CBUTTON_LEFT_TRIGGER;
		case BTN_TR2: return MTY_CBUTTON_RIGHT_TRIGGER;
		case BTN_SELECT: return MTY_CBUTTON_BACK;
		case BTN_START: return MTY_CBUTTON_START;
		case BTN_MODE: return MTY_CBUTTON_GUIDE;
		case BTN_THUMBL: return MTY_CBUTTON_LEFT_THUMB;
		case BTN_THUMBR: return MTY_CBUTTON_RIGHT_THUMB;

		// Gamepad Unknown
		case BTN_C: return MTY_CBUTTON_TOUCHPAD;
		case BTN_Z: return MTY_CBUTTON_CAPTURE;

		// Joystick
		case BTN_TRIGGER: return MTY_CBUTTON_X;
		case BTN_THUMB: return MTY_CBUTTON_A;
		case BTN_THUMB2: return MTY_CBUTTON_B;
		case BTN_TOP: return MTY_CBUTTON_Y;
		case BTN_TOP2: return MTY_CBUTTON_LEFT_SHOULDER;
		case BTN_PINKIE: return MTY_CBUTTON_RIGHT_SHOULDER;
		case BTN_BASE: return MTY_CBUTTON_LEFT_TRIGGER;
		case BTN_BASE2: return MTY_CBUTTON_RIGHT_TRIGGER;
		case BTN_BASE3: return MTY_CBUTTON_BACK;
		case BTN_BASE4: return MTY_CBUTTON_START;
		case BTN_BASE5: return MTY_CBUTTON_LEFT_THUMB;
		case BTN_BASE6: return MTY_CBUTTON_RIGHT_THUMB;

		// Joystick Unknown
		case BTN_DEAD: return MTY_CBUTTON_GUIDE;
	}

	return -1;
}

static uint16_t evdev_gamepad_usage(struct evdev_dev *ctx, uint16_t type, const struct input_event *event)
{
	switch (type) {
		case ABS_X: return 0x30;
		case ABS_Y: return 0x31;
		case ABS_Z: return 0x33;
		case ABS_RX: return 0x32;
		case ABS_RY: return 0x35;
		case ABS_RZ: return 0x34;
	}

	return 0;
}

static uint16_t evdev_joystick_usage(struct evdev_dev *ctx, uint16_t type, const struct input_event *event)
{
	switch (type) {
		case ABS_X: return 0x30;
		case ABS_Y: return 0x31;
		case ABS_Z: return 0x32;
		case ABS_RZ: return 0x35;
	}

	return 0;
}

static void evdev_joystick_event(struct evdev *ctx, int32_t fd, EVDEV_REPORT report)
{
	struct evdev_dev *edev = MTY_HashGetInt(ctx->devices_rev, fd);
	if (!edev)
		return;

	MTY_ControllerEvent *c = &edev->state;
	struct input_event event_list[EVDEV_EVENTS_MAX] = {0};

	ssize_t size_read;
	if ((size_read = read(fd, event_list, sizeof(event_list))) < (ssize_t)sizeof(struct input_event))
		return;

	for (unsigned int event_index = 0; event_index < size_read/sizeof(struct input_event); event_index++) {

		if (event_list[event_index].type == EV_KEY) {
			if (event_list[event_index].code >= 0x130 && event_list[event_index].code < 0x140)
				edev->gamepad = true;

			MTY_CButton cb = evdev_button(event_list[event_index].code);

			if (cb >= 0) {
				if (cb >= MTY_CBUTTON_MAX)
					continue;

				c->buttons[cb] = event_list[event_index].value != 0;
				report(edev, ctx->opaque);
			}

		} else if (event_list[event_index].type == EV_ABS) {
			// D-pad
			if (event_list[event_index].code == ABS_HAT0X) {
				c->buttons[MTY_CBUTTON_DPAD_RIGHT] = event_list[event_index].value > 0;
				c->buttons[MTY_CBUTTON_DPAD_LEFT] = event_list[event_index].value < 0;

			} else if (event_list[event_index].code == ABS_HAT0Y) {
				c->buttons[MTY_CBUTTON_DPAD_UP] = event_list[event_index].value < 0;
				c->buttons[MTY_CBUTTON_DPAD_DOWN] = event_list[event_index].value > 0;

			// Axes
			} else {
				uint16_t usage = edev->gamepad ? evdev_gamepad_usage(edev, event_list[event_index].code, event_list + event_index) :
					evdev_joystick_usage(edev, event_list[event_index].code, event_list + event_index);

				if (usage > 0) {
					uint8_t slot = edev->ainfo[event_list[event_index].code].slot;

					if (slot >= MTY_CAXIS_MAX)
						continue;

					c->axes[slot].value  = event_list[event_index].value;
					c->axes[slot].usage  = usage;
					c->axes[slot].min = edev->ainfo[event_list[event_index].code].min;
					c->axes[slot].max = edev->ainfo[event_list[event_index].code].max;
				}
			}
		}

		if (event_list[event_index].type != EV_SYN)
			report(edev, ctx->opaque);
	}
}

static void evdev_initial_scan(struct evdev *ctx)
{
	struct udev_enumerate *enumerate = udev_enumerate_new(ctx->udev);
	if (!enumerate)
		return;

	udev_enumerate_add_match_subsystem(enumerate, "input");
	udev_enumerate_scan_devices(enumerate);

	struct udev_list_entry *devs = udev_enumerate_get_list_entry(enumerate);
	for (struct udev_list_entry *item = devs; item; item = udev_list_entry_get_next(item)) {
		const char *syspath = udev_list_entry_get_name(item);
		struct udev_device *dev = udev_device_new_from_syspath(ctx->udev, syspath);

		if (dev) {
			const char *devnode = udev_device_get_devnode(dev);
			if (devnode && strstr(devnode, "/event"))
				evdev_device_add(ctx, devnode, syspath);
		}
	}

	udev_enumerate_unref(enumerate);
}

struct evdev *mty_evdev_create(EVDEV_CONNECT connect, EVDEV_DISCONNECT disconnect, void *opaque)
{
	if (!libudev_global_init())
		return NULL;

	bool r = true;

	struct evdev *ctx = MTY_Alloc(1, sizeof(struct evdev));
	ctx->connect = connect;
	ctx->disconnect = disconnect;
	ctx->opaque = opaque;
	ctx->devices = MTY_HashCreate(0);
	ctx->devices_rev = MTY_HashCreate(0);

	for (uint8_t x = 0; x < EVDEV_FD_MAX; x++) {
		ctx->fds[x].fd = -1;
		ctx->fds[x].events = POLLIN;
	}

	ctx->udev = udev_new();
	if (!ctx->udev) {
		r = false;
		MTY_Log("'udev_new' failed");
		goto except;
	}

	ctx->udev_monitor = udev_monitor_new_from_netlink(ctx->udev, "udev");
	if (!ctx->udev_monitor) {
		r = false;
		MTY_Log("'udev_monitor_new_from_netlink' failed");
		goto except;
	}

	int32_t e = udev_monitor_enable_receiving(ctx->udev_monitor);
	if (e < 0) {
		r = false;
		MTY_Log("'udev_monitor_enable_receiving' failed with error %d", e);
		goto except;
	}

	e = udev_monitor_filter_add_match_subsystem_devtype(ctx->udev_monitor, "input", NULL);
	if (e < 0) {
		r = false;
		MTY_Log("'udev_monitor_filter_add_match_subsystem_devtype' failed with error %d", e);
		goto except;
	}

	ctx->fds[0].fd = udev_monitor_get_fd(ctx->udev_monitor);
	if (ctx->fds[0].fd < 0) {
		r = false;
		MTY_Log("'udev_monitor_get_fd' failed with error %d", ctx->fds[0].fd);
		goto except;
	}

	except:

	if (!r)
		mty_evdev_destroy(&ctx);

	return ctx;
}

void mty_evdev_poll(struct evdev *ctx, EVDEV_REPORT report)
{
	// Fire off an initial enumerate to populate already connected joysticks
	if (!ctx->init_scan) {
		evdev_initial_scan(ctx);
		ctx->init_scan = true;
	}

	// Poll the monitor file descriptor (0) and any additional open joysticks
	int32_t e = poll(ctx->fds, EVDEV_FD_MAX, 0);
	if (e <= 0)
		return;

	for (uint8_t x = 0; x < EVDEV_FD_MAX; x++) {
		if (ctx->fds[x].revents & POLLIN) {
			// udev_monitor fd
			if (x == 0) {
				evdev_new_device(ctx);

			// evdev event
			} else {
				evdev_joystick_event(ctx, ctx->fds[x].fd, report);
			}
		}
	}
}

void mty_evdev_destroy(struct evdev **evdev)
{
	if (!evdev || !*evdev)
		return;

	struct evdev *ctx = *evdev;

	if (ctx->udev_monitor)
		udev_monitor_unref(ctx->udev_monitor);

	if (ctx->udev)
		udev_unref(ctx->udev);

	for (uint8_t x = 0; x < EVDEV_FD_MAX; x++)
		if (ctx->fds[x].fd != -1)
			close(ctx->fds[x].fd);

	MTY_HashDestroy(&ctx->devices, evdev_device_destroy);
	MTY_HashDestroy(&ctx->devices_rev, NULL);

	MTY_Free(ctx);
	*evdev = NULL;
}

MTY_ControllerEvent mty_evdev_state(struct evdev_dev *ctx)
{
	return ctx->state;
}

void mty_evdev_rumble(struct evdev *ctx, uint32_t id, uint16_t low, uint16_t high)
{
	struct evdev_dev *edev = MTY_HashGetInt(ctx->devices_rev, id);
	if (!edev)
		return;

	if (!edev->rumble)
		return;

	// evdev can only store a certain number of effects, make sure to delete them
	if (edev->ff.id != -1) {
		ioctl(ctx->fds[edev->slot].fd, EVIOCRMFF, edev->ff.id);
		edev->ff.id = -1;
	}

	edev->ff.u.rumble.strong_magnitude = low;
	edev->ff.u.rumble.weak_magnitude = high;

	// Upload the effect
	if (ioctl(ctx->fds[edev->slot].fd, EVIOCSFF, &edev->ff) != -1) {
		struct input_event ev = {0};
		ev.type = EV_FF;
		ev.code = edev->ff.id;
		ev.value = 1;

		// Write the effect
		if (write(ctx->fds[edev->slot].fd, &ev, sizeof(struct input_event)) == -1)
			MTY_Log("'write' failed with errno %d", errno);
	}
}
