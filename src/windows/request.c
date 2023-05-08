// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <stdlib.h>
#include <stdio.h>

#include "net-common.h"

bool MTY_HttpRequest(const char *url, const char *method, const char *headers,
	const void *body, size_t bodySize, const char *proxy, uint32_t timeout,
	void **response, size_t *responseSize, uint16_t *status)
{
	*responseSize = 0;
	*response = NULL;

	HINTERNET session = NULL;
	HINTERNET connect = NULL;
	HINTERNET request = NULL;

	// Parse URL
	bool r = net_connect(url, method, headers, body, bodySize, NULL, proxy, timeout, NULL, false,
		&session, &connect, &request);
	if (!r) {
		r = false;
		goto except;
	}

	// Read response headers
	r = WinHttpReceiveResponse(request, NULL);
	if (!r)
		goto except;

	// Status code query
	r = net_get_status_code(request, status);
	if (!r)
		goto except;

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

	if (!r) {
		MTY_Free(*response);
		*responseSize = 0;
		*response = NULL;
	}

	return r;
}
