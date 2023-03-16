// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "net.h"
#include "http.h"

enum {
	WS_OPCODE_CONTINUE = 0x0,
	WS_OPCODE_TEXT     = 0x1,
	WS_OPCODE_BINARY   = 0x2,
	WS_OPCODE_CLOSE    = 0x8,
	WS_OPCODE_PING     = 0x9,
	WS_OPCODE_PONG     = 0xA,
};

struct MTY_WebSocket {
	struct net *net;
	bool connected;

	MTY_Time last_ping;
	MTY_Time last_pong;
	uint16_t close_code;

	uint8_t *buf;
	size_t size;
};

#define WS_HEADER_SIZE   14
#define WS_PING_INTERVAL 60000.0f
#define WS_PONG_TO       (WS_PING_INTERVAL * 3.0f)
#define WS_MAGIC         "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"


// HTTP

struct http_pair {
	char *key;
	char *val;
};

struct http_header {
	char *first_line;
	struct http_pair *pairs;
	uint32_t npairs;
};

#define HTTP_HEADER_MAX (16 * 1024)

static struct http_header *http_parse_header(const char *header)
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

static void http_header_destroy(struct http_header **header)
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

static bool http_get_status_code(struct http_header *h, uint16_t *status_code)
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

static bool http_get_header_str(struct http_header *h, const char *key, const char **val)
{
	for (uint32_t x = 0; x < h->npairs; x++) {
		if (!MTY_Strcasecmp(key, h->pairs[x].key)) {
			*val = h->pairs[x].val;
			return true;
		}
	}

	return false;
}

static void http_set_header_str(char **header, const char *name, const char *val)
{
	size_t len = *header ? strlen(*header) : 0;
	size_t new_len = len + strlen(name) + strlen(val) + 32;

	*header = MTY_Realloc(*header, new_len, 1);
	snprintf(*header + len, new_len, "%s: %s\r\n", name, val);
}

static struct http_header *http_read_header(struct net *net, uint32_t timeout)
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
		return NULL;
	}

	struct http_header *hdr = http_parse_header(h);
	MTY_Free(h);

	return hdr;
}

static bool http_write_request_header(struct net *net, const char *method, const char *host,
	const char *path, const char *headers)
{
	if (!headers)
		headers = "";

	char *hstr = MTY_SprintfD("%s %s HTTP/1.1\r\nHost: %s\r\n%s\r\n", method, path, host, headers);
	bool r = mty_net_write(net, hstr, strlen(hstr));

	MTY_Free(hstr);

	return r;
}


// Helpers

static void ws_create_accept_key(const char *key, char *akey, size_t size)
{
	char concat[16 * 2 + 36 + 1];
	snprintf(concat, 16 * 2 + 36 + 1, "%s%s", key, WS_MAGIC);

	uint8_t sha1[MTY_SHA1_SIZE];
	MTY_CryptoHash(MTY_ALGORITHM_SHA1, concat, strlen(concat), NULL, 0, sha1, MTY_SHA1_SIZE);

	MTY_BytesToBase64(sha1, MTY_SHA1_SIZE, akey, size);
}

static void ws_mask(const uint8_t *in, size_t size, const uint8_t *mask, uint8_t *out)
{
	for (size_t x = 0; x < size; x++)
		out[x] = in[x] ^ mask[x % 4];
}


// Connect, accept

static void ws_parse_headers(const char *key, const char *val, void *opaque)
{
	http_set_header_str((char **) opaque, key, val);
}

