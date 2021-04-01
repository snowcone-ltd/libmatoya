// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

struct evdev_dev;
struct evdev;

typedef void (*EVDEV_CONNECT)(struct evdev_dev *device, void *opaque);
typedef void (*EVDEV_DISCONNECT)(struct evdev_dev *device, void *opaque);
typedef void (*EVDEV_REPORT)(struct evdev_dev *device, void *opaque);

struct evdev *mty_evdev_create(EVDEV_CONNECT connect, EVDEV_DISCONNECT disconnect, void *opaque);
void mty_evdev_poll(struct evdev *ctx, EVDEV_REPORT report);
void mty_evdev_destroy(struct evdev **evdev);
MTY_ControllerEvent mty_evdev_state(struct evdev_dev *ctx);
void mty_evdev_rumble(struct evdev *ctx, uint32_t id, uint16_t low, uint16_t high);
