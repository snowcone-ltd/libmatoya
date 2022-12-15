// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define HTTP_PORT   80
#define HTTP_PORT_S 443

void mty_http_parse_headers(const char *all,
	void (*func)(const char *key, const char *val, void *opaque), void *opaque);
bool mty_http_parse_url(const char *url, bool *secure, char *host, size_t hostSize,
	uint16_t *port, char *path, size_t pathSize);
