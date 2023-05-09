// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#define MTY_USER_AGENT "libmatoya/v" MTY_VERSION_STRING
#define MTY_USER_AGENTW L"libmatoya/v" MTY_VERSION_STRINGW

void mty_http_parse_headers(const char *all,
	void (*func)(const char *key, const char *val, void *opaque), void *opaque);
char *mty_http_fix_scheme(const char *url);
