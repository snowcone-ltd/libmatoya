// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <stdlib.h>
#include <string.h>

struct async_state {
	MTY_Async status;
	uint32_t timeout;
	bool image;

	struct {
		char *url;
		char *method;
		char *headers;
		void *body;
		size_t body_size;
		char *proxy;
	} req;

	struct {
		uint16_t code;
		void *body;
		size_t body_size;
	} res;
};

static MTY_Atomic32 ASYNC_GLOCK;
static MTY_ThreadPool *ASYNC_CTX;

static void http_async_free_state(void *opaque)
{
	struct async_state *s = opaque;

	if (s) {
		if (s->req.url)
			MTY_SecureFree(s->req.url, strlen(s->req.url));

		if (s->req.headers)
			MTY_SecureFree(s->req.headers, strlen(s->req.headers));

		if (s->req.body && s->req.body_size > 0)
			MTY_SecureFree(s->req.body, s->req.body_size);

		MTY_Free(s->req.method);
		MTY_Free(s->res.body);
		MTY_Free(s);
	}
}

void MTY_HttpAsyncCreate(uint32_t maxThreads)
{
	MTY_GlobalLock(&ASYNC_GLOCK);

	if (!ASYNC_CTX)
		ASYNC_CTX = MTY_ThreadPoolCreate(maxThreads);

	MTY_GlobalUnlock(&ASYNC_GLOCK);
}

void MTY_HttpAsyncDestroy(void)
{
	MTY_GlobalLock(&ASYNC_GLOCK);

	MTY_ThreadPoolDestroy(&ASYNC_CTX, http_async_free_state);

	MTY_GlobalUnlock(&ASYNC_GLOCK);
}

static void http_async_thread(void *opaque)
{
	struct async_state *s = opaque;

	bool req_ok = MTY_HttpRequest(s->req.url, s->req.method, s->req.headers,
		s->req.body, s->req.body_size, s->req.proxy, s->timeout,
		&s->res.body, &s->res.body_size, &s->res.code);

	bool res_ok = s->res.code >= 200 && s->res.code < 300;

	if (s->image && req_ok && res_ok && s->res.body && s->res.body_size > 0) {
		uint32_t w = 0;
		uint32_t h = 0;
		void *image = MTY_DecompressImage(s->res.body, s->res.body_size, &w, &h);

		MTY_Free(s->res.body);
		s->res.body = image;
		s->res.body_size = w | h << 16;
	}

	s->status = !req_ok ? MTY_ASYNC_ERROR : MTY_ASYNC_OK;
}

void MTY_HttpAsyncRequest(uint32_t *index, const char *url, const char *method, const char *headers,
	const void *body, size_t bodySize, const char *proxy, uint32_t timeout, bool image)
{
	if (!ASYNC_CTX)
		return;

	if (*index != 0)
		MTY_ThreadPoolDetach(ASYNC_CTX, *index, http_async_free_state);

	struct async_state *s = MTY_Alloc(1, sizeof(struct async_state));
	s->timeout = timeout;
	s->image = image;

	s->req.url = MTY_Strdup(url);
	s->req.method = MTY_Strdup(method);
	s->req.headers = headers ? MTY_Strdup(headers) : MTY_Alloc(1, 1);
	s->req.body_size = bodySize;
	s->req.body = body ? MTY_Dup(body, bodySize) : NULL;
	s->req.proxy = proxy ? MTY_Strdup(proxy) : NULL;

	*index = MTY_ThreadPoolDispatch(ASYNC_CTX, http_async_thread, s);

	if (*index == 0) {
		MTY_Log("Failed to start %s", url);
		http_async_free_state(s);
	}
}

MTY_Async MTY_HttpAsyncPoll(uint32_t index, void **response, size_t *size, uint16_t *status)
{
	if (!ASYNC_CTX)
		return MTY_ASYNC_ERROR;

	if (index == 0)
		return MTY_ASYNC_DONE;

	struct async_state *s = NULL;
	MTY_Async r = MTY_ASYNC_DONE;
	MTY_Async pstatus = MTY_ThreadPoolPoll(ASYNC_CTX, index, (void **) &s);

	if (pstatus == MTY_ASYNC_OK) {
		*response = s->res.body;
		*size = s->res.body_size;
		*status = s->res.code;

		r = s->status;

	} else if (pstatus == MTY_ASYNC_CONTINUE) {
		r = MTY_ASYNC_CONTINUE;
	}

	return r;
}

void MTY_HttpAsyncClear(uint32_t *index)
{
	if (!ASYNC_CTX)
		return;

	MTY_ThreadPoolDetach(ASYNC_CTX, *index, http_async_free_state);
	*index = 0;
}
