// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <stdlib.h>
#include <string.h>

#include "net.h"
#include "http.h"
#include "dl/libcurl.h"

struct request_parse_args {
	struct curl_slist **slist;
	bool ua_found;
};

struct request_response {
	uint8_t *data;
	size_t size;
};

static void request_parse_headers(const char *key, const char *val, void *opaque)
{
	struct request_parse_args *pargs = opaque;

	if (!MTY_Strcasecmp(key, "User-Agent"))
		pargs->ua_found = true;

	*pargs->slist = curl_slist_append(*pargs->slist, MTY_SprintfDL("%s: %s", key, val));
}

static size_t request_write_func(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	size_t realsize = size * nmemb;
	struct request_response *res = userdata;

	res->data = MTY_Realloc(res->data, res->size + realsize + 1, 1);

	memcpy(res->data + res->size, ptr, realsize);
	res->size += realsize;
	res->data[res->size] = 0;

	return realsize;
}

bool MTY_HttpRequest(const char *url, const char *method, const char *headers,
	const void *body, size_t bodySize, const char *proxy, uint32_t timeout,
	void **response, size_t *responseSize, uint16_t *status)
{
	*responseSize = 0;
	*response = NULL;

	if (!libcurl_global_init())
		return false;

	CURL *curl = curl_easy_init();
	if (!curl) {
		MTY_Log("'curl_easy_init' failed");
		return false;
	}

	bool r = true;
	struct curl_slist *slist = NULL;
	struct request_response res = {0};

	// No signals
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);

	// Timeouts
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, timeout);
	curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, timeout);
	curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1);

	// Method
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);

	// URL
	curl_easy_setopt(curl, CURLOPT_URL, url);

	// Request headers
	struct request_parse_args pargs = {.slist = &slist};

	if (headers)
		mty_http_parse_headers(headers, request_parse_headers, &pargs);

	if (!pargs.ua_found)
		slist = curl_slist_append(slist, "User-Agent: " MTY_USER_AGENT);

	if (slist)
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);

	// Body
	if (body && bodySize > 0) {
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, bodySize);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
	}

	// Proxy
	if (proxy)
		curl_easy_setopt(curl, CURLOPT_PROXY, proxy);

	// Send request, receive response
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, request_write_func);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &res);

	CURLcode e = curl_easy_perform(curl);
	if (e != CURLE_OK) {
		MTY_Log("'curl_easy_perform' failed with error %d", e);
		r = false;
		goto except;
	}

	// Look for status code in response headers
	long code = 0;
	e = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
	if (e != CURLE_OK) {
		MTY_Log("'curl_easy_getinfo' failed with error %d", e);
		r = false;
		goto except;
	}

	*status = code;

	if (res.size > 0) {
		*responseSize = res.size;
		*response = res.data;
	}

	except:

	if (slist)
		curl_slist_free_all(slist);

	if (!r)
		MTY_Free(res.data);

	curl_easy_cleanup(curl);

	return r;
}
