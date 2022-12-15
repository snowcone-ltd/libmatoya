// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <stdlib.h>
#include <string.h>

#include "net/net.h"
#include "net/gzip.h"
#include "net/http.h"
#include "net/http-parse.h"
#include "net/http-proxy.h"

#define MTY_USER_AGENT "libmatoya/v" MTY_VERSION_STRING

struct request_parse_args {
	char **headers;
	bool ua_found;
};

static bool http_read_chunk_len(struct net *net, uint32_t timeout, size_t *len)
{
	*len = 0;
	char len_buf[64] = {0};

	for (uint32_t x = 0; x < 64 - 1; x++) {
		if (!mty_net_read(net, len_buf + x, 1, timeout))
			break;

		if (x > 0 && len_buf[x - 1] == '\r' && len_buf[x] == '\n') {
			len_buf[x - 1] = '\0';
			*len = strtoul(len_buf, NULL, 16);

			return true;
		}
	}

	return false;
}

static bool http_read_chunked(struct net *net, void **res, size_t *size, uint32_t timeout)
{
	size_t chunk_len = 0;

	do {
		// Read the chunk size one byte at a time
		if (!http_read_chunk_len(net, timeout, &chunk_len))
			return false;

		// Overflow protection
		if (chunk_len > MTY_RES_MAX || *size + chunk_len > MTY_RES_MAX)
			return false;

		// Make room for chunk and "\r\n" after chunk
		*res = MTY_Realloc(*res, *size + chunk_len + 2, 1);

		// Read chunk into buffer with extra 2 bytes for "\r\n"
		if (!mty_net_read(net, (uint8_t *) *res + *size, chunk_len + 2, timeout))
			return false;

		*size += chunk_len;

		// Keep null character at the end of the buffer for protection
		memset((uint8_t *) *res + *size, 0, 1);

	} while (chunk_len > 0);

	return true;
}

static void request_parse_headers(const char *key, const char *val, void *opaque)
{
	struct request_parse_args *pargs = opaque;

	if (!MTY_Strcasecmp(key, "User-Agent"))
		pargs->ua_found = true;

	mty_http_set_header_str(pargs->headers, key, val);
}

bool MTY_HttpRequest(const char *host, uint16_t port, bool secure, const char *method,
	const char *path, const char *headers, const void *body, size_t bodySize,
	uint32_t timeout, void **response, size_t *responseSize, uint16_t *status)
{
	*responseSize = 0;
	*response = NULL;

	bool r = true;
	char *req = NULL;
	struct http_header *hdr = NULL;

	// Make the TCP/TLS connection
	struct net *net = mty_net_connect(host, port, secure, timeout);
	if (!net) {
		r = false;
		goto except;
	}

	// Set request headers
	mty_http_set_header_str(&req, "Connection", "close");

	struct request_parse_args pargs = {0};
	pargs.headers = &req;

	if (headers)
		mty_http_parse_headers(headers, request_parse_headers, &pargs);

	if (!pargs.ua_found)
		mty_http_set_header_str(&req, "User-Agent", MTY_USER_AGENT);

	if (bodySize)
		mty_http_set_header_int(&req, "Content-Length", (int32_t) bodySize);

	// Send the request header
	r = mty_http_write_request_header(net, method, path, req);
	if (!r)
		goto except;

	// Send the request body
	if (body && bodySize > 0) {
		r = mty_net_write(net, body, bodySize);
		if (!r)
			goto except;
	}

	// Read the response header
	hdr = mty_http_read_header(net, timeout);
	if (!hdr) {
		r = false;
		goto except;
	}

	// Get the status code
	r = mty_http_get_status_code(hdr, status);
	if (!r)
		goto except;

	// Read response body -- either fixed content length or chunked
	const char *val = NULL;
	if (mty_http_get_header_int(hdr, "Content-Length", (int32_t *) responseSize) && *responseSize > 0) {

		// Overflow protection
		if (*responseSize > MTY_RES_MAX) {
			r = false;
			goto except;
		}

		*response = MTY_Alloc(*responseSize + 1, 1);

		r = mty_net_read(net, *response, *responseSize, timeout);
		if (!r)
			goto except;

	} else if (mty_http_get_header_str(hdr, "Transfer-Encoding", &val) && !MTY_Strcasecmp(val, "chunked")) {
		r = http_read_chunked(net, response, responseSize, timeout);
		if (!r)
			goto except;
	}

	// Check for content-encoding header and attempt to uncompress
	if (*response && *responseSize > 0) {
		if (mty_http_get_header_str(hdr, "Content-Encoding", &val) && !MTY_Strcasecmp(val, "gzip")) {
			size_t zlen = 0;
			void *z = mty_gzip_decompress(*response, *responseSize, &zlen);
			if (!z) {
				r = false;
				goto except;
			}

			MTY_Free(*response);
			*response = z;
			*responseSize = zlen;
		}
	}

	except:

	MTY_Free(req);
	mty_http_header_destroy(&hdr);
	mty_net_destroy(&net);

	if (!r) {
		MTY_Free(*response);
		*responseSize = 0;
		*response = NULL;
	}

	return r;
}
