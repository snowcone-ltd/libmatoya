// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <string.h>
#include <errno.h>

#include "fsutil.h"
#include "tlocal.h"

void *MTY_ReadFile(const char *path, size_t *size)
{
	size_t tmp = 0;
	if (!size)
		size = &tmp;

	*size = fsutil_size(path);
	if (*size == 0 || *size > MTY_FILE_MAX)
		return NULL;

	FILE *f = fsutil_open(path, "rb");
	if (!f)
		return NULL;

	void *buf = MTY_Alloc(*size + 1, 1);

	if (fread(buf, 1, *size, f) != *size) {
		MTY_Log("'fread' failed with ferror %d", ferror(f));
		MTY_Free(buf);
		buf = NULL;
		*size = 0;
	}

	fclose(f);

	return buf;
}

bool MTY_WriteFile(const char *path, const void *buf, size_t size)
{
	FILE *f = fsutil_open(path, "wb");
	if (!f)
		return false;

	bool r = true;
	if (fwrite(buf, 1, size, f) != size) {
		MTY_Log("'fwrite' failed with ferror %d", ferror(f));
		r = false;
	}

	fclose(f);

	return r;
}

static bool file_vfprintf(const char *path, const char *mode, const char *fmt, va_list args)
{
	FILE *f = fsutil_open(path, mode);
	if (!f)
		return false;

	bool r = true;
	if (vfprintf(f, fmt, args) < (int32_t) strlen(fmt)) {
		MTY_Log("'vfprintf' failed with ferror %d", ferror(f));
		r = false;
	}

	fclose(f);

	return r;
}

bool MTY_WriteTextFile(const char *path, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	bool r = file_vfprintf(path, "w", fmt, args);

	va_end(args);

	return r;
}

bool MTY_AppendTextToFile(const char *path, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	bool r = file_vfprintf(path, "a", fmt, args);

	va_end(args);

	return r;
}

void MTY_FreeFileList(MTY_FileList **fileList)
{
	if (!fileList || !*fileList)
		return;

	for (uint32_t x = 0; x < (*fileList)->len; x++) {
		MTY_Free((*fileList)->files[x].name);
		MTY_Free((*fileList)->files[x].path);
	}

	MTY_Free((*fileList)->files);

	MTY_Free(*fileList);
	*fileList = NULL;
}

const char *MTY_JoinPath(const char *path0, const char *path1)
{
	char *path = MTY_SprintfD("%s%c%s", path0, FSUTIL_DELIM, path1);
	char *local = mty_tlocal_strcpy(path);

	MTY_Free(path);

	return local;
}

const char *MTY_GetFileName(const char *path, bool extension)
{
	const char *name = strrchr(path, FSUTIL_DELIM);
	name = name ? name + 1 : path;

	char *local = mty_tlocal_strcpy(name);

	if (!extension) {
		char *ext = strrchr(local, '.');

		if (ext)
			*ext = '\0';
	}

	return local;
}

const char *MTY_GetFileExtension(const char *path)
{
	const char *ext = strrchr(path, '.');

	if (ext)
		return mty_tlocal_strcpy(ext + 1);

	return "";
}

const char *MTY_GetPathPrefix(const char *path)
{
	char *local = mty_tlocal_strcpy(path);
	char *name = strrchr(local, FSUTIL_DELIM);

	if (name)
		*name = '\0';

	return local;
}
