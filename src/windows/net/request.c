// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <stdlib.h>
#include <stdio.h>

#include <windows.h>
#include <winhttp.h>

#include "net/http-parse.h"
#include "net/http-proxy.h"

#define MTY_USER_AGENTW L"libmatoya/v" MTY_VERSION_STRINGW

struct request_parse_args {
	WCHAR *ua;
	char *headers;
};

static void request_append_header(char **header, const char *name, const char *val)
{
	size_t len = *header ? strlen(*header) : 0;
	size_t new_len = len + strlen(name) + strlen(val) + 32;

	*header = MTY_Realloc(*header, new_len, 1);
	snprintf(*header + len, new_len, "%s: %s\r\n", name, val);
}

static void request_parse_headers(const char *key, const char *val, void *opaque)
{
	struct request_parse_args *pargs = opaque;

	// WinHTTP needs to take the user-agent as a separate argument, so we need
	// to pluck it out of the headers string
	if (!MTY_Strcasecmp(key, "User-Agent")) {
		if (!pargs->ua)
			pargs->ua = MTY_MultiToWideD(val);

	} else {
		request_append_header(&pargs->headers, key, val);
	}
}

bool MTY_HttpRequest(const char *host, uint16_t port, bool secure, const char *method,
	const char *path, const char *headers, const void *body, size_t bodySize,
	uint32_t timeout, void **response, size_t *responseSize, uint16_t *status)
{
	*responseSize = 0;
	*response = NULL;

	bool r = true;

	HINTERNET session = NULL;
	HINTERNET connect = NULL;
	HINTERNET request = NULL;

	struct request_parse_args pargs = {0};
	if (headers)
		mty_http_parse_headers(headers, request_parse_headers, &pargs);

	WCHAR *whost = MTY_MultiToWideD(host);
	WCHAR *wpath = MTY_MultiToWideD(path);
	WCHAR *wmethod = MTY_MultiToWideD(method);
	WCHAR *wheaders = pargs.headers ? MTY_MultiToWideD(pargs.headers) : WINHTTP_NO_ADDITIONAL_HEADERS;

	if (port == 0)
		port = secure ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;

	// Proxy
	const char *proxy = mty_http_get_proxy();
	DWORD access_type = WINHTTP_ACCESS_TYPE_DEFAULT_PROXY;
	WCHAR *wproxy = WINHTTP_NO_PROXY_NAME;

	if (proxy) {
		access_type = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
		wproxy = MTY_MultiToWideD(proxy);
	}

	// Context initialization
	session = WinHttpOpen(pargs.ua ? pargs.ua : MTY_USER_AGENTW, access_type, wproxy, WINHTTP_NO_PROXY_BYPASS, 0);
	if (!session) {
		r = false;
		goto except;
	}

	// Set timeouts
	r = WinHttpSetTimeouts(session, timeout, timeout, timeout, timeout);
	if (!r)
		goto except;

	// Attempt to force TLS 1.2, ignore failure
	DWORD opt = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
	WinHttpSetOption(session, WINHTTP_OPTION_SECURE_PROTOCOLS, &opt, sizeof(DWORD));

	// Handle gzip
	opt = WINHTTP_DECOMPRESSION_FLAG_GZIP;
	WinHttpSetOption(session, WINHTTP_OPTION_DECOMPRESSION, &opt, sizeof(DWORD));

	connect = WinHttpConnect(session, whost, port, 0);
	if (!connect) {
		r = false;
		goto except;
	}

	// HTTP TCP/TLS connection
	request = WinHttpOpenRequest(connect, wmethod, wpath, NULL, WINHTTP_NO_REFERER,
		WINHTTP_DEFAULT_ACCEPT_TYPES, secure ? WINHTTP_FLAG_SECURE : 0);
	if (!request) {
		r = false;
		goto except;
	}

	// Write headers and body
	r = WinHttpSendRequest(request, wheaders, pargs.headers ? -1L : 0,
		(void *) body, (DWORD) bodySize, (DWORD) bodySize, 0);
	if (!r)
		goto except;

	// Read response headers
	r = WinHttpReceiveResponse(request, NULL);
	if (!r)
		goto except;

	// Status code query
	WCHAR wheader[128];
	DWORD buf_len = 128 * sizeof(WCHAR);
	r = WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE, NULL, wheader, &buf_len, NULL);
	if (!r)
		goto except;

	*status = (uint16_t) _wtoi(wheader);

	// Receive response body
	while (true) {
		DWORD available = 0;
		r = WinHttpQueryDataAvailable(request, &available);
		if (!r)
			goto except;

		if (available == 0)
			break;

		// Overflow protection
		if (available > MTY_RES_MAX || *responseSize + available > MTY_RES_MAX) {
			r = false;
			goto except;
		}

		*response = MTY_Realloc(*response, *responseSize + available + 1, 1);

		DWORD read = 0;
		r = WinHttpReadData(request, (uint8_t *) *response + *responseSize, available, &read);
		if (!r)
			goto except;

		*responseSize += read;

		// Keep null character at the end of the buffer for protection
		memset((uint8_t *) *response + *responseSize, 0, 1);
	}

	except:

	if (request)
		WinHttpCloseHandle(request);

	if (connect)
		WinHttpCloseHandle(connect);

	if (session)
		WinHttpCloseHandle(session);

	if (wproxy != WINHTTP_NO_PROXY_NAME)
		MTY_Free(wproxy);

	if (wheaders != WINHTTP_NO_ADDITIONAL_HEADERS)
		MTY_Free(wheaders);

	MTY_Free(pargs.ua);
	MTY_Free(pargs.headers);
	MTY_Free(wmethod);
	MTY_Free(wpath);
	MTY_Free(whost);

	if (!r) {
		MTY_Free(*response);
		*responseSize = 0;
		*response = NULL;
	}

	return r;
}
