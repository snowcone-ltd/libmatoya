// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <windows.h>
#include <commdlg.h>

#include "tlocal.h"

bool MTY_HasDialogs(void)
{
	return true;
}

void MTY_ShowMessageBox(const char *title, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	char *msg = MTY_VsprintfD(fmt, args);

	va_end(args);

	WCHAR *wmsg = MTY_MultiToWideD(msg);
	WCHAR *wtitle = MTY_MultiToWideD(title);

	MessageBox(NULL, wmsg, wtitle, MB_OK);

	MTY_Free(wmsg);
	MTY_Free(wtitle);
	MTY_Free(msg);
}

const char *MTY_OpenFile(void)
{
	WCHAR file[MTY_PATH_MAX * sizeof(WCHAR)];

	OPENFILENAME ofn = {0};
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.lpstrFile = file;
	ofn.nMaxFile = MTY_PATH_MAX;

	if (!GetOpenFileName(&ofn))
		return NULL;

	return MTY_WideToMultiDL(file);
}
