// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <string.h>

enum {
	QUEUE_EMPTY = 0,
	QUEUE_FULL  = 1,
};

struct queue_slot {
	void *data;
	size_t size;
	bool ptr;
	MTY_Atomic32 state;
};

struct MTY_Queue {
	size_t buf_size;
	uint32_t len;

	MTY_Waitable *pop_sync;
	MTY_Mutex *push_mutex;

	struct queue_slot *slots;
	uint32_t push_pos;
	uint32_t pop_pos;
};

MTY_Queue *MTY_QueueCreate(uint32_t len, size_t bufSize)
{
	MTY_Queue *ctx = MTY_Alloc(1, sizeof(MTY_Queue));
	ctx->len = len;
	ctx->buf_size = bufSize;

	if (ctx->buf_size < sizeof(void *))
		ctx->buf_size = sizeof(void *);

	ctx->pop_sync = MTY_WaitableCreate();
	ctx->push_mutex = MTY_MutexCreate();

	ctx->slots = MTY_Alloc(ctx->len, sizeof(struct queue_slot));

	for (uint32_t x = 0; x < ctx->len; x++)
		ctx->slots[x].data = MTY_Alloc(ctx->buf_size, 1);

	return ctx;
}

void MTY_QueueDestroy(MTY_Queue **queue)
{
	if (!queue || !*queue)
		return;

	MTY_Queue *ctx = *queue;

	for (uint32_t x = 0; x < ctx->len; x++)
		MTY_Free(ctx->slots[x].data);

	MTY_Free(ctx->slots);

	MTY_MutexDestroy(&ctx->push_mutex);
	MTY_WaitableDestroy(&ctx->pop_sync);

	MTY_Free(ctx);
	*queue = NULL;
}

uint32_t MTY_QueueGetLength(MTY_Queue *ctx)
{
	int32_t pos = (int32_t) ctx->push_pos - (int32_t) ctx->pop_pos;

	if (pos < 0)
		pos += ctx->len;

	return (uint32_t) pos;
}

void *MTY_QueueGetInputBuffer(MTY_Queue *ctx)
{
	MTY_MutexLock(ctx->push_mutex);

	int32_t state = MTY_Atomic32Get(&ctx->slots[ctx->push_pos].state);

	if (state == QUEUE_EMPTY) {
		return ctx->slots[ctx->push_pos].data;

	} else {
		MTY_MutexUnlock(ctx->push_mutex);
	}

	return NULL;
}

static uint32_t queue_next_pos(MTY_Queue *ctx, uint32_t pos)
{
	if (++pos == ctx->len)
		pos = 0;

	return pos;
}

static void queue_push(MTY_Queue *ctx, size_t size, bool ptr)
{
	if (size > 0) {
		uint32_t lock_pos = ctx->push_pos;
		ctx->slots[lock_pos].size = size;

		ctx->push_pos = queue_next_pos(ctx, ctx->push_pos);

		ctx->slots[lock_pos].ptr = ptr;
		MTY_Atomic32Set(&ctx->slots[lock_pos].state, QUEUE_FULL);

		MTY_WaitableSignal(ctx->pop_sync);
	}

	MTY_MutexUnlock(ctx->push_mutex);
}

void MTY_QueuePush(MTY_Queue *ctx, size_t size)
{
	queue_push(ctx, size, false);
}

static bool queue_pop(MTY_Queue *ctx, int32_t timeout, bool last, void **buffer, size_t *size)
{
	begin:

	if (MTY_Atomic32Get(&ctx->slots[ctx->pop_pos].state) == QUEUE_FULL) {
		*buffer = ctx->slots[ctx->pop_pos].data;

		if (size)
			*size = ctx->slots[ctx->pop_pos].size;

		if (last) {
			uint32_t next_pos = queue_next_pos(ctx, ctx->pop_pos);

			if (MTY_Atomic32Get(&ctx->slots[next_pos].state) == QUEUE_FULL) {
				MTY_QueuePop(ctx);
				goto begin;
			}
		}

		return true;

	} else if (timeout != 0) {
		// Because of the lock free check, this may already be signaled when
		// there is no data. Worst case the loop spins one extra time
		if (MTY_WaitableWait(ctx->pop_sync, timeout))
			goto begin;
	}

	return false;
}

bool MTY_QueueGetOutputBuffer(MTY_Queue *ctx, int32_t timeout, void **buffer, size_t *size)
{
	return queue_pop(ctx, timeout, false, buffer, size);
}

bool MTY_QueueGetLastOutputBuffer(MTY_Queue *ctx, int32_t timeout, void **buffer, size_t *size)
{
	return queue_pop(ctx, timeout, true, buffer, size);
}

void MTY_QueuePop(MTY_Queue *ctx)
{
	uint32_t lock_pos = ctx->pop_pos;

	ctx->pop_pos = queue_next_pos(ctx, ctx->pop_pos);

	MTY_Atomic32Set(&ctx->slots[lock_pos].state, QUEUE_EMPTY);
}

bool MTY_QueuePushPtr(MTY_Queue *ctx, void *opaque, size_t size)
{
	void *buffer = MTY_QueueGetInputBuffer(ctx);

	if (buffer) {
		memcpy(buffer, &opaque, sizeof(void *));
		queue_push(ctx, size, true);

		return true;
	}

	return false;
}

bool MTY_QueuePopPtr(MTY_Queue *ctx, int32_t timeout, void **opaque, size_t *size)
{
	void *buffer = NULL;

	if (queue_pop(ctx, timeout, false, &buffer, size)) {
		memcpy(opaque, buffer, sizeof(void *));
		MTY_QueuePop(ctx);

		return true;
	}

	return false;
}

void MTY_QueueFlush(MTY_Queue *ctx, MTY_FreeFunc freeFunc)
{
	for (void *data = NULL; queue_pop(ctx, 0, false, (void **) &data, NULL);) {
		struct queue_slot *slot = &ctx->slots[ctx->pop_pos];

		if (freeFunc && slot->ptr) {
			void *ptr = NULL;
			memcpy(&ptr, slot->data, sizeof(void *));
			freeFunc(ptr);
		}

		MTY_QueuePop(ctx);
	}
}
