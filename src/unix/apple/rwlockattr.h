// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

// Apple platforms prevent writer starvation by default, so no need
// to fool with rwlock attributes

#define mty_rwlockattr_set(attr)
