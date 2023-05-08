// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"
#include "net.h"

#include "dl/libcurl.h"
#include "http.h"

#include <poll.h>

struct net {
	CURL *curl;
	curl_socket_t s;
};

bool mty_net_parse_url(const char *url, bool *secure, char **host, uint16_t *port, char **path)
{
	if (!libcurl_global_init())
		return NULL;

	return false;
}

struct net *mty_net_connect(const char *url, const char *proxy, uint32_t timeout)
{
	if (!libcurl_global_init())
		return NULL;

	struct net *ctx = MTY_Alloc(1, sizeof(struct net));
	bool r = true;

	ctx->curl = curl_easy_init();
	if (!ctx->curl) {
		MTY_Log("'curl_easy_init' failed");
		r = false;
		goto except;
	}

	// No HTTP, connection only
	curl_easy_setopt(ctx->curl, CURLOPT_CONNECT_ONLY, 1);
	curl_easy_setopt(ctx->curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);

	// No signals
	curl_easy_setopt(ctx->curl, CURLOPT_NOSIGNAL, 1);

	// Connect timeout
	curl_easy_setopt(ctx->curl, CURLOPT_CONNECTTIMEOUT_MS, timeout);

	// URL
	curl_easy_setopt(ctx->curl, CURLOPT_URL, url);

	// Proxy
	if (proxy)
		curl_easy_setopt(ctx->curl, CURLOPT_PROXY, proxy);

	// Connect
	CURLcode e = curl_easy_perform(ctx->curl);
	if (e != CURLE_OK) {
		MTY_Log("'curl_easy_perform' failed with error %d", e);
		r = false;
		goto except;
	}

	// Get socket for polling
	e = curl_easy_getinfo(ctx->curl, CURLINFO_ACTIVESOCKET, &ctx->s);
	if (e != CURLE_OK) {
		MTY_Log("'curl_easy_getinfo' failed with error %d", e);
		r = false;
		goto except;
	}

	except:

	if (!r)
		mty_net_destroy(&ctx);

	return ctx;
}

void mty_net_destroy(struct net **net)
{
	if (!net || !*net)
		return;

	struct net *ctx = *net;

	if (ctx->curl)
		curl_easy_cleanup(ctx->curl);

	MTY_Free(ctx);
	*net = NULL;
}

MTY_Async mty_net_poll(struct net *ctx, uint32_t timeout)
{
	struct pollfd fd = {
		.events = POLLIN,
		.fd = ctx->s,
	};

	int32_t e = poll(&fd, 1, timeout);

	return e == 0 ? MTY_ASYNC_CONTINUE : e < 0 ? MTY_ASYNC_ERROR : MTY_ASYNC_OK;
}

bool mty_net_write(struct net *ctx, const void *buf, size_t size)
{
	for (size_t total = 0; total < size;) {
		size_t n = 0;
		CURLcode e = curl_easy_send(ctx->curl, (uint8_t *) buf + total, size - total, &n);

		if (e != CURLE_OK || n == 0)
			return false;

		total += n;
	}

	return true;
}

bool mty_net_read(struct net *ctx, void *buf, size_t size, uint32_t timeout)
{
	for (size_t total = 0; total < size;) {
		size_t n = 0;
		CURLcode e = curl_easy_recv(ctx->curl, (uint8_t *) buf + total, size - total, &n);

		if (e != CURLE_OK) {
			if (e == CURLE_AGAIN)
				if (mty_net_poll(ctx, timeout) != MTY_ASYNC_OK)
					return false;

		} else if (n == 0) {
			return false;

		} else {
			total += n;
		}
	}

	return true;
}
