// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#define TLOCAL __thread

void *mty_tlocal(size_t size);
char *mty_tlocal_strcpy(const char *str);
