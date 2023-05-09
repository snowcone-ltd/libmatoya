// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"
#include "http.h"

#include <string.h>

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

char *mty_http_fix_scheme(const char *url)
{
	const char *scheme = NULL;

	if (MTY_Strcasestr(url, "wss") == url) {
		scheme = "https";

	} else if (MTY_Strcasestr(url, "ws") == url) {
		scheme = "http";
	}

	if (scheme) {
		char *furl = MTY_Alloc(strlen(url) + 3, 1);
		memcpy(furl + 2, url, strlen(url));
		memcpy(furl, scheme, strlen(scheme));

		return furl;
	}

	return MTY_Strdup(url);
}
