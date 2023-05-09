// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "http.h"
#include "dl/libcurl.h"

static void net_set_url(CURL *curl, const char *url)
{
	char *eurl = NULL;

	if (curl_url) {
		CURLU *u = curl_url();
		CURLUcode ue = curl_url_set(u, CURLUPART_URL, url, CURLU_URLENCODE);

		if (ue == CURLUE_OK)
			curl_url_get(u, CURLUPART_URL, &eurl, 0);

		curl_url_cleanup(u);
	}

	curl_easy_setopt(curl, CURLOPT_URL, eurl ? eurl : url);
	curl_free(eurl);
}
