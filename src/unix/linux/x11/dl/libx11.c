// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include "matoya.h"
#include "libx11.h"

#include "sym.h"


// X interface

// Reference: https://code.woboq.org/qt5/include/X11/

static Display *(*XOpenDisplay)(const char *display_name);
static Screen *(*XScreenOfDisplay)(Display *display, int screen_number);
static Screen *(*XDefaultScreenOfDisplay)(Display *display);
static int (*XScreenNumberOfScreen)(Screen *screen);
static int (*XCloseDisplay)(Display *display);
static Window (*XDefaultRootWindow)(Display *display);
static Window (*XRootWindowOfScreen)(Screen *screen);
static Colormap (*XCreateColormap)(Display *display, Window w, Visual *visual, int alloc);
static Window (*XCreateWindow)(Display *display, Window parent, int x, int y, unsigned int width,
	unsigned int height, unsigned int border_width, int depth, unsigned int class, Visual *visual,
	unsigned long valuemask, XSetWindowAttributes *attributes);
static int (*XWithdrawWindow)(Display *display, Window w);
static int (*XMapRaised)(Display *display, Window w);
static int (*XSetInputFocus)(Display *display, Window focus, int revert_to, Time time);
static int (*XStoreName)(Display *display, Window w, const char *window_name);
static Status (*XGetWindowAttributes)(Display *display, Window w, XWindowAttributes *window_attributes_return);
static Bool (*XTranslateCoordinates)(Display *display, Window src_w, Window dest_w, int src_x, int src_y,
	int *dest_x_return, int *dest_y_return, Window *child_return);
static KeySym (*XLookupKeysym)(XKeyEvent *key_event, int index);
static Status (*XSetWMProtocols)(Display *display, Window w, Atom *protocols, int count);
static Atom (*XInternAtom)(Display *display, const char *atom_name, Bool only_if_exists);
static int (*XNextEvent)(Display *display, XEvent *event_return);
static int (*XEventsQueued)(Display *display, int mode);
static int (*XMoveWindow)(Display *display, Window w, int x, int y);
static int (*XMoveResizeWindow)(Display *display, Window w, int x, int y, unsigned int width, unsigned int height);
static int (*XChangeProperty)(Display *display, Window w, Atom property, Atom type, int format, int mode, const unsigned char *data, int nelements);
static int (*XGetInputFocus)(Display *display, Window *focus_return, int *revert_to_return);
static char *(*XGetDefault)(Display *display, const char *program, const char *option);
static int (*XWidthOfScreen)(Screen *screen);
static int (*XHeightOfScreen)(Screen *screen);
static int (*XDestroyWindow)(Display *display, Window w);
static int (*XFree)(void *data);
static Status (*XInitThreads)(void);
static int (*Xutf8LookupString)(XIC ic, XKeyPressedEvent *event, char *buffer_return, int bytes_buffer,
	KeySym *keysym_return, Status *status_return);
static XIM (*XOpenIM)(Display *dpy, struct _XrmHashBucketRec *rdb, char *res_name, char *res_class);
static Status (*XCloseIM)(XIM im);
static XIC (*XCreateIC)(XIM im, ...);
static void (*XDestroyIC)(XIC ic);
static Bool (*XGetEventData)(Display *dpy, XGenericEventCookie *cookie);
static int (*XGrabPointer)(Display *display, Window grab_window, Bool owner_events, unsigned int event_mask, int pointer_mode,
	int keyboard_mode, Window confine_to, Cursor cursor, Time time);
static int (*XUngrabPointer)(Display *display, Time time);
static int (*XGrabKeyboard)(Display *display, Window grab_window, Bool owner_events, int pointer_mode, int keyboard_mode, Time time);
static int (*XUngrabKeyboard)(Display *display, Time time);
static int (*XWarpPointer)(Display *display, Window src_w, Window dest_w, int src_x, int src_y, unsigned int src_width,
	unsigned int src_height, int dest_x, int dest_y);
static int (*XSync)(Display *display, Bool discard);
static Pixmap (*XCreateBitmapFromData)(Display *display, Drawable d, _Xconst char *data, unsigned int width, unsigned int height);
static Cursor (*XCreatePixmapCursor)(Display *display, Pixmap source, Pixmap mask, XColor *foreground_color,
	XColor *background_color, unsigned int x, unsigned int y);
