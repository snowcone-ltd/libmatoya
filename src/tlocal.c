// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"
#include "tlocal.h"

#include <string.h>
#include <stdio.h>

#define TLOCAL_MAX (8 * 1024)

static TLOCAL uint8_t TLOCAL_HEAP[TLOCAL_MAX];
static TLOCAL size_t TLOCAL_OFFSET;

void *mty_tlocal(size_t size)
{
	if (size > TLOCAL_MAX)
		MTY_LogFatal("Thread local storage heap overflow");

	if (TLOCAL_OFFSET + size > TLOCAL_MAX)
		TLOCAL_OFFSET = 0;

	void *ptr = TLOCAL_HEAP + TLOCAL_OFFSET;
	memset(ptr, 0, size);

	TLOCAL_OFFSET += size;

	return ptr;
}

char *mty_tlocal_strcpy(const char *str)
{
	size_t len = strlen(str) + 1;

	if (len > TLOCAL_MAX)
		len = TLOCAL_MAX;

	char *local = mty_tlocal(len);
	snprintf(local, len, "%s", str);

	return local;
}
