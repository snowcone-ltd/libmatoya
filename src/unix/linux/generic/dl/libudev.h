// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include "sym.h"


// Interface

struct udev;
struct udev_enumerate;
struct udev_list_entry;
struct udev_monitor;
struct udev_device;

static struct udev *(*udev_new)(void);
static struct udev *(*udev_unref)(struct udev *udev);
static struct udev_monitor *(*udev_monitor_new_from_netlink)(struct udev *udev, const char *name);
static int (*udev_monitor_enable_receiving)(struct udev_monitor *udev_monitor);
static int (*udev_monitor_filter_add_match_subsystem_devtype)(struct udev_monitor *udev_monitor,
	const char *subsystem, const char *devtype);
static struct udev_monitor *(*udev_monitor_unref)(struct udev_monitor *udev_monitor);
static int (*udev_monitor_get_fd)(struct udev_monitor *udev_monitor);
static struct udev_device *(*udev_monitor_receive_device)(struct udev_monitor *udev_monitor);
static struct udev_device *(*udev_device_new_from_syspath)(struct udev *udev, const char *syspath);
static const char *(*udev_device_get_syspath)(struct udev_device *udev_device);
static const char *(*udev_device_get_action)(struct udev_device *udev_device);
static const char *(*udev_device_get_devnode)(struct udev_device *udev_device);
static struct udev_device *(*udev_device_unref)(struct udev_device *udev_device);
static struct udev_enumerate *(*udev_enumerate_new)(struct udev *udev);
static int (*udev_enumerate_add_match_subsystem)(struct udev_enumerate *udev_enumerate, const char *subsystem);
static int (*udev_enumerate_scan_devices)(struct udev_enumerate *udev_enumerate);
static struct udev_list_entry *(*udev_enumerate_get_list_entry)(struct udev_enumerate *udev_enumerate);
static struct udev_enumerate *(*udev_enumerate_unref)(struct udev_enumerate *udev_enumerate);
static struct udev_list_entry *(*udev_list_entry_get_next)(struct udev_list_entry *list_entry);
static const char *(*udev_list_entry_get_name)(struct udev_list_entry *list_entry);


// Runtime open

static MTY_Atomic32 LIBUDEV_LOCK;
static MTY_SO *LIBUDEV_SO;
static bool LIBUDEV_INIT;

static void __attribute__((destructor)) libudev_global_destroy(void)
{
	MTY_GlobalLock(&LIBUDEV_LOCK);

	MTY_SOUnload(&LIBUDEV_SO);
	LIBUDEV_INIT = false;

	MTY_GlobalUnlock(&LIBUDEV_LOCK);
}

static bool libudev_global_init(void)
{
	MTY_GlobalLock(&LIBUDEV_LOCK);

	if (!LIBUDEV_INIT) {
		bool r = true;

		LIBUDEV_SO = MTY_SOLoad("libudev.so.1");
		if (!LIBUDEV_SO) {
			r = false;
			goto except;
		}

		LOAD_SYM(LIBUDEV_SO, udev_new);
		LOAD_SYM(LIBUDEV_SO, udev_unref);
		LOAD_SYM(LIBUDEV_SO, udev_monitor_new_from_netlink);
		LOAD_SYM(LIBUDEV_SO, udev_monitor_enable_receiving);
		LOAD_SYM(LIBUDEV_SO, udev_monitor_filter_add_match_subsystem_devtype);
		LOAD_SYM(LIBUDEV_SO, udev_monitor_unref);
		LOAD_SYM(LIBUDEV_SO, udev_monitor_get_fd);
		LOAD_SYM(LIBUDEV_SO, udev_monitor_receive_device);
		LOAD_SYM(LIBUDEV_SO, udev_device_new_from_syspath);
		LOAD_SYM(LIBUDEV_SO, udev_device_get_action);
		LOAD_SYM(LIBUDEV_SO, udev_device_get_syspath);
		LOAD_SYM(LIBUDEV_SO, udev_device_get_devnode);
		LOAD_SYM(LIBUDEV_SO, udev_device_unref);
		LOAD_SYM(LIBUDEV_SO, udev_enumerate_new);
		LOAD_SYM(LIBUDEV_SO, udev_enumerate_add_match_subsystem);
		LOAD_SYM(LIBUDEV_SO, udev_enumerate_scan_devices);
		LOAD_SYM(LIBUDEV_SO, udev_enumerate_get_list_entry);
		LOAD_SYM(LIBUDEV_SO, udev_enumerate_unref);
		LOAD_SYM(LIBUDEV_SO, udev_list_entry_get_next);
		LOAD_SYM(LIBUDEV_SO, udev_list_entry_get_name);

		except:

		if (!r)
			libudev_global_destroy();

		LIBUDEV_INIT = r;
	}

	MTY_GlobalUnlock(&LIBUDEV_LOCK);

	return LIBUDEV_INIT;
}
