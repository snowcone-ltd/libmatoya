// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "tcp.h"

#include <stdlib.h>
#include <string.h>

#include "net/sock.h"

struct tcp {
	SOCKET s;
};


// Connect, accept

static bool tcp_socket_ok(SOCKET s)
{
	int32_t opt = 0;
	socklen_t size = sizeof(int32_t);
	int32_t e = getsockopt(s, SOL_SOCKET, SO_ERROR, (char *) &opt, &size);

	return e == 0 && opt == 0;
}

static void tcp_set_sockopt(SOCKET s, int32_t level, int32_t opt_name, int32_t val)
{
	setsockopt(s, level, opt_name, (const char *) &val, sizeof(int32_t));
}

static void tcp_set_options(SOCKET s)
{
	tcp_set_sockopt(s, SOL_SOCKET, SO_RCVBUF, 64 * 1024);
	tcp_set_sockopt(s, SOL_SOCKET, SO_SNDBUF, 64 * 1024);
	tcp_set_sockopt(s, SOL_SOCKET, SO_KEEPALIVE, 1);
	tcp_set_sockopt(s, SOL_SOCKET, SO_REUSEADDR, 1);

	tcp_set_sockopt(s, IPPROTO_TCP, TCP_NODELAY, 1);
}

static struct tcp *tcp_create(const char *ip, uint16_t port, struct sockaddr_in *addr)
{
	bool r = true;

	struct tcp *ctx = MTY_Alloc(1, sizeof(struct tcp));

	ctx->s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (ctx->s == INVALID_SOCKET) {
		r = false;
		goto except;
	}

	r = sock_set_nonblocking(ctx->s);
	if (!r)
		goto except;

	tcp_set_options(ctx->s);

	memset(addr, 0, sizeof(struct sockaddr_in));
	addr->sin_family = AF_INET;
	addr->sin_port = htons(port);

	if (ip) {
		inet_pton(AF_INET, ip, &addr->sin_addr);

	} else {
		addr->sin_addr.s_addr = INADDR_ANY;
	}

	except:

	if (!r)
		mty_tcp_destroy(&ctx);

	return ctx;
}

struct tcp *mty_tcp_connect(const char *ip, uint16_t port, uint32_t timeout)
{
	struct sockaddr_in addr = {0};

	struct tcp *ctx = tcp_create(ip, port, &addr);
	if (!ctx)
		return NULL;

	bool r = true;

	// Since this is async, it will always return -1, no need to check it
	connect(ctx->s, (struct sockaddr *) &addr, sizeof(struct sockaddr_in));

	// Initial socket state must be 'in progress' for nonblocking connect
	if (SOCK_ERROR != SOCK_IN_PROGRESS) {
		r = false;
		goto except;
	}

	// Wait for socket to be ready to write
	if (mty_tcp_poll(ctx, true, timeout) != MTY_ASYNC_OK) {
		r = false;
		goto except;
	}

	// If the socket is clear of errors, we made a successful connection
	r = tcp_socket_ok(ctx->s);
	if (!r)
		goto except;

	except:

	if (!r)
		mty_tcp_destroy(&ctx);

	return ctx;
}

struct tcp *mty_tcp_listen(const char *ip, uint16_t port)
{
	struct sockaddr_in addr = {0};

	struct tcp *ctx = tcp_create(ip, port, &addr);
	if (!ctx)
		return NULL;

	bool r = true;

	if (bind(ctx->s, (struct sockaddr *) &addr, sizeof(struct sockaddr_in)) != 0) {
		MTY_Log("'bind' failed with errno %d", SOCK_ERROR);
		r = false;
		goto except;
	}

	if (listen(ctx->s, SOMAXCONN) != 0) {
		MTY_Log("'listen' failed with errno %d", SOCK_ERROR);
		r = false;
		goto except;
	}

	except:

	if (!r)
		mty_tcp_destroy(&ctx);

	return ctx;
}

struct tcp *mty_tcp_accept(struct tcp *ctx, uint32_t timeout)
{
	if (mty_tcp_poll(ctx, false, timeout) != MTY_ASYNC_OK)
		return NULL;

	bool r = true;

	struct tcp *child = MTY_Alloc(1, sizeof(struct tcp));

	child->s = accept(ctx->s, NULL, NULL);
	if (child->s == INVALID_SOCKET) {
		r = false;
		goto except;
	}

	r = sock_set_nonblocking(child->s);
	if (!r)
		goto except;

	tcp_set_options(child->s);

	except:

	if (!r)
		mty_tcp_destroy(&child);

	return child;
}

void mty_tcp_destroy(struct tcp **tcp)
{
	if (!tcp || !*tcp)
		return;

	struct tcp *ctx = *tcp;

	if (ctx->s != INVALID_SOCKET) {
		shutdown(ctx->s, SHUT_RDWR);
		closesocket(ctx->s);
	}

	MTY_Free(ctx);
	*tcp = NULL;
}


// Poll, read, write

MTY_Async mty_tcp_poll(struct tcp *ctx, bool out, uint32_t timeout)
{
	struct pollfd fd = {0};
	fd.events = out ? POLLOUT : POLLIN;
	fd.fd = ctx->s;

	int32_t e = poll(&fd, 1, timeout);

	return e == 0 ? MTY_ASYNC_CONTINUE : e < 0 ? MTY_ASYNC_ERROR : MTY_ASYNC_OK;
}

bool mty_tcp_write(struct tcp *ctx, const void *buf, size_t size)
{
	for (size_t total = 0; total < size;) {
		int32_t n = send(ctx->s, (const char *) buf + total, (int32_t) (size - total), 0);

		if (n <= 0)
			return false;

		total += n;
	}

	return true;
}

bool mty_tcp_read(struct tcp *ctx, void *buf, size_t size, uint32_t timeout)
{
	for (size_t total = 0; total < size;) {
		if (mty_tcp_poll(ctx, false, timeout) != MTY_ASYNC_OK)
			return false;

		int32_t n = recv(ctx->s, (char *) buf + total, (int32_t) (size - total), 0);

		if (n <= 0) {
			if (SOCK_ERROR != SOCK_WOULD_BLOCK)
				return false;

		} else {
			total += n;
		}
	}

	return true;
}


// DNS

bool mty_dns_query(const char *host, char *ip, size_t size)
{
	bool r = true;

	// IP4 only
	struct addrinfo hints = {0};
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	struct addrinfo *servinfo = NULL;
	int32_t e = getaddrinfo(host, NULL, &hints, &servinfo);
	if (e != 0) {
		r = false;
		goto except;
	}

	struct sockaddr_in *addr = (struct sockaddr_in *) servinfo->ai_addr;
	if (!inet_ntop(AF_INET, &addr->sin_addr, ip, size)) {
		MTY_Log("'inet_ntop' failed with errno %d", errno);
		r = false;
		goto except;
	}

	except:

	if (servinfo)
		freeaddrinfo(servinfo);

	return r;
}