static int (*XFreePixmap)(Display *display, Pixmap pixmap);
static int (*XDefineCursor)(Display *display, Window w, Cursor cursor);
static int (*XFreeCursor)(Display *display, Cursor cursor);
static Window (*XGetSelectionOwner)(Display *display, Atom selection);
static int (*XSetSelectionOwner)(Display *display, Atom selection, Window owner, Time time);
static char *(*XKeysymToString)(KeySym keysym);
static void (*XConvertCase)(KeySym keysym, KeySym *lower_return, KeySym *upper_return);
static Bool (*XQueryPointer)(Display *display, Window w, Window *root_return, Window *child_return,
	int *root_x_return, int *root_y_return, int *win_x_return, int *win_y_return, unsigned int *mask_return);
static int (*XGetWindowProperty)(Display *display, Window w, Atom property, long long_offset, long long_length,
	Bool delete, Atom req_type, Atom *actual_type_return, int *actual_format_return, unsigned long *nitems_return,
	unsigned long *bytes_after_return, unsigned char **prop_return);
static Status (*XSendEvent)(Display *display, Window w, Bool propagate, long event_mask, XEvent *event_send);
static int (*XConvertSelection)(Display *display, Atom selection, Atom target, Atom property, Window requestor, Time time);
static void (*XSetWMProperties)(Display *display, Window w, XTextProperty *window_name, XTextProperty *icon_name, char **argv,
	int argc, XSizeHints *normal_hints, XWMHints *wm_hints, XClassHint *class_hints);
static XSizeHints *(*XAllocSizeHints)(void);
static XWMHints *(*XAllocWMHints)(void);
static XClassHint *(*XAllocClassHint)(void);
static int (*XResetScreenSaver)(Display *display);


// XKB interface (part of libX11 in modern times)

static Bool (*XkbSetDetectableAutoRepeat)(Display *dpy, Bool detectable, Bool *supported);


// XI2 interface

// Reference: https://code.woboq.org/kde/include/X11/extensions/

static int (*XISelectEvents)(Display *dpy, Window win, XIEventMask *masks, int num_masks);


// Xcursor interface

static XcursorImage *(*XcursorImageCreate)(int width, int height);
static Cursor (*XcursorImageLoadCursor)(Display *dpy, const XcursorImage *image);
static void (*XcursorImageDestroy)(XcursorImage *image);


// GLX interface

// Reference: https://code.woboq.org/qt5/include/GL/

static void *(*glXGetProcAddress)(const GLubyte *procName);
static void (*glXSwapBuffers)(Display *dpy, GLXDrawable drawable);
static XVisualInfo *(*glXChooseVisual)(Display *dpy, int screen, int *attribList);
static GLXContext (*glXCreateContext)(Display *dpy, XVisualInfo *vis, GLXContext shareList, Bool direct);
static Bool (*glXMakeCurrent)(Display *dpy, GLXDrawable drawable, GLXContext ctx);
static void (*glXDestroyContext)(Display *dpy, GLXContext ctx);
static GLXContext (*glXGetCurrentContext)(void);

static MTY_Atomic32 LIBX11_LOCK;
static MTY_SO *LIBGL_SO;
static MTY_SO *LIBXI_SO;
static MTY_SO *LIBXCURSOR_SO;
static MTY_SO *LIBX11_SO;
static bool LIBX11_INIT;

static void __attribute__((destructor)) libX11_global_destroy(void)
{
	MTY_GlobalLock(&LIBX11_LOCK);

	MTY_SOUnload(&LIBGL_SO);
	MTY_SOUnload(&LIBXI_SO);
	MTY_SOUnload(&LIBXCURSOR_SO);
	MTY_SOUnload(&LIBX11_SO);
	LIBX11_INIT = false;

	MTY_GlobalUnlock(&LIBX11_LOCK);
}

static bool libX11_global_init(void)
{
	MTY_GlobalLock(&LIBX11_LOCK);

	if (!LIBX11_INIT) {
		bool r = true;

		LIBX11_SO = MTY_SOLoad("libX11.so.6");
		LIBXI_SO = MTY_SOLoad("libXi.so.6");
		LIBXCURSOR_SO = MTY_SOLoad("libXcursor.so.1");
		LIBGL_SO = MTY_SOLoad("libGL.so.1");

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
