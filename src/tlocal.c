// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"
#include "tlocal.h"

#include <string.h>
#include <stdio.h>

#define TLOCAL_MAX (8 * 1024)

static TLOCAL uint8_t *TLOCAL_MEM;
static TLOCAL size_t *TLOCAL_OFFSET;
static TLOCAL size_t TLOCAL_SIZE;

static TLOCAL uint8_t TLOCAL_MEM_INTERNAL[TLOCAL_MAX];
static TLOCAL size_t TLOCAL_OFFSET_INTERNAL;
static TLOCAL size_t TLOCAL_OFFSET_EXTERNAL;

static void tlocal_check_mem(void)
{
	if (!TLOCAL_MEM) {
		TLOCAL_MEM = TLOCAL_MEM_INTERNAL;
		TLOCAL_OFFSET = &TLOCAL_OFFSET_INTERNAL;
		TLOCAL_SIZE = TLOCAL_MAX;
	}
}

void *mty_tlocal(size_t size)
{
	tlocal_check_mem();

	if (size > TLOCAL_SIZE)
		MTY_LogFatal("Thread local storage heap overflow");

	if (*TLOCAL_OFFSET + size > TLOCAL_SIZE)
		*TLOCAL_OFFSET = 0;

	void *ptr = TLOCAL_MEM + *TLOCAL_OFFSET;
	memset(ptr, 0, size);

	*TLOCAL_OFFSET += size;

	return ptr;
}

char *mty_tlocal_strcpy(const char *str)
{
	tlocal_check_mem();

	size_t len = strlen(str) + 1;

	if (len > TLOCAL_SIZE)
		len = TLOCAL_SIZE;

	char *local = mty_tlocal(len);
	snprintf(local, len, "%s", str);

	return local;
}

void mty_tlocal_set_mem(void *buf, size_t size)
{
	TLOCAL_MEM = buf;
	TLOCAL_SIZE = size;

	if (buf) {
		TLOCAL_OFFSET = &TLOCAL_OFFSET_EXTERNAL;
		*TLOCAL_OFFSET = 0;
	}
}
