// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"
#include "http.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "tlocal.h"
#include "net/http-parse.h"
#include "net/http-proxy.h"


// Standard header generators

static const char HTTP_REQUEST_FMT[] =
	"%s %s HTTP/1.1\r\n"
	"Host: %s\r\n"
	"%s"
	"\r\n";

static const char HTTP_CONNECT_FMT[] =
	"CONNECT %s:%u HTTP/1.1\r\n"
	"%s"
	"\r\n";

static const char HTTP_RESPONSE_FMT[] =
	"HTTP/1.1 %s %s\r\n"
	"%s"
	"\r\n";

static char *http_request(const char *method, const char *host, const char *path, const char *fields)
{
	if (!fields)
		fields = "";

	size_t size = snprintf(NULL, 0, HTTP_REQUEST_FMT, method, path, host, fields) + 1;
	char *str = MTY_Alloc(size, 1);

	snprintf(str, size, HTTP_REQUEST_FMT, method, path, host, fields);

	return str;
}

static char *http_connect(const char *host, uint16_t port, const char *fields)
{
	if (!fields)
		fields = "";

	size_t size = snprintf(NULL, 0, HTTP_CONNECT_FMT, host, port, fields) + 1;
	char *str = MTY_Alloc(size, 1);

	snprintf(str, size, HTTP_CONNECT_FMT, host, port, fields);

	return str;
}

static char *http_response(const char *code, const char *msg, const char *fields)
{
	if (!fields)
		fields = "";

	size_t size = snprintf(NULL, 0, HTTP_RESPONSE_FMT, code, msg, fields) + 1;
	char *str = MTY_Alloc(size, 1);

	snprintf(str, size, HTTP_RESPONSE_FMT, code, msg, fields);

	return str;
}


// Header parsing, construction

struct http_pair {
	char *key;
	char *val;
};

struct http_header {
	char *first_line;
	struct http_pair *pairs;
	uint32_t npairs;
};

static struct http_header *mty_http_parse_header(const char *header)
{
	struct http_header *h = MTY_Alloc(1, sizeof(struct http_header));
	char *dup = MTY_Strdup(header);

	// HTTP header lines are delimited by "\r\n"
	char *ptr = NULL;
	char *line = MTY_Strtok(dup, "\r\n", &ptr);

	for (bool first = true; line; first = false) {

		// First line is special and is stored seperately
		if (first) {
			h->first_line = MTY_Strdup(line);

		// All lines following the first are in the "key: val" format
		} else {
			char *delim = strpbrk(line, ": ");

			if (delim) {
				h->pairs = MTY_Realloc(h->pairs, h->npairs + 1, sizeof(struct http_pair));

				// Place a null character to separate the line
				char save = delim[0];
				delim[0] = '\0';

				// Save the key and remove the null character
				h->pairs[h->npairs].key = MTY_Strdup(line);
				delim[0] = save;

				// Advance the val past whitespace or the : character
				while (*delim && (*delim == ':' || *delim == ' '))
					delim++;

				// Store the val and increment npairs
				h->pairs[h->npairs].val = MTY_Strdup(delim);
				h->npairs++;
			}
		}

		line = MTY_Strtok(NULL, "\r\n", &ptr);
	}

	MTY_Free(dup);

	return h;
}

void mty_http_header_destroy(struct http_header **header)
{
	if (!header || !*header)
		return;

	struct http_header *h = *header;

	for (uint32_t x = 0; x < h->npairs; x++) {
		MTY_Free(h->pairs[x].key);
		MTY_Free(h->pairs[x].val);
	}

	MTY_Free(h->first_line);
	MTY_Free(h->pairs);

	MTY_Free(h);
	*header = NULL;
}

bool mty_http_get_status_code(struct http_header *h, uint16_t *status_code)
{
	bool r = true;
	char *dup = MTY_Strdup(h->first_line);

	char *ptr = NULL;
	char *tok = MTY_Strtok(dup, " ", &ptr);
	if (!tok) {
		r = false;
		goto except;
	}

	tok = MTY_Strtok(NULL, " ", &ptr);
	if (!tok) {
		r = false;
		goto except;
	}

	*status_code = (uint16_t) strtol(tok, NULL, 10);

	except:

	MTY_Free(dup);

	return r;
}

