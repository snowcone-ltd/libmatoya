// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <stdio.h>

#include <windows.h>
#include <shlwapi.h>
#include <shlobj_core.h>

#include "tlocal.h"

bool MTY_DeleteFile(const char *path)
{
	wchar_t *wpath = MTY_MultiToWideD(path);

	BOOL ok = DeleteFile(wpath);
	MTY_Free(wpath);

	if (!ok) {
		MTY_Log("'DeleteFile' failed with error 0x%X", GetLastError());
		return false;
	}

	return true;
}

bool MTY_FileExists(const char *path)
{
	wchar_t *wpath = MTY_MultiToWideD(path);

	BOOL ok = PathFileExists(wpath);
	MTY_Free(wpath);

	if (!ok) {
		DWORD e = GetLastError();
		if (e != ERROR_PATH_NOT_FOUND && e != ERROR_FILE_NOT_FOUND)
			MTY_Log("'PathFileExists' failed with error 0x%X", e);
	}

	return ok;
}

bool MTY_Mkdir(const char *path)
{
	wchar_t *wpath = MTY_MultiToWideD(path);

	int32_t e = SHCreateDirectory(NULL, wpath);
	MTY_Free(wpath);

	if (e == ERROR_ALREADY_EXISTS) {
		return false;

	} else if (e != ERROR_SUCCESS) {
		MTY_Log("'SHCreateDirectory' failed with error %d", e);
		return false;
	}

	return true;
}

bool MTY_CopyFile(const char *src, const char *dst)
{
	bool r = true;
	wchar_t *srcw = MTY_MultiToWideD(src);
	wchar_t *dstw = MTY_MultiToWideD(dst);

	if (!CopyFile(srcw, dstw, FALSE)) {
		MTY_Log("'CopyFile' failed with error 0x%X", GetLastError());
		r = false;
	}

	MTY_Free(srcw);
	MTY_Free(dstw);

	return r;
}

bool MTY_MoveFile(const char *src, const char *dst)
{
	wchar_t *srcw = MTY_MultiToWideD(src);
	wchar_t *dstw = MTY_MultiToWideD(dst);

	BOOL r = MoveFileEx(srcw, dstw, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
	if (!r) {
		// MoveFileEx may fail across system volumes or when directory locks are present, fall back to CopyFile
		r = CopyFile(srcw, dstw, FALSE);

		if (r) {
			DeleteFile(srcw);

		} else {
			MTY_Log("Failed to move file via 'MoveFileEx' and 'CopyFile', last error 0x%X", GetLastError());
		}
	}

	MTY_Free(srcw);
	MTY_Free(dstw);

	return r;
}

static const char *file_known_folder(const KNOWNFOLDERID *fid)
{
	WCHAR *dirw = NULL;
	const char *local = NULL;

	HRESULT e = SHGetKnownFolderPath(fid, 0, NULL, &dirw);

	if (e == S_OK) {
		local = MTY_WideToMultiDL(dirw);
		CoTaskMemFree(dirw);

	} else {
		MTY_Log("'SHGetKnownFolderPath' failed with HRESULT 0x%X", e);
	}

	return local;
}

const char *MTY_GetDir(MTY_Dir dir)
{
	wchar_t tmp[MTY_PATH_MAX] = {0};
	const char *local = NULL;

	switch (dir) {
		case MTY_DIR_CWD: {
			DWORD n = GetCurrentDirectory(MTY_PATH_MAX, tmp);
			if (n > 0) {
				local = MTY_WideToMultiDL(tmp);

			} else {
				MTY_Log("'GetCurrentDirectory' failed with error 0x%X", GetLastError());
			}

			break;
		}
		case MTY_DIR_HOME:
			local = file_known_folder(&FOLDERID_RoamingAppData);
			break;
		case MTY_DIR_GLOBAL_HOME:
			local = file_known_folder(&FOLDERID_ProgramData);
			break;
		case MTY_DIR_PROGRAMS:
			local = file_known_folder(&FOLDERID_ProgramFiles);
			break;
	}

	return local ? local : ".";
}

MTY_LockFile *MTY_LockFileCreate(const char *path, MTY_FileMode mode)
{
	DWORD share = FILE_SHARE_READ;
	DWORD access = GENERIC_READ;
	DWORD create = OPEN_EXISTING;

	if (mode == MTY_FILE_MODE_EXCLUSIVE) {
		share = 0;
		access = GENERIC_WRITE;
		create = CREATE_ALWAYS;
	}

	wchar_t *pathw = MTY_MultiToWideD(path);
	HANDLE lock = CreateFile(pathw, access, share, NULL, create, FILE_ATTRIBUTE_NORMAL, NULL);
	MTY_Free(pathw);

	if (lock == INVALID_HANDLE_VALUE) {
		MTY_Log("'CreateFile' failed with error 0x%X", GetLastError());
		return NULL;
	}

	return (MTY_LockFile *) lock;
}

void MTY_LockFileDestroy(MTY_LockFile **lockFile)
{
	if (!lockFile || !*lockFile)
		return;

	MTY_LockFile *ctx = *lockFile;

	if (!CloseHandle((HANDLE) ctx))
		MTY_Log("'CloseHandle' failed with error 0x%X", GetLastError());

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
		wchar_t *name1 = MTY_MultiToWideD(fi1->name);
		wchar_t *name2 = MTY_MultiToWideD(fi2->name);

		int32_t r = _wcsicmp(name1, name2);

		if (r == 0)
			r = -wcscmp(name1, name2);

		MTY_Free(name1);
		MTY_Free(name2);

		return r;
	}
}

MTY_FileList *MTY_GetFileList(const char *path, const char *filter)
{
	MTY_FileList *fl = MTY_Alloc(1, sizeof(MTY_FileList));
	char *pathd = MTY_Strdup(path);

	WIN32_FIND_DATA ent;
	const wchar_t *pathw = MTY_MultiToWideDL(MTY_JoinPath(pathd, "*"));

	HANDLE dir = FindFirstFile(pathw, &ent);
	bool ok = dir != INVALID_HANDLE_VALUE;

	wchar_t *filterw = filter ? MTY_MultiToWideD(filter) : NULL;

	while (ok) {
		wchar_t *namew = ent.cFileName;
		bool is_dir = ent.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;

		if (is_dir || wcsstr(namew, filterw ? filterw : L"")) {
			fl->files = MTY_Realloc(fl->files, fl->len + 1, sizeof(MTY_FileDesc));

			char *name = MTY_WideToMultiD(namew);
			fl->files[fl->len].name = name;
			fl->files[fl->len].path = MTY_Strdup(MTY_JoinPath(pathd, name));
			fl->files[fl->len].dir = is_dir;
			fl->len++;
		}

		ok = FindNextFile(dir, &ent);

		if (!ok)
			FindClose(dir);
	}

	MTY_Free(filterw);
	MTY_Free(pathd);

	if (fl->len > 0)
		MTY_Sort(fl->files, fl->len, sizeof(MTY_FileDesc), file_compare);

	return fl;
}
