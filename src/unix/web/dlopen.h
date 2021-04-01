// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#define DLOPEN_FLAGS 0

#define dlsym(so, name) NULL
#define dlerror() NULL
#define dlopen(name, flags) NULL
#define dlclose(so) -1
