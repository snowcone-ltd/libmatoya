// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"
#include "http-proxy.h"

#include <stdio.h>

#include "tlocal.h"

static MTY_Atomic32 HTTP_GLOCK;
static char HTTP_PROXY[MTY_URL_MAX];

void MTY_HttpSetProxy(const char *proxy)
{
	MTY_GlobalLock(&HTTP_GLOCK);

	if (proxy) {
		snprintf(HTTP_PROXY, MTY_URL_MAX, "%s", proxy);

	} else {
		HTTP_PROXY[0] = '\0';
	}

	MTY_GlobalUnlock(&HTTP_GLOCK);
}

const char *mty_http_get_proxy(void)
{
	char *proxy = NULL;

	MTY_GlobalLock(&HTTP_GLOCK);

	if (HTTP_PROXY[0])
		proxy = mty_tlocal_strcpy(HTTP_PROXY);

	MTY_GlobalUnlock(&HTTP_GLOCK);

	return proxy;
}
