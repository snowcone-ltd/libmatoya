// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "secure.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define SECURE_PADDING (32 * 1024)

struct secure {
	MTY_TLS *tls;

	uint8_t *buf;
	size_t buf_size;

	uint8_t *pbuf;
	size_t pbuf_size;
	size_t pending;
};

void mty_secure_destroy(struct secure **secure)
{
	if (!secure || !*secure)
		return;

	struct secure *ctx = *secure;

	MTY_Free(ctx->buf);
	MTY_Free(ctx->pbuf);

	MTY_TLSDestroy(&ctx->tls);

	MTY_Free(ctx);
	*secure = NULL;
}

static bool secure_read_message(struct secure *ctx, struct tcp *tcp, uint32_t timeout, size_t *size)
{
	// TLS first 5 bytes contain message type, version, and size
	if (!mty_tcp_read(tcp, ctx->buf, 5, timeout))
		return false;

	// Size is bytes 3-4 big endian
	uint16_t len = 0;
	memcpy(&len, ctx->buf + 3, 2);
	len = MTY_SwapFromBE16(len);

	// Grow buf to handle message
	*size = len + 5;
	if (ctx->buf_size < *size) {
		ctx->buf_size = *size;
		ctx->buf = MTY_Realloc(ctx->buf, ctx->buf_size, 1);
	}

	return mty_tcp_read(tcp, ctx->buf + 5, len, timeout);
}

static bool secure_write_callback(const void *buf, size_t size, void *opaque)
{
	return mty_tcp_write((struct tcp *) opaque, buf, size);
}

struct secure *mty_secure_connect(struct tcp *tcp, const char *host, uint32_t timeout)
{
	bool r = true;

	struct secure *ctx = MTY_Alloc(1, sizeof(struct secure));

	ctx->buf = MTY_Alloc(SECURE_PADDING, 1);
	ctx->buf_size = SECURE_PADDING;

	// TLS context
	ctx->tls = MTY_TLSCreate(MTY_TLS_PROTOCOL_TLS, NULL, host, NULL, 0);
	if (!ctx->tls) {
		r = false;
		goto except;
	}

	// Handshake part 1 (->Client Hello) -- Initiate with NULL message
	MTY_Async a = MTY_TLSHandshake(ctx->tls, NULL, 0, secure_write_callback, tcp);
	if (a != MTY_ASYNC_CONTINUE) {
		r = false;
		goto except;
	}

	// Handshake part 2 (<-Server Hello, ...) -- Loop until handshake complete
	while (a == MTY_ASYNC_CONTINUE) {
		size_t size = 0;
		r = secure_read_message(ctx, tcp, timeout, &size);
		if (!r)
			break;

		a = MTY_TLSHandshake(ctx->tls, ctx->buf, size, secure_write_callback, tcp);
		if (a == MTY_ASYNC_ERROR)
			r = false;
	}

	except:

	if (!r)
		mty_secure_destroy(&ctx);

	return ctx;
}

bool mty_secure_write(struct secure *ctx, struct tcp *tcp, const void *buf, size_t size)
{
	// Output buffer will be slightly larger than input
	if (ctx->buf_size < size + SECURE_PADDING) {
		ctx->buf_size = size + SECURE_PADDING;
		ctx->buf = MTY_Realloc(ctx->buf, ctx->buf_size, 1);
	}

	// Encrypt
	size_t written = 0;
	if (!MTY_TLSEncrypt(ctx->tls, buf, size, ctx->buf, ctx->buf_size, &written))
		return false;

	// Write resulting encrypted message via TCP
	return mty_tcp_write(tcp, ctx->buf, written);
}

bool mty_secure_read(struct secure *ctx, struct tcp *tcp, void *buf, size_t size, uint32_t timeout)
{
	while (ctx->pending < size) {
		// We need more data, read a TLS message from the socket
		size_t msize = 0;
		if (!secure_read_message(ctx, tcp, timeout, &msize))
			return false;

		// Resize the pending buffer
		if (ctx->pbuf_size < ctx->pending + SECURE_PADDING) {
			ctx->pbuf_size = ctx->pending + SECURE_PADDING;
			ctx->pbuf = MTY_Realloc(ctx->pbuf, ctx->pbuf_size, 1);
		}

		// Decrypt
		size_t read = 0;
		if (!MTY_TLSDecrypt(ctx->tls, ctx->buf, msize, ctx->pbuf + ctx->pending,
			ctx->pbuf_size - ctx->pending, &read))
			return false;

		ctx->pending += read;
	}

	// We have enough data to fill the buffer
	memcpy(buf, ctx->pbuf, size);
	ctx->pending -= size;

	memmove(ctx->pbuf, ctx->pbuf + size, ctx->pending);

	return true;
}
