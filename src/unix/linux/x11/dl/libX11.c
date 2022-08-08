// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#define LIBX11_EXTERN
#include "libX11.h"

#include "sym.h"

static MTY_Atomic32 LIBX11_LOCK;
static MTY_SO *LIBGL_SO;
static MTY_SO *LIBXI_SO;
static MTY_SO *LIBXCURSOR_SO;
static MTY_SO *LIBX11_SO;
static MTY_SO *LIBXRANDR_SO;
static bool LIBX11_INIT;

static void __attribute__((destructor)) libX11_global_destroy(void)
{
	MTY_GlobalLock(&LIBX11_LOCK);

	MTY_SOUnload(&LIBGL_SO);
	MTY_SOUnload(&LIBXI_SO);
	MTY_SOUnload(&LIBXCURSOR_SO);
	MTY_SOUnload(&LIBX11_SO);
	MTY_SOUnload(&LIBXRANDR_SO);
	LIBX11_INIT = false;

	MTY_GlobalUnlock(&LIBX11_LOCK);
}

bool mty_libX11_global_init(void)
{
	MTY_GlobalLock(&LIBX11_LOCK);

	if (!LIBX11_INIT) {
		bool r = true;

		LIBX11_SO = MTY_SOLoad("libX11.so.6");
		LIBXI_SO = MTY_SOLoad("libXi.so.6");
		LIBXCURSOR_SO = MTY_SOLoad("libXcursor.so.1");
		LIBGL_SO = MTY_SOLoad("libGL.so.1");
		LIBXRANDR_SO = MTY_SOLoad("libXrandr.so.2");

		if (!LIBX11_SO || !LIBGL_SO || !LIBXI_SO || !LIBXCURSOR_SO) {
			r = false;
			goto except;
		}

		LOAD_SYM(LIBX11_SO, XOpenDisplay);
		LOAD_SYM(LIBX11_SO, XScreenOfDisplay);
		LOAD_SYM(LIBX11_SO, XDefaultScreenOfDisplay);
		LOAD_SYM(LIBX11_SO, XScreenNumberOfScreen);
		LOAD_SYM(LIBX11_SO, XCloseDisplay);
		LOAD_SYM(LIBX11_SO, XDefaultRootWindow);
		LOAD_SYM(LIBX11_SO, XRootWindowOfScreen);
		LOAD_SYM(LIBX11_SO, XCreateColormap);
		LOAD_SYM(LIBX11_SO, XCreateWindow);
		LOAD_SYM(LIBX11_SO, XWithdrawWindow);
		LOAD_SYM(LIBX11_SO, XMapRaised);
		LOAD_SYM(LIBX11_SO, XSetInputFocus);
		LOAD_SYM(LIBX11_SO, XStoreName);
		LOAD_SYM(LIBX11_SO, XGetWindowAttributes);
		LOAD_SYM(LIBX11_SO, XTranslateCoordinates);
		LOAD_SYM(LIBX11_SO, XLookupKeysym);
		LOAD_SYM(LIBX11_SO, XSetWMProtocols);
		LOAD_SYM(LIBX11_SO, XInternAtom);
		LOAD_SYM(LIBX11_SO, XNextEvent);
		LOAD_SYM(LIBX11_SO, XEventsQueued);
		LOAD_SYM(LIBX11_SO, XMoveWindow);
		LOAD_SYM(LIBX11_SO, XMoveResizeWindow);
		LOAD_SYM(LIBX11_SO, XChangeProperty);
		LOAD_SYM(LIBX11_SO, XGetInputFocus);
		LOAD_SYM(LIBX11_SO, XGetDefault);
		LOAD_SYM(LIBX11_SO, XWidthOfScreen);
		LOAD_SYM(LIBX11_SO, XHeightOfScreen);
		LOAD_SYM(LIBX11_SO, XDestroyWindow);
		LOAD_SYM(LIBX11_SO, XFree);
		LOAD_SYM(LIBX11_SO, XInitThreads);
		LOAD_SYM(LIBX11_SO, Xutf8LookupString);
		LOAD_SYM(LIBX11_SO, XOpenIM);
		LOAD_SYM(LIBX11_SO, XCloseIM);
		LOAD_SYM(LIBX11_SO, XCreateIC);
		LOAD_SYM(LIBX11_SO, XDestroyIC);
		LOAD_SYM(LIBX11_SO, XGetEventData);
		LOAD_SYM(LIBX11_SO, XGrabPointer);
		LOAD_SYM(LIBX11_SO, XUngrabPointer);
		LOAD_SYM(LIBX11_SO, XGrabKeyboard);
		LOAD_SYM(LIBX11_SO, XUngrabKeyboard);
		LOAD_SYM(LIBX11_SO, XWarpPointer);
		LOAD_SYM(LIBX11_SO, XSync);
		LOAD_SYM(LIBX11_SO, XCreateBitmapFromData);
		LOAD_SYM(LIBX11_SO, XCreatePixmapCursor);
		LOAD_SYM(LIBX11_SO, XFreePixmap);
		LOAD_SYM(LIBX11_SO, XDefineCursor);
		LOAD_SYM(LIBX11_SO, XFreeCursor);
		LOAD_SYM(LIBX11_SO, XGetSelectionOwner);
		LOAD_SYM(LIBX11_SO, XSetSelectionOwner);
		LOAD_SYM(LIBX11_SO, XKeysymToString);
		LOAD_SYM(LIBX11_SO, XConvertCase);
		LOAD_SYM(LIBX11_SO, XQueryPointer);
		LOAD_SYM(LIBX11_SO, XGetWindowProperty);
		LOAD_SYM(LIBX11_SO, XSendEvent);
		LOAD_SYM(LIBX11_SO, XConvertSelection);
		LOAD_SYM(LIBX11_SO, XSetWMProperties);
		LOAD_SYM(LIBX11_SO, XAllocSizeHints);
		LOAD_SYM(LIBX11_SO, XAllocWMHints);
		LOAD_SYM(LIBX11_SO, XAllocClassHint);
		LOAD_SYM(LIBX11_SO, XResetScreenSaver);

		LOAD_SYM_OPT(LIBX11_SO, XkbSetDetectableAutoRepeat);

		LOAD_SYM(LIBXI_SO, XISelectEvents);

		LOAD_SYM(LIBXCURSOR_SO, XcursorImageCreate);
		LOAD_SYM(LIBXCURSOR_SO, XcursorImageLoadCursor);
		LOAD_SYM(LIBXCURSOR_SO, XcursorImageDestroy);

		if (LIBXRANDR_SO) {
			LOAD_SYM_OPT(LIBXRANDR_SO, XRRGetScreenInfo);
			LOAD_SYM_OPT(LIBXRANDR_SO, XRRFreeScreenConfigInfo);
			LOAD_SYM_OPT(LIBXRANDR_SO, XRRConfigCurrentRate);
		}

		LOAD_SYM(LIBGL_SO, glXGetProcAddress);
		LOAD_SYM(LIBGL_SO, glXSwapBuffers);
		LOAD_SYM(LIBGL_SO, glXChooseVisual);
		LOAD_SYM(LIBGL_SO, glXCreateContext);
		LOAD_SYM(LIBGL_SO, glXMakeCurrent);
		LOAD_SYM(LIBGL_SO, glXDestroyContext);
		LOAD_SYM(LIBGL_SO, glXGetCurrentContext);

		except:

		if (!r)
			libX11_global_destroy();

		LIBX11_INIT = r;
	}

	MTY_GlobalUnlock(&LIBX11_LOCK);

	return LIBX11_INIT;
}
