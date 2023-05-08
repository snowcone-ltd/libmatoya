// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <stdlib.h>
#include <stdio.h>

#include <windows.h>
#include <winhttp.h>

#include "net-common.h"

bool MTY_HttpRequest(const char *url, const char *method, const char *headers,
	const void *body, size_t bodySize, const char *proxy, uint32_t timeout,
	void **response, size_t *responseSize, uint16_t *status)
{
	*responseSize = 0;
	*response = NULL;

	bool r = true;

	HINTERNET session = NULL;
	HINTERNET connect = NULL;
	HINTERNET request = NULL;

	WCHAR *wmethod = MTY_MultiToWideD(method);

	// Proxy
	DWORD access_type = WINHTTP_ACCESS_TYPE_DEFAULT_PROXY;
	WCHAR *wproxy = WINHTTP_NO_PROXY_NAME;

	if (proxy) {
		access_type = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
		wproxy = MTY_MultiToWideD(proxy);
	}

	// Parse URL
	struct net_args nargs = {0};
	if (!net_make_args(url, headers, &nargs)) {
		r = false;
		goto except;
	}

	// Context initialization
	session = WinHttpOpen(nargs.ua ? nargs.ua : MTY_USER_AGENTW, access_type, wproxy, WINHTTP_NO_PROXY_BYPASS, 0);
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

	connect = WinHttpConnect(session, nargs.host, nargs.port, 0);
	if (!connect) {
		r = false;
		goto except;
	}

	// HTTP TCP/TLS connection
	request = WinHttpOpenRequest(connect, wmethod, nargs.path, NULL, WINHTTP_NO_REFERER,
		WINHTTP_DEFAULT_ACCEPT_TYPES, nargs.secure ? WINHTTP_FLAG_SECURE : 0);
	if (!request) {
		r = false;
		goto except;
	}

	// Write headers and body
	DWORD hlen = nargs.headers == WINHTTP_NO_ADDITIONAL_HEADERS ? -1L : 0;
	r = WinHttpSendRequest(request, nargs.headers, hlen, (void *) body, (DWORD) bodySize, (DWORD) bodySize, 0);
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

	net_free_args(&nargs);

	if (wproxy != WINHTTP_NO_PROXY_NAME)
		MTY_Free(wproxy);

	MTY_Free(wmethod);

	if (!r) {
		MTY_Free(*response);
		*responseSize = 0;
		*response = NULL;
	}

	return r;
}
