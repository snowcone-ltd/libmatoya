// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#define _DEFAULT_SOURCE  // DT_DIR
#define _DARWIN_C_SOURCE // flock, DT_DIR

#include "matoya.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <dirent.h>

#include "home.h"
#include "tlocal.h"

bool MTY_DeleteFile(const char *path)
{
	if (remove(path) != 0) {
		MTY_Log("'remove' failed with errno %d", errno);
		return false;
	}

	return true;
}

bool MTY_FileExists(const char *path)
{
	return access(path, F_OK) != -1;
}

bool MTY_Mkdir(const char *path)
{
	char *tmp = MTY_Strdup(path);
	size_t len = strlen(tmp);

	while (len > 0 && tmp[len - 1] == '/') {
		tmp[len - 1] = '\0';
		len--;
	}

	for (size_t x = 1; x < len; x++) {
		if (tmp[x] == '/') {
			tmp[x] = '\0';
			if (mkdir(tmp, S_IRWXU) == -1 && errno != EEXIST)
				MTY_Log("'mkdir' failed with errno %d", errno);
			tmp[x] = '/';
		}
	}

	if (mkdir(tmp, S_IRWXU) == -1 && errno != EEXIST)
		MTY_Log("'mkdir' failed with errno %d", errno);

	MTY_Free(tmp);

	return true;
}

bool MTY_CopyFile(const char *src, const char *dst)
{
	bool r = false;
	size_t size = 0;
	void *data = MTY_ReadFile(src, &size);

	if (data) {
		r = MTY_WriteFile(dst, data, size);
		MTY_Free(data);
	}

	return r;
}

bool MTY_MoveFile(const char *src, const char *dst)
{
	if (rename(src, dst) != 0) {
		MTY_Log("'rename' failed with errno %d", errno);
		return false;
	}

	return true;
}

const char *MTY_GetDir(MTY_Dir dir)
{
	char tmp[MTY_PATH_MAX] = {0};
	char *local = NULL;

	switch (dir) {
		case MTY_DIR_CWD: {
			if (getcwd(tmp, MTY_PATH_MAX)) {
				local = mty_tlocal_strcpy(tmp);

			} else {
				MTY_Log("'getcwd' failed with errno %d", errno);
			}

			break;
		}
		case MTY_DIR_GLOBAL_HOME:
		case MTY_DIR_HOME:
			if (home_get_dir(tmp, MTY_PATH_MAX))
				local = mty_tlocal_strcpy(tmp);

			break;
		case MTY_DIR_PROGRAMS:
			return "/user/bin";
	}

	return local ? local : ".";
}

MTY_LockFile *MTY_LockFileCreate(const char *path, MTY_FileMode mode)
{
	int32_t open_flags = 0;
	int32_t flock_flags = LOCK_SH;

	if (mode == MTY_FILE_MODE_EXCLUSIVE) {
		open_flags = O_CREAT;
		flock_flags = LOCK_EX;
	}

	int32_t f = open(path, O_RDWR | open_flags, S_IRWXU | S_IRWXG | S_IRWXO);

	if (f != -1) {
		if (flock(f, flock_flags | LOCK_NB) == 0) {
			return MTY_Dup(&f, 4);

		} else {
			MTY_Log("'flock' failed with errno %d", errno);

			if (close(f) != 0)
				MTY_Log("'close' failed with errno %d", errno);
		}
	} else {
		MTY_Log("'open' failed with errno %d", errno);
	}

	return NULL;
}

void MTY_LockFileDestroy(MTY_LockFile **lockFile)
{
	if (!lockFile || !*lockFile)
		return;

	int32_t *f = (int32_t *) *lockFile;

	if (flock(*f, LOCK_UN) != 0)
		MTY_Log("'flock' failed with errno %d", errno);

	if (close(*f) != 0)
		MTY_Log("'close' failed with errno %d", errno);

	MTY_Free(f);
	*lockFile = NULL;
}

static int32_t file_compare(const void *p1, const void *p2)
{
	MTY_FileDesc *fi1 = (MTY_FileDesc *) p1;
	MTY_FileDesc *fi2 = (MTY_FileDesc *) p2;

	if (fi1->dir && !fi2->dir) {
		return -1;

	} else if (!fi1->dir && fi2->dir) {
		return 1;

	} else {
		int32_t r = strcasecmp(fi1->name, fi2->name);

		if (r != 0)
			return r;

		return -strcmp(fi1->name, fi2->name);
	}
}

MTY_FileList *MTY_GetFileList(const char *path, const char *filter)
{
	MTY_FileList *fl = MTY_Alloc(1, sizeof(MTY_FileList));
	char *pathd = MTY_Strdup(path);

	bool ok = false;

	struct dirent *ent = NULL;
	DIR *dir = opendir(pathd);
	if (dir) {
		ent = readdir(dir);
		ok = ent;
	}

	while (ok) {
		char *name = ent->d_name;
		bool is_dir = ent->d_type == DT_DIR;

		if (is_dir || strstr(name, filter ? filter : "")) {
			fl->files = MTY_Realloc(fl->files, fl->len + 1, sizeof(MTY_FileDesc));

			fl->files[fl->len].dir = is_dir;
			fl->files[fl->len].name = MTY_Strdup(name);
			fl->files[fl->len].path = MTY_Strdup(MTY_JoinPath(pathd, name));
			fl->len++;
		}

		ent = readdir(dir);
		if (!ent) {
			closedir(dir);
			ok = false;
		}
	}

	MTY_Free(pathd);

	if (fl->len > 0)
		MTY_Sort(fl->files, fl->len, sizeof(MTY_FileDesc), file_compare);

	return fl;
}
