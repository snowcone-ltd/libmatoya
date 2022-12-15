// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "net.h"

struct http_header;

void mty_http_header_destroy(struct http_header **header);
bool mty_http_get_status_code(struct http_header *h, uint16_t *status_code);
bool mty_http_get_header_int(struct http_header *h, const char *key, int32_t *val);
bool mty_http_get_header_str(struct http_header *h, const char *key, const char **val);
void mty_http_set_header_int(char **header, const char *name, int32_t val);
void mty_http_set_header_str(char **header, const char *name, const char *val);

struct http_header *mty_http_read_header(struct net *net, uint32_t timeout);
bool mty_http_write_response_header(struct net *net, const char *code, const char *reason, const char *headers);
bool mty_http_write_request_header(struct net *net, const char *method, const char *path, const char *headers);

bool mty_http_should_proxy(const char **host, uint16_t *port);
bool mty_http_proxy_connect(struct net *net, uint16_t port, uint32_t timeout);
