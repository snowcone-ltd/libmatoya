// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

// Leaving typedef in place for potential future export

typedef struct MTY_Device MTY_Device;
typedef struct MTY_Context MTY_Context;
typedef struct MTY_Surface MTY_Surface;

typedef struct {
	void *device;
	const void *physicalDeviceMemoryProperties;
} MTY_VkDeviceObjects;
