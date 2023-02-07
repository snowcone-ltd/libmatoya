// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <AppKit/AppKit.h>

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

	NSAlert *alert = [NSAlert new];
	[alert setMessageText:[NSString stringWithUTF8String:title]];
	[alert setInformativeText:[NSString stringWithUTF8String:msg]];
	[alert runModal];

	MTY_Free(msg);
}

const char *MTY_OpenFile(const char *title, MTY_App *app, MTY_Window window)
{
	NSOpenPanel *panel = [NSOpenPanel openPanel];
	panel.canChooseFiles = YES;

	const char *path = [panel runModal] == NSModalResponseOK ?
		MTY_SprintfDL("%s", [[[panel URL] path] UTF8String]) : NULL;

	MTY_WindowActivate(app, window, true);

	return path;
}