static bool ws_connect(MTY_WebSocket *ctx, const char *host, const char *path,
	const char *headers, uint32_t timeout, uint16_t *upgrade_status)
{
	char *req = NULL;
	struct http_header *hdr = NULL;

	// Generate the random base64 key
	uint8_t key[16];
	MTY_GetRandomBytes(key, 16);

	char skey[16 * 2 + 1];
	MTY_BytesToBase64(key, 16, skey, 16 * 2 + 1);

	// Obligatory websocket headers
	http_set_header_str(&req, "Upgrade", "websocket");
	http_set_header_str(&req, "Connection", "Upgrade");
	http_set_header_str(&req, "Sec-WebSocket-Key", skey);
	http_set_header_str(&req, "Sec-WebSocket-Version", "13");

	// Optional headers
	if (headers)
		mty_http_parse_headers(headers, ws_parse_headers, &req);

	// Write http the header
	bool r = http_write_request_header(ctx->net, "GET", host, path, req);
	if (!r)
		goto except;

	// Read response headers
	hdr = http_read_header(ctx->net, timeout);
	if (!hdr) {
		r = false;
		goto except;
	}

	// We expect a 101 response code from the server
	r = http_get_status_code(hdr, upgrade_status);
	if (!r)
		goto except;

	r = *upgrade_status == 101;
	if (!r)
		goto except;

	// Validate the security key response
	const char *akey = NULL;
	r = http_get_header_str(hdr, "Sec-WebSocket-Accept", &akey);
	if (!r)
		goto except;

	char tkey[MTY_SHA1_SIZE * 2 + 1];
	ws_create_accept_key(skey, tkey, MTY_SHA1_SIZE * 2 + 1);

	if (strcmp(tkey, akey)) {
		r = false;
		goto except;
	}

	except:

	http_header_destroy(&hdr);
	MTY_Free(req);

	return r;
}


// Write, read

static bool ws_write(MTY_WebSocket *ctx, const void *buf, size_t size, uint8_t opcode)
{
	// Resize the serialized buffer if necessary
	if (size + WS_HEADER_SIZE > ctx->size) {
		ctx->size = size + WS_HEADER_SIZE;
		ctx->buf = MTY_Realloc(ctx->buf, ctx->size, 1);
	}

	// Serialize the payload into a websocket conformant message
	size_t o = 0;
	ctx->buf[o++] = 0x80 | (opcode & 0xF); // 'fin' | opcode;
	ctx->buf[o] = 0x80;                    // 'mask' | size detection

	// Payload len calculations -- can use 1, 2, or 8 bytes
	if (size < 126) {
		ctx->buf[o++] |= (uint8_t) size;

	} else if (size <= UINT16_MAX) {
		ctx->buf[o++] |= 0x7E;

		uint16_t l = MTY_SwapToBE16((uint16_t) size);
		memcpy(ctx->buf + o, &l, 2);
		o += 2;

	} else {
		ctx->buf[o++] |= 0x7F;

		uint64_t l = MTY_SwapToBE64((uint64_t) size);
		memcpy(ctx->buf + o, &l, 8);
		o += 8;
	}

	uint8_t *masking_key = ctx->buf + o;
	MTY_GetRandomBytes(masking_key, 4);
	o += 4;

	ws_mask(buf, size, masking_key, ctx->buf + o);

	// Write full network buffer
	return mty_net_write(ctx->net, ctx->buf, size + o);;
}

static bool ws_read(MTY_WebSocket *ctx, void *buf, size_t size, uint8_t *opcode, uint32_t timeout, size_t *read)
{
	// First two bytes contain most control information
	uint8_t hbuf[WS_HEADER_SIZE];
	if (!mty_net_read(ctx->net, hbuf, 2, timeout))
		return false;

	*opcode = hbuf[0] & 0xF;
	*read = hbuf[1] & 0x7F;

	bool mask = hbuf[1] & 0x80;
	uint8_t addtl = *read == 126 ? 2 : *read == 127 ? 8 : 0;
	uint8_t *masking_key = hbuf + addtl; // Masking key will be here after subsequent read

	if (mask)
		addtl += 4;

	// Read the payload size and mask
	if (!mty_net_read(ctx->net, hbuf, addtl, timeout))
		return false;

	// Payload len of < 126 uses 1 bytes, == 126 uses 2 bytes, == 127 uses 8 bytes
	if (*read == 126) {
		*read = MTY_SwapFromBE16(*((uint16_t *) hbuf));

	} else if (*read == 127) {
		*read = (size_t) MTY_SwapFromBE64(*((uint64_t *) hbuf));
	}

	// Check bounds
	if (*read > size)
		return false;

	// Read the payload
	if (!mty_net_read(ctx->net, buf, *read, timeout))
		return false;

	// Unmask the data if necessary
	if (mask)
		ws_mask(buf, *read, masking_key, buf);

	return true;
}


