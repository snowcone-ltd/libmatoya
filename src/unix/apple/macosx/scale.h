// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

static CGFloat mty_screen_scale(NSScreen *screen)
{
	// Wrapper to protect against backingScaleFactor of zero

	CGFloat scale = screen.backingScaleFactor;

	return scale == 0.0f ? 1.0f : scale;
}
