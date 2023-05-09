// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include <windows.h>
#include <winhttp.h>

#include "http.h"

#define NET_WS_PING_INTERVAL 60000

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

static bool net_connect(const char *url, const char *method, const char *headers, const void *body, size_t bodySize,
	void *ctx, const char *proxy, int32_t timeout, WINHTTP_STATUS_CALLBACK async_callback, bool ws,
	HINTERNET *session, HINTERNET *connect, HINTERNET *request)
{
	bool r = true;

	DWORD access_type = WINHTTP_ACCESS_TYPE_DEFAULT_PROXY;
	WCHAR *wmethod = MTY_MultiToWideD(method);
	WCHAR *wproxy = WINHTTP_NO_PROXY_NAME;

	// Parse URL
	struct net_args nargs = {0};
	if (!net_make_args(url, headers, &nargs)) {
		r = false;
		goto except;
	}

	// Proxy
	if (proxy) {
		access_type = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
		wproxy = MTY_MultiToWideD(proxy);
	}

	// Context initialization
	DWORD flags = async_callback ? WINHTTP_FLAG_ASYNC : 0;
	*session = WinHttpOpen(nargs.ua ? nargs.ua : MTY_USER_AGENTW, access_type,
		wproxy, WINHTTP_NO_PROXY_BYPASS, flags);
	if (!*session) {
		r = false;
		goto except;
	}

	// Attempt to force TLS 1.2, ignore failure
	DWORD opt = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
	WinHttpSetOption(*session, WINHTTP_OPTION_SECURE_PROTOCOLS, &opt, sizeof(DWORD));

	// Handle gzip
	if (!ws) {
		opt = WINHTTP_DECOMPRESSION_FLAG_GZIP;
		WinHttpSetOption(*session, WINHTTP_OPTION_DECOMPRESSION, &opt, sizeof(DWORD));
	}

	// Set async callback
	if (async_callback) {
		if (WinHttpSetStatusCallback(*session, async_callback, WINHTTP_CALLBACK_FLAG_ALL_NOTIFICATIONS, 0)) {
			r = false;
			goto except;
		}
	}

	// Set timeouts
	if (timeout >= 0) {
		r = WinHttpSetTimeouts(*session, timeout, timeout, timeout, timeout);
		if (!r)
			goto except;
	}

	// TCP connection
	*connect = WinHttpConnect(*session, nargs.host, nargs.port, 0);
	if (!*connect) {
		r = false;
		goto except;
	}

	// TLS, HTTP connection
	*request = WinHttpOpenRequest(*connect, wmethod, nargs.path, NULL, WINHTTP_NO_REFERER,
		WINHTTP_DEFAULT_ACCEPT_TYPES, nargs.secure ? WINHTTP_FLAG_SECURE : 0);
	if (!*request) {
		r = false;
		goto except;
	}

	// WebSocket upgrade
	if (ws) {
		r = WinHttpSetOption(*request, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, NULL, 0);
		if (!r)
			goto except;

		opt = NET_WS_PING_INTERVAL;
		WinHttpSetOption(session, WINHTTP_OPTION_WEB_SOCKET_KEEPALIVE_INTERVAL, &opt, sizeof(DWORD));
	}

	// Write headers and body
	DWORD hlen = nargs.headers == WINHTTP_NO_ADDITIONAL_HEADERS ? -1L : 0;
	r = WinHttpSendRequest(*request, nargs.headers, hlen, (VOID *) body, (DWORD) bodySize,
		(DWORD) bodySize, (DWORD_PTR) ctx);
	if (!r)
		goto except;

	except:

	if (!r) {
		if (*request)
			WinHttpCloseHandle(*request);

		if (*connect)
			WinHttpCloseHandle(*connect);

		if (*session)
			WinHttpCloseHandle(*session);

		*request = NULL;
		*connect = NULL;
		*session = NULL;
	}

	net_free_args(&nargs);

	if (wproxy != WINHTTP_NO_PROXY_NAME)
		MTY_Free(wproxy);

	MTY_Free(wmethod);

	return r;
}

static bool net_get_status_code(HINTERNET request, uint16_t *status_code)
{
	*status_code = 0;

	WCHAR wheader[128];
	DWORD buf_len = 128 * sizeof(WCHAR);
	if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE, NULL, wheader, &buf_len, NULL))
		return false;

	*status_code = (uint16_t) _wtoi(wheader);

	return true;
}