// Public

MTY_WebSocket *MTY_WebSocketConnect(const char *host, uint16_t port, bool secure, const char *path,
	const char *headers, uint32_t timeout, uint16_t *upgradeStatus)
{
	bool r = true;

	MTY_WebSocket *ctx = MTY_Alloc(1, sizeof(MTY_WebSocket));

	ctx->net = mty_net_connect(host, port, secure, timeout);
	if (!ctx->net) {
		r = false;
		goto except;
	}

	r = ws_connect(ctx, host, path, headers, timeout, upgradeStatus);
	if (!r)
		goto except;

	ctx->connected = true;
	ctx->last_ping = ctx->last_pong = MTY_GetTime();

	except:

	if (!r)
		MTY_WebSocketDestroy(&ctx);

	return ctx;
}

void MTY_WebSocketDestroy(MTY_WebSocket **webSocket)
{
	if (!webSocket || !*webSocket)
		return;

	MTY_WebSocket *ctx = *webSocket;

	if (ctx->connected) {
		uint16_t code_be = MTY_SwapToBE16(1000);
		ws_write(ctx, &code_be, 2, WS_OPCODE_CLOSE);
	}

	mty_net_destroy(&ctx->net);

	MTY_Free(ctx->buf);

	MTY_Free(ctx);
	*webSocket = NULL;
}

MTY_Async MTY_WebSocketRead(MTY_WebSocket *ctx, uint32_t timeout, char *msg, size_t size)
{
	// Implicit ping handler
	MTY_Time now = MTY_GetTime();

	if (MTY_TimeDiff(ctx->last_ping, now) > WS_PING_INTERVAL) {
		if (!ws_write(ctx, "ping", 4, WS_OPCODE_PING))
			return MTY_ASYNC_ERROR;

		ctx->last_ping = now;
	}

	// Poll for more data
	MTY_Async r = mty_net_poll(ctx->net, timeout);

	if (r == MTY_ASYNC_OK) {
		uint8_t opcode = WS_OPCODE_CONTINUE;
		memset(msg, 0, size);

		size_t read = 0;
		if (!ws_read(ctx, msg, size - 1, &opcode, 1000, &read))
			return MTY_ASYNC_ERROR;

		switch (opcode) {
			case WS_OPCODE_PING:
				r = ws_write(ctx, msg, read, WS_OPCODE_PONG) ? MTY_ASYNC_CONTINUE : MTY_ASYNC_ERROR;
				break;
			case WS_OPCODE_PONG:
				ctx->last_pong = MTY_GetTime();
				r = MTY_ASYNC_CONTINUE;
				break;
			case WS_OPCODE_TEXT:
				if (read == 0)
					r = MTY_ASYNC_CONTINUE;
				break;
			case WS_OPCODE_CLOSE: {
				r = MTY_ASYNC_DONE;
				memcpy(&ctx->close_code, msg, 2);
				ctx->close_code = MTY_SwapFromBE16(ctx->close_code);
				break;
			}
			default:
				r = MTY_ASYNC_CONTINUE;
				break;
		}
	}

	// If we haven't gotten a pong within WS_PONG_TO, error
	if (MTY_TimeDiff(ctx->last_pong, MTY_GetTime()) > WS_PONG_TO)
		r = MTY_ASYNC_ERROR;

	return r;
}

bool MTY_WebSocketWrite(MTY_WebSocket *ctx, const char *msg)
{
	return ws_write(ctx, msg, strlen(msg), WS_OPCODE_TEXT);
}

uint16_t MTY_WebSocketGetCloseCode(MTY_WebSocket *ctx)
{
	return ctx->close_code;
}
