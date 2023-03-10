// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

struct gfx_device;
struct gfx_context;
struct gfx_surface;

struct vk_device_objects {
	void *device;
	const void *physicalDeviceMemoryProperties;
};
