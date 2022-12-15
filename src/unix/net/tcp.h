// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include "matoya.h"

struct tcp;

struct tcp *mty_tcp_connect(const char *ip, uint16_t port, uint32_t timeout);
void mty_tcp_destroy(struct tcp **tcp);

MTY_Async mty_tcp_poll(struct tcp *ctx, bool out, uint32_t timeout);
bool mty_tcp_write(struct tcp *ctx, const void *buf, size_t size);
bool mty_tcp_read(struct tcp *ctx, void *buf, size_t size, uint32_t timeout);
