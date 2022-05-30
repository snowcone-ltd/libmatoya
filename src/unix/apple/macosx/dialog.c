// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <CoreFoundation/CoreFoundation.h>

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

	CFStringRef nstitle = CFStringCreateWithCString(NULL, title, kCFStringEncodingUTF8);
	CFStringRef nsmsg = CFStringCreateWithCString(NULL, msg, kCFStringEncodingUTF8);

	CFOptionFlags r = 0;
	CFUserNotificationDisplayAlert(0, kCFUserNotificationNoteAlertLevel, NULL,
		NULL, NULL, nstitle, nsmsg, NULL, NULL, NULL, &r);

	CFRelease(nstitle);
	CFRelease(nsmsg);
	MTY_Free(msg);
}

const char *MTY_OpenFile(const char *title, MTY_App *app, MTY_Window window)
{
	return NULL;
}
