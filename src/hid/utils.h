// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include "matoya.h"

void mty_hid_u_to_s16(MTY_Axis *v, bool invert);
void mty_hid_s_to_s16(MTY_Axis *v);
void mty_hid_u_to_u8(MTY_Axis *v);
void mty_hid_axis_to_dpad(int16_t v, MTY_ControllerEvent *c);
void mty_hid_map_axes(MTY_ControllerEvent *c);
bool mty_hid_dedupe(MTY_Hash *h, MTY_ControllerEvent *c);