bool mty_http_get_header_int(struct http_header *h, const char *key, int32_t *val)
{
	for (uint32_t x = 0; x < h->npairs; x++) {
		if (!MTY_Strcasecmp(key, h->pairs[x].key)) {
			char *endptr = h->pairs[x].val;
			*val = strtol(h->pairs[x].val, &endptr, 10);

			return endptr != h->pairs[x].val;
		}
	}

	return false;
}

bool mty_http_get_header_str(struct http_header *h, const char *key, const char **val)
{
	for (uint32_t x = 0; x < h->npairs; x++) {
		if (!MTY_Strcasecmp(key, h->pairs[x].key)) {
			*val = h->pairs[x].val;
			return true;
		}
	}

	return false;
}

void mty_http_set_header_int(char **header, const char *name, int32_t val)
{
	size_t len = *header ? strlen(*header) : 0;
	size_t new_len = len + strlen(name) + 32;

	*header = MTY_Realloc(*header, new_len, 1);
	snprintf(*header + len, new_len, "%s: %d\r\n", name, val);
}

void mty_http_set_header_str(char **header, const char *name, const char *val)
{
	size_t len = *header ? strlen(*header) : 0;
	size_t new_len = len + strlen(name) + strlen(val) + 32;

	*header = MTY_Realloc(*header, new_len, 1);
	snprintf(*header + len, new_len, "%s: %s\r\n", name, val);
}


// Wrapped header IO

#define HTTP_HEADER_MAX (16 * 1024)

static char *http_read_header_io(struct net *net, uint32_t timeout)
{
	bool r = false;
	char *h = MTY_Alloc(HTTP_HEADER_MAX, 1);

	for (uint32_t x = 0; x < HTTP_HEADER_MAX - 1; x++) {
		if (!mty_net_read(net, h + x, 1, timeout))
			break;

		if (x > 2 && h[x - 3] == '\r' && h[x - 2] == '\n' && h[x - 1] == '\r' && h[x] == '\n') {
			r = true;
			break;
		}
	}

	if (!r) {
		MTY_Free(h);
		h = NULL;
	}

	return h;
}

struct http_header *mty_http_read_header(struct net *net, uint32_t timeout)
{
	char *header = http_read_header_io(net, timeout);
	if (!header)
		return NULL;

	struct http_header *h = mty_http_parse_header(header);
	MTY_Free(header);

	return h;
}

bool mty_http_write_response_header(struct net *net, const char *code, const char *reason, const char *headers)
{
	char *hstr = http_response(code, reason, headers);
	bool r = mty_net_write(net, hstr, strlen(hstr));

	MTY_Free(hstr);

	return r;
}

bool mty_http_write_request_header(struct net *net, const char *method, const char *path, const char *headers)
{
	char *hstr = http_request(method, mty_net_get_host(net), path, headers);
	bool r = mty_net_write(net, hstr, strlen(hstr));

	MTY_Free(hstr);

	return r;
}


// HTTP Proxy IO and global settings

bool mty_http_should_proxy(const char **host, uint16_t *port)
{
	const char *proxy = mty_http_get_proxy();
	if (!proxy)
		return false;

	bool secure = false;
	char hbuf[MTY_URL_MAX] = {0};

	bool r = mty_http_parse_url(proxy, &secure, hbuf, MTY_URL_MAX, port, NULL, 0);
	if (r)
		*host = mty_tlocal_strcpy(hbuf);

	return r;
}

bool mty_http_proxy_connect(struct net *net, uint16_t port, uint32_t timeout)
{
	struct http_header *hdr = NULL;
	char *h = http_connect(mty_net_get_host(net), port, NULL);

	// Write the header to the HTTP client/server
	bool r = mty_net_write(net, h, strlen(h));
	MTY_Free(h);

	if (!r)
		goto except;

	// Read the response header
	hdr = mty_http_read_header(net, timeout);
	if (!hdr) {
		r = false;
		goto except;
	}

	// Get the status code
	uint16_t status_code = 0;
	r = mty_http_get_status_code(hdr, &status_code);
	if (!r)
		goto except;

	if (status_code != 200) {
		r = false;
		goto except;
	}

	except:

	mty_http_header_destroy(&hdr);

	return r;
}
