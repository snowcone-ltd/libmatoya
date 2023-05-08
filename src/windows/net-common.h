// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include "http.h"

struct net_args {
	bool secure;
	uint16_t port;
	WCHAR *ua;
	WCHAR *host;
	WCHAR *path;
	WCHAR *headers;
	char *_headers;
};

static void net_append_header(char **header, const char *name, const char *val)
{
	size_t len = *header ? strlen(*header) : 0;
	size_t new_len = len + strlen(name) + strlen(val) + 32;

	*header = MTY_Realloc(*header, new_len, 1);
	snprintf(*header + len, new_len, "%s: %s\r\n", name, val);
}

static void net_parse_headers(const char *key, const char *val, void *opaque)
{
	struct net_args *args = opaque;

	// WinHTTP needs to take the user-agent as a separate argument, so we need
	// to pluck it out of the headers string
	if (!MTY_Strcasecmp(key, "User-Agent")) {
		if (!args->ua)
			args->ua = MTY_MultiToWideD(val);

	} else {
		net_append_header(&args->_headers, key, val);
	}
}

static void net_free_args(struct net_args *args)
{
	if (args->headers != WINHTTP_NO_ADDITIONAL_HEADERS)
		MTY_Free(args->headers);

	MTY_Free(args->_headers);
	MTY_Free(args->host);
	MTY_Free(args->path);
	MTY_Free(args->ua);

	memset(args, 0, sizeof(struct net_args));
}

static bool net_make_args(const char *url, const char *headers, struct net_args *args)
{
	char *furl = mty_http_fix_scheme(url);
	WCHAR *wurl = MTY_MultiToWideD(furl);

	args->headers = WINHTTP_NO_ADDITIONAL_HEADERS;

	if (headers)
		mty_http_parse_headers(headers, net_parse_headers, args);

	if (args->_headers)
		args->headers = MTY_MultiToWideD(args->_headers);

	URL_COMPONENTS cmp = {
		.dwStructSize = sizeof(URL_COMPONENTS),
		.dwHostNameLength = (DWORD) -1,
		.dwUrlPathLength = (DWORD) -1,
		.dwExtraInfoLength = (DWORD) -1,
	};

	if (!WinHttpCrackUrl(wurl, 0, 0, &cmp)) {
		MTY_Log("'WinHttpCrackUrl' failed to parse '%s' with error 0x%X", url, GetLastError());
		return false;
	}

	args->host = MTY_Alloc(cmp.dwHostNameLength + 1, sizeof(WCHAR));
	memcpy(args->host, cmp.lpszHostName, cmp.dwHostNameLength * sizeof(WCHAR));

	args->path = MTY_Alloc(cmp.dwUrlPathLength + cmp.dwExtraInfoLength + 1, sizeof(WCHAR));
	memcpy(args->path, cmp.lpszUrlPath, cmp.dwUrlPathLength * sizeof(WCHAR));
	memcpy(args->path + cmp.dwUrlPathLength, cmp.lpszExtraInfo, cmp.dwExtraInfoLength * sizeof(WCHAR));

	args->secure = cmp.nScheme == INTERNET_SCHEME_HTTPS;
	args->port = cmp.nPort;

	if (args->port == 0)
		args->port = args->secure ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;

	MTY_Free(wurl);
	MTY_Free(furl);

	return true;
}
