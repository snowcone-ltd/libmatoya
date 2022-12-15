// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"
#include "http-parse.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

void mty_http_parse_headers(const char *all,
	void (*func)(const char *key, const char *val, void *opaque), void *opaque)
{
	char *dup = MTY_Strdup(all);

	char *ptr = NULL;
	char *tok = MTY_Strtok(dup, "\n", &ptr);

	while (tok) {
		char *ptr2 = NULL;
		char *key = MTY_Strtok(tok, " :", &ptr2);
		if (!key)
			break;

		char *val = MTY_Strtok(NULL, "", &ptr2);
		if (!val)
			break;

		// Skip past leading whitespace in the val
		while (*val && (*val == ' ' || *val == '\t'))
			val++;

		func(key, val, opaque);

		tok = MTY_Strtok(NULL, "\n", &ptr);
	}

	MTY_Free(dup);
}

bool mty_http_parse_url(const char *url, bool *secure, char *host, size_t hostSize,
	uint16_t *port, char *path, size_t pathSize)
{
	bool r = true;
	char *dup = MTY_Strdup(url);

	*secure = false;
	*port = 0;

	// Scheme
	char *tok = NULL;
	char *ptr = NULL;

	if (strstr(dup, "http") || strstr(dup, "ws")) {
		tok = MTY_Strtok(dup, ":", &ptr);
		if (!tok) {
			r = false;
			goto except;
		}

		if (!MTY_Strcasecmp(tok, "https") || !MTY_Strcasecmp(tok, "wss")) {
			*secure = true;

		} else if (!MTY_Strcasecmp(tok, "http") || !MTY_Strcasecmp(tok, "ws")) {
			*secure = false;

		} else {
			r = false;
			goto except;
		}

		// Leave tok at host:port/path
		tok = MTY_Strtok(NULL, "/", &ptr);

	// No scheme, assume host:port/path
	} else {
		*secure = false;
		tok = MTY_Strtok(dup, "/", &ptr);
	}

	// Host
	if (!tok) {
		r = false;
		goto except;
	}

	// This buffer currently needs room for host:port, but strtok will null the ':' character
	snprintf(host, hostSize, "%s", tok);

	// Try to find a port
	char *ptr2 = NULL;
	char *tok2 = MTY_Strtok(host, ":", &ptr2);

	if (tok2)
		tok2 = MTY_Strtok(NULL, ":", &ptr2);

	if (tok2) { // We have a port
		*port = (uint16_t) atoi(tok2);

	} else {
		*port = *secure ? HTTP_PORT_S : HTTP_PORT;
	}

	// Path
	tok = MTY_Strtok(NULL, "", &ptr);
	if (!tok)
		tok = "";

	if (path)
		snprintf(path, pathSize, "/%s", tok);

	except:

	MTY_Free(dup);

	return r;
}

bool MTY_HttpParseUrl(const char *url, char *host, size_t hostSize, char *path, size_t pathSize)
{
	bool secure = false;
	uint16_t port = 0;

	return mty_http_parse_url(url, &secure, host, hostSize, &port, path, pathSize);
}

void MTY_HttpEncodeUrl(const char *src, char *dst, size_t size)
{
	memset(dst, 0, size);

	char table[256];
	for (int32_t c = 0; c < 256; c++)
		table[c] = isalnum(c) || c == '*' || c == '-' || c == '.' || c == '_' ? (char) c : (c == ' ') ? '+' : '\0';

	size_t src_len = strlen(src);

	for (size_t x = 0; x < src_len && size > 0; x++) {
		int32_t c = src[x];
		size_t inc = 1;

		if (table[c]) {
			snprintf(dst, size, "%c", table[c]);

		} else {
			snprintf(dst, size, "%%%02X", c);
			inc = 3;
		}

		if (inc > size)
			break;

		dst += inc;
		size -= inc;
	}
}
