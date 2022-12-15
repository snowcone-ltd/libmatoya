// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"
#include "net.h"

#include <stdio.h>
#include <string.h>

#include "tcp.h"
#include "http.h"
#include "secure.h"
#include "net/dns.h"
#include "net/http-parse.h"

struct net {
	char *host;
	struct tcp *tcp;
	struct secure *sec;
};

struct net *mty_net_connect(const char *host, uint16_t port, bool secure, uint32_t timeout)
{
	struct net *ctx = MTY_Alloc(1, sizeof(struct net));
	ctx->host = MTY_Strdup(host);

	if (port == 0)
		port = secure ? HTTP_PORT_S : HTTP_PORT;

	// Check global HTTP proxy settings
	const char *chost = ctx->host;
	uint16_t cport = port;
	bool use_proxy = mty_http_should_proxy(&chost, &cport);

	// DNS resolve hostname into an ip address string
	char ip[64];
	bool r = mty_dns_query(chost, false, ip, 64);
	if (!r)
		goto except;

	// Make the tcp connection
	ctx->tcp = mty_tcp_connect(ip, cport, timeout);
	if (!ctx->tcp) {
		r = false;
		goto except;
	}

	// HTTP proxy CONNECT request
	if (use_proxy) {
		r = mty_http_proxy_connect(ctx, port, timeout);
		if (!r)
			goto except;
	}

	// TLS handshake
	if (secure) {
		ctx->sec = mty_secure_connect(ctx->tcp, ctx->host, timeout);
		if (!ctx->sec) {
			r = false;
			goto except;
		}
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

	mty_secure_destroy(&ctx->sec);
	mty_tcp_destroy(&ctx->tcp);

	MTY_Free(ctx->host);

	MTY_Free(ctx);
	*net = NULL;
}

MTY_Async mty_net_poll(struct net *ctx, uint32_t timeout)
{
	return mty_tcp_poll(ctx->tcp, false, timeout);
}

bool mty_net_write(struct net *ctx, const void *buf, size_t size)
{
	return ctx->sec ? mty_secure_write(ctx->sec, ctx->tcp, buf, size) :
		mty_tcp_write(ctx->tcp, buf, size);
}

bool mty_net_read(struct net *ctx, void *buf, size_t size, uint32_t timeout)
{
	return ctx->sec ? mty_secure_read(ctx->sec, ctx->tcp, buf, size, timeout) :
		mty_tcp_read(ctx->tcp, buf, size, timeout);
}

const char *mty_net_get_host(struct net *ctx)
{
	return ctx->host;
}
