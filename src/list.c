// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

struct MTY_List {
	MTY_ListNode *first;
	MTY_ListNode *last;
};

MTY_List *MTY_ListCreate(void)
{
	return MTY_Alloc(1, sizeof(MTY_List));
}

void MTY_ListDestroy(MTY_List **list, MTY_FreeFunc freeFunc)
{
	if (!list || !*list)
		return;

	MTY_List *ctx = *list;

	for (MTY_ListNode *n = ctx->first; n;) {
		MTY_ListNode *next = n->next;

		if (freeFunc)
			freeFunc(n->value);

		MTY_Free(n);
		n = next;
	}

	MTY_Free(ctx);
	*list = NULL;
}

MTY_ListNode *MTY_ListGetFirst(MTY_List *ctx)
{
	return ctx->first;
}

void MTY_ListAppend(MTY_List *ctx, void *value)
{
	MTY_ListNode *node = MTY_Alloc(1, sizeof(MTY_ListNode));
	node->value = value;

	if (!ctx->first) {
		ctx->first = ctx->last = node;

	} else {
		node->prev = ctx->last;
		ctx->last->next = node;
		ctx->last = node;
	}
}

void *MTY_ListRemove(MTY_List *ctx, MTY_ListNode *node)
{
	if (node->prev) {
		node->prev->next = node->next;

	} else {
		ctx->first = node->next;
	}

	if (node->next) {
		node->next->prev = node->prev;

	}  else {
		ctx->last = node->prev;
	}

	void *r = node->value;

	MTY_Free(node);

	return r;
}
