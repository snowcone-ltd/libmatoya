// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#define _DEFAULT_SOURCE // pid_t

#include "app.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <limits.h>

#include <unistd.h>

#include "dl/libx11.c"
#include "hid/utils.h"
#include "evdev.h"
#include "keymap.h"

struct window {
	struct window_common cmn;
	Window window;
	MTY_Window index;
	XIC ic;
	MTY_Frame frame;
	int32_t last_width;
	int32_t last_height;
	struct xinfo info;
};

struct MTY_App {
	Display *display;
	Cursor empty_cursor;
	Cursor custom_cursor;
	Cursor cursor;
	bool hide_cursor;
	char *class_name;
	MTY_Cursor scursor;
	MTY_DetachState detach;
	XVisualInfo *vis;
	Atom wm_close;
	Atom wm_ping;
	Window sel_owner;
	XIM im;

	MTY_EventFunc event_func;
	MTY_AppFunc app_func;
	MTY_Hash *hotkey;
	MTY_Hash *deduper;
	MTY_Mutex *mutex;
	struct evdev *evdev;
	struct window *windows[MTY_WINDOW_MAX];
	uint32_t timeout;
	MTY_Time suspend_ts;
	bool relative;
	bool suspend_ss;
	bool mgrab;
	bool kbgrab;
	void *opaque;
	char *clip;
	float scale;

	bool xfixes;
	int32_t xfixes_base;

	uint64_t state;
	uint64_t prev_state;
};


// Helpers

static struct window *app_get_window(MTY_App *ctx, MTY_Window window)
{
	return window < 0 ? NULL : ctx->windows[window];
}

static MTY_Window app_find_open_window(MTY_App *ctx, MTY_Window req)
{
	if (req >= 0 && req < MTY_WINDOW_MAX && !ctx->windows[req])
		return req;

	for (MTY_Window x = 0; x < MTY_WINDOW_MAX; x++)
		if (!ctx->windows[x])
			return x;

	return -1;
}

static MTY_Window app_find_window(MTY_App *ctx, Window xwindow)
{
	for (MTY_Window x = 0; x < MTY_WINDOW_MAX; x++) {
		struct window *win = app_get_window(ctx, x);

		if (win->window == xwindow)
			return x;
	}

	return -1;
}

static struct window *app_get_active_window(MTY_App *ctx)
{
	Window w = 0;
	int32_t revert = 0;
	XGetInputFocus(ctx->display, &w, &revert);

	for (MTY_Window x = 0; x < MTY_WINDOW_MAX; x++) {
		struct window *win = app_get_window(ctx, x);

		if (win && win->window == w)
			return win;
	}

	return NULL;
}


// Window manager helpers

static uint8_t window_wm_state(Display *display, Window w)
{
	Atom type;
	int format;
	unsigned char *value = NULL;
	unsigned long bytes, n;

	uint8_t state = 0;

	if (XGetWindowProperty(display, w, XInternAtom(display, "_NET_WM_STATE", False),
		0, 1024, False, XA_ATOM, &type, &format, &n, &bytes, &value) == Success)
	{
		Atom *atoms = (Atom *) value;

		for (unsigned long x = 0; x < n; x++) {
			if (atoms[x] == XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", False))
				state |= 0x1;

			if (atoms[x] == XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_HORZ", False))
				state |= 0x2;

			if (atoms[x] == XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_VERT", False))
				state |= 0x4;
		}
	}

	return state;
}

static void window_wm_event(Display *display, Window w, int action, const char *state1, const char *state2)
{
	XWindowAttributes attr = {0};
	XGetWindowAttributes(display, w, &attr);

	XEvent evt = {0};
	evt.type = ClientMessage;
	evt.xclient.message_type = XInternAtom(display, "_NET_WM_STATE", False);
	evt.xclient.format = 32;
	evt.xclient.window = w;
	evt.xclient.data.l[0] = action;
	evt.xclient.data.l[1] = state1 ? XInternAtom(display, state1, False) : 0;
	evt.xclient.data.l[2] = state2 ? XInternAtom(display, state2, False) : 0;

	XSendEvent(display, XRootWindowOfScreen(attr.screen), 0,
		SubstructureNotifyMask | SubstructureRedirectMask, &evt);

	XSync(display, False);
}


// Clipboard (selections)

static void app_handle_selection_request(MTY_App *ctx, const XEvent *event)
{
	// Unlike other OSs, X11 receives a request from other applications
	// to set a window property (Atom)

	struct window *win0 = app_get_window(ctx, 0);
	if (!win0)
		return;

	const XSelectionRequestEvent *req = &event->xselectionrequest;

	XEvent snd = {0};
	snd.type = SelectionNotify;

	XSelectionEvent *res = &snd.xselection;
	res->selection = req->selection;
	res->requestor = req->requestor;
	res->time = req->time;

	unsigned long bytes, overflow;
	unsigned char *data = NULL;
	int format = 0;

	Atom targets = XInternAtom(ctx->display, "TARGETS", False);
	Atom mty_clip = XInternAtom(ctx->display, "MTY_CLIPBOARD", False);

	// Get the utf8 clipboard buffer associated with window 0
	if (XGetWindowProperty(ctx->display, win0->window, mty_clip, 0, INT_MAX / 4, False, req->target,
		&res->target, &format, &bytes, &overflow, &data) == Success)
	{
		// Requestor wants the data, if the target (format) matches our buffer, set it
		if (req->target == res->target) {
			XChangeProperty(ctx->display, req->requestor, req->property, res->target,
				format, PropModeReplace, data, bytes);

			res->property = req->property;

		// Requestor is querying which targets (formats) are available (the TARGETS atom)
		} else if (req->target == targets) {
			Atom formats[] = {targets, res->target};
			XChangeProperty(ctx->display, req->requestor, req->property, XA_ATOM, 32, PropModeReplace,
				(unsigned char *) formats, 2);

			res->property = req->property;
			res->target = targets;
		}

		XFree(data);
	}

	// Send the response event
	XSendEvent(ctx->display, req->requestor, False, 0, &snd);
	XSync(ctx->display, False);
}

static void app_handle_selection_notify(MTY_App *ctx, const XSelectionEvent *res)
{
	struct window *win0 = app_get_window(ctx, 0);
	if (!win0)
		return;

	Atom format = XInternAtom(ctx->display, "UTF8_STRING", False);
	Atom mty_clip = XInternAtom(ctx->display, "MTY_CLIPBOARD", False);

	// Unhandled format or bad conversion
	if (!res->property || res->target != format)
		return;

	unsigned long overflow, bytes;
	unsigned char *src = NULL;
	Atom res_type;
	int res_format;

	if (XGetWindowProperty(ctx->display, win0->window, mty_clip, 0, INT_MAX / 4, False,
		format, &res_type, &res_format, &bytes, &overflow, &src) == Success)
	{
		MTY_MutexLock(ctx->mutex);

		MTY_Free(ctx->clip);
		ctx->clip = MTY_Alloc(bytes + 1, 1);
		memcpy(ctx->clip, src, bytes);

		MTY_MutexUnlock(ctx->mutex);

		// FIXME this is a questionable technique: will it interfere with other applications?
		// We take back the selection so that we can be notified the next time a different app takes it
		// Xfixes allows us to be notified when the selection owner changes, so there is no need for this
		if (!ctx->xfixes)
			XSetSelectionOwner(ctx->display, XInternAtom(ctx->display, "CLIPBOARD", False), win0->window, CurrentTime);

		MTY_Event evt = {0};
		evt.type = MTY_EVENT_CLIPBOARD;
		ctx->event_func(&evt, ctx->opaque);
	}
}

static void app_poll_clipboard(MTY_App *ctx)
{
	Atom clip = XInternAtom(ctx->display, "CLIPBOARD", False);

	// Xfixes notifies when ownership changes
	Window sel_owner = XGetSelectionOwner(ctx->display, clip);
	if (sel_owner != ctx->sel_owner || ctx->xfixes) {
		struct window *win0 = app_get_window(ctx, 0);

		if (win0 && sel_owner != win0->window) {
			Atom format = XInternAtom(ctx->display, "UTF8_STRING", False);
			Atom mty_clip = XInternAtom(ctx->display, "MTY_CLIPBOARD", False);

			// This will send the request out to the owner
			XConvertSelection(ctx->display, clip, format, mty_clip, win0->window, CurrentTime);
		}

		ctx->sel_owner = sel_owner;
	}
}


// Cursor/grab state

static void app_apply_keyboard_grab(MTY_App *app, struct window *win)
{
	if (win && app->kbgrab && app->detach == MTY_DETACH_STATE_NONE) {
		XGrabKeyboard(app->display, win->window, False, GrabModeAsync, GrabModeAsync, CurrentTime);

	} else {
		XUngrabKeyboard(app->display, CurrentTime);
	}
}

static void app_apply_mouse_grab(MTY_App *app, struct window *win)
{
	if (win && // One of our windows is the focus window
		((app->relative && app->detach != MTY_DETACH_STATE_FULL) || // In relative mode and not fully detached
		(app->mgrab && app->detach == MTY_DETACH_STATE_NONE))) // Mouse grab active and not detached
	{
		XGrabPointer(app->display, win->window, False,
			ButtonPressMask | ButtonReleaseMask | PointerMotionMask | FocusChangeMask,
			GrabModeAsync, GrabModeAsync, win->window, None, CurrentTime);

	} else {
		// FIXME This should also warp to the stored coordinates when SetRelative was called
		XUngrabPointer(app->display, CurrentTime);
	}
}

static void app_apply_cursor(MTY_App *app, bool focus)
{
	Cursor cur = None;

	if (focus && (app->hide_cursor || (app->relative && app->detach == MTY_DETACH_STATE_NONE))) {
		cur = app->empty_cursor;

	} else {
		Cursor scursor = None;

		switch (app->scursor) {
			case MTY_CURSOR_ARROW: scursor = XCreateFontCursor(app->display, XC_left_ptr); break;
			case MTY_CURSOR_HAND:  scursor = XCreateFontCursor(app->display, XC_hand2);    break;
			case MTY_CURSOR_IBEAM: scursor = XCreateFontCursor(app->display, XC_xterm);    break;
		}

		cur = scursor != None ? scursor : app->custom_cursor != None ? app->custom_cursor : None;
	}

	if (cur != app->cursor) {
		for (MTY_Window x = 0; x < MTY_WINDOW_MAX; x++) {
			struct window *win = app_get_window(app, x);

			if (win)
				XDefineCursor(app->display, win->window, cur);
		}

		app->cursor = cur;
	}
}


// Event handling

static void window_text_event(MTY_App *ctx, XEvent *event)
{
	struct window *win = app_get_window(ctx, 0);
	if (!win)
		return;

	MTY_Event evt = {0};
	evt.type = MTY_EVENT_TEXT;

	Status status = 0;
	KeySym ks = 0;

	if (Xutf8LookupString(win->ic, (XKeyPressedEvent *) event, evt.text, 8, &ks, &status) > 0)
		if (!win->cmn.webview || !mty_webview_event(win->cmn.webview, &evt))
			ctx->event_func(&evt, ctx->opaque);
}

static float app_get_scale(Display *display)
{
	const char *dpi = XGetDefault(display, "Xft", "dpi");
	float scale = dpi ? atoi(dpi) / 96.0f : 1.0f;

	return scale == 0.0f ? 1.0f : scale;
}

static void app_refresh_scale(MTY_App *ctx)
{
	Display *display = XOpenDisplay(NULL);

	ctx->scale = app_get_scale(display);

	XCloseDisplay(display);
}

static void app_event(MTY_App *ctx, XEvent *event)
{
	MTY_Event evt = {0};

	switch (event->type) {
		case KeyPress:
			window_text_event(ctx, event);
			// Fall through

		case KeyRelease: {
			evt.key.key = keymap_keycode_to_key(event->xkey.keycode);

			if (evt.key.key != MTY_KEY_NONE) {
				evt.type = MTY_EVENT_KEY;
				evt.window = app_find_window(ctx, event->xkey.window);
				evt.key.vkey = XLookupKeysym(&event->xkey, 0);
				evt.key.pressed = event->type == KeyPress;
				evt.key.mod = keymap_keystate_to_keymod(evt.key.key,
					evt.key.pressed, event->xkey.state);
			}
			break;
		}
		case ButtonPress:
		case ButtonRelease:
			evt.type = MTY_EVENT_BUTTON;
			evt.window = app_find_window(ctx, event->xbutton.window);
			evt.button.pressed = event->type == ButtonPress;
			evt.button.x = event->xbutton.x;
			evt.button.y = event->xbutton.y;

			switch (event->xbutton.button) {
				case 1: evt.button.button = MTY_BUTTON_LEFT;   break;
				case 2: evt.button.button = MTY_BUTTON_MIDDLE; break;
				case 3: evt.button.button = MTY_BUTTON_RIGHT;  break;
				case 8: evt.button.button = MTY_BUTTON_X1;     break;
				case 9: evt.button.button = MTY_BUTTON_X2;     break;

				// Mouse wheel
				case 4:
				case 5:
				case 6:
				case 7:
					if (event->type == ButtonPress) {
						evt.type = MTY_EVENT_SCROLL;
						evt.scroll.x = event->xbutton.button == 6 ? -120 : event->xbutton.button == 7 ?  120 : 0;
						evt.scroll.y = event->xbutton.button == 4 ?  120 : event->xbutton.button == 5 ? -120 : 0;
						evt.scroll.pixels = false;

					} else {
						evt.type = MTY_EVENT_NONE;
					}
					break;
			}
			break;
		case MotionNotify:
			if (ctx->relative)
				break;

			evt.type = MTY_EVENT_MOTION;
			evt.window = app_find_window(ctx, event->xmotion.window);
			evt.motion.relative = false;
			evt.motion.x = event->xmotion.x;
			evt.motion.y = event->xmotion.y;
			break;
		case ClientMessage: {
			Atom type = (Atom) event->xclient.data.l[0];

			if (type == ctx->wm_close) {
				evt.window = app_find_window(ctx, event->xclient.window);
				evt.type = MTY_EVENT_CLOSE;

			// Ping -> Pong
			} else if (type == ctx->wm_ping) {
				Window root = XDefaultRootWindow(ctx->display);

				event->xclient.window = root;
				XSendEvent(ctx->display, root, False, SubstructureRedirectMask | SubstructureNotifyMask, event);
			}
			break;
		}
		case GenericEvent:
			if (!ctx->relative)
				break;

			if (XGetEventData(ctx->display, &event->xcookie)) {
				if (event->xcookie.evtype == XI_RawMotion) {
					const XIRawEvent *re = (const XIRawEvent *) event->xcookie.data;
					const double *input = (const double *) re->raw_values;
					int32_t output[2] = {0};

					for (int32_t i = 0; i < 2; i++)
						if (XIMaskIsSet(re->valuators.mask, i))
							output[i] = (int32_t) lrint(*input++);

					struct window *win = app_get_active_window(ctx);
					if (win && (output[0] != 0 || output[1] != 0)) {
						evt.type = MTY_EVENT_MOTION;
						evt.window = win->index;
						evt.motion.relative = true;
						evt.motion.x = output[0];
						evt.motion.y = output[1];
					}
				}
			}
			break;
		case SelectionRequest:
			app_handle_selection_request(ctx, event);
			break;
		case SelectionNotify:
			app_handle_selection_notify(ctx, &event->xselection);
			break;
		case Expose:
			app_refresh_scale(ctx);
			ctx->state++;
			break;
		case ConfigureNotify: {
			const XConfigureEvent *xc = &event->xconfigure;

			struct window *win = app_get_window(ctx, app_find_window(ctx, xc->window));
			if (!win)
				break;

			if (win->last_width != xc->width || win->last_height != xc->height) {
				evt.type = MTY_EVENT_SIZE;
				win->last_width = xc->width;
				win->last_height = xc->height;

				if (window_wm_state(ctx->display, xc->window) == 0) {
					win->frame.size.w = xc->width;
					win->frame.size.h = xc->height;
				}
			}

			if (xc->send_event && (win->frame.x != xc->x || win->frame.y != xc->y)) {
				evt.type = MTY_EVENT_MOVE;
				win->frame.x = xc->x;
				win->frame.y = xc->y;
			}
			break;
		}
		case FocusIn:
		case FocusOut:
			// Do not respond to NotifyGrab or NotifyUngrab
			if (event->xfocus.mode == NotifyNormal || event->xfocus.mode == NotifyWhileGrabbed) {
				evt.type = MTY_EVENT_FOCUS;
				evt.focus = event->type == FocusIn;
			}
			ctx->state++;
			break;
		default:
			// Xfixes gets selection ownership changes without polling
			if (ctx->xfixes && event->type == ctx->xfixes_base + XFixesSelectionNotify)
				app_poll_clipboard(ctx);
			break;
	}

	// Transform keyboard into hotkey
	if (evt.type == MTY_EVENT_KEY)
		mty_app_kb_to_hotkey(ctx, &evt, MTY_EVENT_HOTKEY);

	// Handle the message
	if (evt.type != MTY_EVENT_NONE) {
		struct window *win = app_get_window(ctx, evt.window);

		if (!win || !win->cmn.webview || !mty_webview_event(win->cmn.webview, &evt))
			ctx->event_func(&evt, ctx->opaque);
	}
}


// App

static void app_evdev_connect(struct evdev_dev *device, void *opaque)
{
	MTY_App *ctx = opaque;

	MTY_Event evt = {0};
	evt.type = MTY_EVENT_CONNECT;
	evt.controller = mty_evdev_state(device);
	mty_hid_map_axes(&evt.controller);

	ctx->event_func(&evt, ctx->opaque);
}

static void app_evdev_disconnect(struct evdev_dev *device, void *opaque)
{
	MTY_App *ctx = opaque;

	MTY_Event evt = {0};
	evt.type = MTY_EVENT_DISCONNECT;
	evt.controller = mty_evdev_state(device);
	mty_hid_map_axes(&evt.controller);

	ctx->event_func(&evt, ctx->opaque);
}

static void app_evdev_report(struct evdev_dev *device, void *opaque)
{
	MTY_App *ctx = opaque;

	if (MTY_AppIsActive(ctx)) {
		MTY_Event evt = {0};
		evt.type = MTY_EVENT_CONTROLLER;
		evt.controller = mty_evdev_state(device);
		mty_hid_map_axes(&evt.controller);

		if (mty_hid_dedupe(ctx->deduper, &evt.controller))
			ctx->event_func(&evt, ctx->opaque);
	}
}

static Cursor app_create_empty_cursor(Display *display)
{
	Cursor cursor = 0;

	char data[1] = {0};
	Pixmap p = XCreateBitmapFromData(display, XDefaultRootWindow(display), data, 1, 1);

	if (p) {
		XColor c = {0};
		cursor = XCreatePixmapCursor(display, p, p, &c, &c, 0, 0);

		XFreePixmap(display, p);
	}

	return cursor;
}

MTY_App *MTY_AppCreate(MTY_AppFlag flags, MTY_AppFunc appFunc, MTY_EventFunc eventFunc, void *opaque)
{
	if (!libX11_global_init())
		return NULL;

	XInitThreads();

	bool r = true;
	MTY_App *ctx = MTY_Alloc(1, sizeof(MTY_App));
	ctx->hotkey = MTY_HashCreate(0);
	ctx->deduper = MTY_HashCreate(0);
	ctx->mutex = MTY_MutexCreate();
	ctx->app_func = appFunc;
	ctx->event_func = eventFunc;
	ctx->opaque = opaque;
	ctx->class_name = MTY_Strdup(MTY_GetFileName(MTY_GetProcessPath(), false));

	// This may return NULL
	ctx->evdev = mty_evdev_create(app_evdev_connect, app_evdev_disconnect, ctx);

	ctx->display = XOpenDisplay(NULL);
	if (!ctx->display) {
		r = false;
		goto except;
	}

	// Prevents KeyRelease from being generated on keyboard autorepeat (optional)
	if (XkbSetDetectableAutoRepeat)
		XkbSetDetectableAutoRepeat(ctx->display, True, NULL);

	ctx->empty_cursor = app_create_empty_cursor(ctx->display);

	unsigned char mask[3] = {0};
	XISetMask(mask, XI_RawMotion);

	XIEventMask em = {0};
	em.deviceid = XIAllMasterDevices;
	em.mask_len = 3;
	em.mask = mask;

	XISelectEvents(ctx->display, XDefaultRootWindow(ctx->display), &em, 1);

	ctx->im = XOpenIM(ctx->display, NULL, NULL, NULL);
	if (!ctx->im) {
		r = false;
		goto except;
	}

	GLint attr[] = {GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None};
	ctx->vis = glXChooseVisual(ctx->display, 0, attr);
	if (!ctx->vis) {
		r = false;
		goto except;
	}

	ctx->wm_close = XInternAtom(ctx->display, "WM_DELETE_WINDOW", False);
	ctx->wm_ping = XInternAtom(ctx->display, "_NET_WM_PING", False);

	app_refresh_scale(ctx);

	// Check if Xfixes is available for clipboard events
	if (XFixesQueryExtension && XFixesSelectSelectionInput) {
		int32_t error_base = 0;

		ctx->xfixes = XFixesQueryExtension(ctx->display, &ctx->xfixes_base, &error_base);

		if (ctx->xfixes)
			XFixesSelectSelectionInput(ctx->display, XDefaultRootWindow(ctx->display),
				XInternAtom(ctx->display, "CLIPBOARD", False), XFixesSetSelectionOwnerNotifyMask);
	}

	except:

	if (!r)
		MTY_AppDestroy(&ctx);

	return ctx;
}

void MTY_AppDestroy(MTY_App **app)
{
	if (!app || !*app)
		return;

	MTY_App *ctx = *app;

	if (ctx->empty_cursor)
		XFreeCursor(ctx->display, ctx->empty_cursor);

	if (ctx->vis)
		XFree(ctx->vis);

	if (ctx->im)
		XCloseIM(ctx->im);

	if (ctx->display)
		XCloseDisplay(ctx->display);

	mty_evdev_destroy(&ctx->evdev);

	MTY_HashDestroy(&ctx->deduper, MTY_Free);
	MTY_HashDestroy(&ctx->hotkey, NULL);
	MTY_MutexDestroy(&ctx->mutex);
	MTY_Free(ctx->clip);
	MTY_Free(ctx->class_name);

	MTY_Free(*app);
	*app = NULL;
}

static void app_suspend_ss(MTY_App *ctx)
{
	MTY_Time now = MTY_GetTime();

	// Keep screen saver disabled in 30s intervals
	if (MTY_TimeDiff(ctx->suspend_ts, now) > 30000.0f) {
		XResetScreenSaver(ctx->display);
		ctx->suspend_ts = now;
	}
}

void MTY_AppRun(MTY_App *ctx)
{
	for (bool cont = true; cont;) {
		struct window *win0 = app_get_window(ctx, 0);

		// Grab / mouse state evaluation
		if (ctx->state != ctx->prev_state) {
			struct window *win = app_get_active_window(ctx);

			app_apply_keyboard_grab(ctx, win);
			app_apply_mouse_grab(ctx, win);
			app_apply_cursor(ctx, win != NULL);
			XSync(ctx->display, False);

			ctx->prev_state = ctx->state;
		}

		// Poll selection ownership changes
		if (!ctx->xfixes)
			app_poll_clipboard(ctx);

		// X11 events
		for (XEvent event; XEventsQueued(ctx->display, QueuedAfterFlush) > 0;) {
			XNextEvent(ctx->display, &event);
			app_event(ctx, &event);
		}

		// evdev events
		if (ctx->evdev)
			mty_evdev_poll(ctx->evdev, app_evdev_report);

		// WebView main thread upkeep (Steam callbacks)
		if (win0->cmn.webview)
			mty_webview_run(win0->cmn.webview);

		// Fire app func after all events have been processed
		cont = ctx->app_func(ctx->opaque);

		// Keep screensaver from turning on
		if (ctx->suspend_ss)
			app_suspend_ss(ctx);

		if (ctx->timeout > 0)
			MTY_Sleep(ctx->timeout);
	}
}

void MTY_AppSetTimeout(MTY_App *ctx, uint32_t timeout)
{
	ctx->timeout = timeout;
}

bool MTY_AppIsActive(MTY_App *ctx)
{
	return app_get_active_window(ctx) != NULL;
}

void MTY_AppActivate(MTY_App *ctx, bool active)
{
	MTY_WindowActivate(ctx, 0, active);
}

void MTY_AppSetTray(MTY_App *ctx, const char *tooltip, const MTY_MenuItem *items, uint32_t len)
{
}

void MTY_AppRemoveTray(MTY_App *ctx)
{
}

void MTY_AppSendNotification(MTY_App *ctx, const char *title, const char *msg)
{
}

char *MTY_AppGetClipboard(MTY_App *ctx)
{
	MTY_MutexLock(ctx->mutex);

	char *str = ctx->clip ? MTY_Strdup(ctx->clip) : NULL;

	MTY_MutexUnlock(ctx->mutex);

	return str;
}

void MTY_AppSetClipboard(MTY_App *ctx, const char *text)
{
	struct window *win = app_get_window(ctx, 0);
	if (!win)
		return;

	Atom clip = XInternAtom(ctx->display, "CLIPBOARD", False);
	Atom mty_clip = XInternAtom(ctx->display, "MTY_CLIPBOARD", False);
	Atom format = XInternAtom(ctx->display, "UTF8_STRING", False);

	XChangeProperty(ctx->display, win->window, mty_clip,
		format, 8, PropModeReplace, (const unsigned char *) text, strlen(text));

	if (XGetSelectionOwner(ctx->display, clip) != win->window)
		XSetSelectionOwner(ctx->display, clip, win->window, CurrentTime);

	if (XGetSelectionOwner(ctx->display, XA_PRIMARY) != win->window)
		XSetSelectionOwner(ctx->display, XA_PRIMARY, win->window, CurrentTime);
}

void MTY_AppStayAwake(MTY_App *ctx, bool enable)
{
	ctx->suspend_ss = enable;
}

MTY_DetachState MTY_AppGetDetachState(MTY_App *ctx)
{
	return ctx->detach;
}

void MTY_AppSetDetachState(MTY_App *ctx, MTY_DetachState state)
{
	if (ctx->detach != state) {
		ctx->detach = state;
		ctx->state++;
	}
}

bool MTY_AppIsMouseGrabbed(MTY_App *ctx)
{
	return ctx->mgrab;
}

void MTY_AppGrabMouse(MTY_App *ctx, bool grab)
{
	if (ctx->mgrab != grab) {
		ctx->mgrab = grab;
		ctx->state++;
	}
}

bool MTY_AppGetRelativeMouse(MTY_App *ctx)
{
	return ctx->relative;
}

void MTY_AppSetRelativeMouse(MTY_App *ctx, bool relative)
{
	if (ctx->relative != relative) {
		// FIXME This should keep track of the position where the cursor went into relative,
		// but for now since we usually call WarpCursor whenever we exit relative it
		// doesn't matter

		ctx->relative = relative;

		struct window *win = app_get_active_window(ctx);

		app_apply_mouse_grab(ctx, win);
		app_apply_cursor(ctx, win != NULL);
		XSync(ctx->display, False);
	}
}

static Cursor app_png_cursor(Display *display, const void *image, size_t size, uint32_t hotX, uint32_t hotY)
{
	Cursor cursor = None;
	XcursorImage *ximage = NULL;

	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t *rgba = MTY_DecompressImage(image, size, &width, &height);
	if (!rgba)
		goto except;

	ximage = XcursorImageCreate(width, height);
	if (!ximage)
		goto except;

	ximage->xhot = hotX;
	ximage->yhot = hotY;
	ximage->delay = 0;

	for (uint32_t i = 0; i < width * height; i++) {
		uint32_t p = rgba[i];
		uint32_t a = (p & 0x00FF0000) >> 16;
		uint32_t b = (p & 0x000000FF) << 16;;

		ximage->pixels[i] = (p & 0xFF00FF00) | a | b;
	}

	cursor = XcursorImageLoadCursor(display, ximage);

	except:

	if (ximage)
		XcursorImageDestroy(ximage);

	MTY_Free(rgba);

	return cursor;
}

void MTY_AppSetPNGCursor(MTY_App *ctx, const void *image, size_t size, uint32_t hotX, uint32_t hotY)
{
	Cursor prev = ctx->custom_cursor;
	ctx->custom_cursor = None;

	if (image && size > 0)
		ctx->custom_cursor = app_png_cursor(ctx->display, image, size, hotX, hotY);

	if (prev)
		XFreeCursor(ctx->display, prev);

	ctx->state++;
}

void MTY_AppSetCursor(MTY_App *ctx, MTY_Cursor cursor)
{
	if (ctx->scursor != cursor) {
		ctx->scursor = cursor;
		ctx->state++;
	}
}

void MTY_AppShowCursor(MTY_App *ctx, bool show)
{
	if (ctx->hide_cursor == show) {
		ctx->hide_cursor = !show;
		ctx->state++;
	}
}

bool MTY_AppCanWarpCursor(MTY_App *ctx)
{
	return true;
}

bool MTY_AppIsKeyboardGrabbed(MTY_App *ctx)
{
	return ctx->kbgrab;
}

bool MTY_AppGrabKeyboard(MTY_App *ctx, bool grab)
{
	if (ctx->kbgrab != grab) {
		ctx->kbgrab = grab;
		ctx->state++;
	}

	return ctx->kbgrab;
}

uint32_t MTY_AppGetHotkey(MTY_App *ctx, MTY_Scope scope, MTY_Mod mod, MTY_Key key)
{
	mod &= 0xFF;

	return (uint32_t) (uintptr_t) MTY_HashGetInt(ctx->hotkey, (mod << 16) | key);
}

void MTY_AppSetHotkey(MTY_App *ctx, MTY_Scope scope, MTY_Mod mod, MTY_Key key, uint32_t id)
{
	mod &= 0xFF;
	MTY_HashSetInt(ctx->hotkey, (mod << 16) | key, (void *) (uintptr_t) id);
}

void MTY_AppRemoveHotkeys(MTY_App *ctx, MTY_Scope scope)
{
	MTY_HashDestroy(&ctx->hotkey, NULL);
	ctx->hotkey = MTY_HashCreate(0);
}

void MTY_AppEnableGlobalHotkeys(MTY_App *ctx, bool enable)
{
}

bool MTY_AppIsSoftKeyboardShowing(MTY_App *ctx)
{
	return false;
}

void MTY_AppShowSoftKeyboard(MTY_App *ctx, bool show)
{
}

MTY_Orientation MTY_AppGetOrientation(MTY_App *ctx)
{
	return MTY_ORIENTATION_USER;
}

void MTY_AppSetOrientation(MTY_App *ctx, MTY_Orientation orientation)
{
}

void MTY_AppRumbleController(MTY_App *ctx, uint32_t id, uint16_t low, uint16_t high)
{
	if (ctx->evdev)
		mty_evdev_rumble(ctx->evdev, id, low, high);
}

const char *MTY_AppGetControllerDeviceName(MTY_App *ctx, uint32_t id)
{
	return NULL;
}

MTY_CType MTY_AppGetControllerType(MTY_App *ctx, uint32_t id)
{
	return MTY_CTYPE_DEFAULT;
}

void MTY_AppSubmitHIDReport(MTY_App *ctx, uint32_t id, const void *report, size_t size)
{
}

bool MTY_AppIsPenEnabled(MTY_App *ctx)
{
	return false;
}

void MTY_AppEnablePen(MTY_App *ctx, bool enable)
{
}

MTY_InputMode MTY_AppGetInputMode(MTY_App *ctx)
{
	return MTY_INPUT_MODE_UNSPECIFIED;
}

void MTY_AppSetInputMode(MTY_App *ctx, MTY_InputMode mode)
{
}

void MTY_AppSetWMsgFunc(MTY_App *ctx, MTY_WMsgFunc func)
{
}


// Window

static void window_set_up_wm(MTY_App *app, Window w)
{
	// This function sets up misc things associated with the standardized _NET_WM
	// system for communicating with the window manager

	XWMHints *wmhints = XAllocWMHints();
	wmhints->input = True;
	wmhints->window_group = (uintptr_t) app;
	wmhints->flags = InputHint | WindowGroupHint;

	XClassHint *chints = XAllocClassHint();
	chints->res_name = app->class_name;
	chints->res_class = app->class_name;

	XSetWMProperties(app->display, w, NULL, NULL, NULL, 0, NULL, wmhints, chints);

	XFree(wmhints);
	XFree(chints);

	Atom wintype = XInternAtom(app->display, "_NET_WM_WINDOW_TYPE_NORMAL", False);
	XChangeProperty(app->display, w, XInternAtom(app->display, "_NET_WM_WINDOW_TYPE", False), XA_ATOM, 32,
		PropModeReplace, (unsigned char *) &wintype, 1);

	pid_t pid = getpid();
	XChangeProperty(app->display, w, XInternAtom(app->display, "_NET_WM_PID", False),
		XA_CARDINAL, 32, PropModeReplace, (unsigned char *) &pid, 1);

	Atom protos[2] = {app->wm_close, app->wm_ping};
	XSetWMProtocols(app->display, w, protos, 2);
}

static MTY_Frame window_denormalize_frame(const MTY_Frame *frame, float scale)
{
	int32_t px_h = lrint(frame->size.h * (scale - 1));
	int32_t px_w = lrint(frame->size.w * (scale - 1));

	MTY_Frame dframe = *frame;
	dframe.x -= px_w / 2;
	dframe.y -= px_h / 2;
	dframe.size.w += px_w;
	dframe.size.h += px_h;

	// Ensure the title bar is visible
	if (dframe.y < 0)
		dframe.y = 0;

	return dframe;
}

static MTY_Frame window_normalize_frame(const MTY_Frame *frame, float scale)
{
	int32_t px_h = lrint(frame->size.h * (1 - (1 / scale)));
	int32_t px_w = lrint(frame->size.w * (1 - (1 / scale)));

	MTY_Frame nframe = *frame;
	nframe.x += px_w / 2;
	nframe.y += px_h / 2;
	nframe.size.w -= px_w;
	nframe.size.h -= px_h;

	return nframe;
}

MTY_Window MTY_WindowCreate(MTY_App *app, const char *title, const MTY_Frame *frame, MTY_Window index)
{
	bool r = true;
	struct window *ctx = MTY_Alloc(1, sizeof(struct window));

	MTY_Window window = app_find_open_window(app, index);
	if (window == -1) {
		r = false;
		MTY_Log("Maximum windows (MTY_WINDOW_MAX) of %u reached", MTY_WINDOW_MAX);
		goto except;
	}

	ctx->index = window;
	app->windows[window] = ctx;

	MTY_Frame dframe = {0};

	if (!frame) {
		dframe = APP_DEFAULT_FRAME();
		frame = &dframe;
	}

	dframe = window_denormalize_frame(frame, app->scale);
	frame = &dframe;

	Screen *screen = XScreenOfDisplay(app->display, atoi(frame->screen));
	Window root = XRootWindowOfScreen(screen);

	XSetWindowAttributes swa = {0};
	swa.colormap = XCreateColormap(app->display, root, app->vis->visual, AllocNone);
	swa.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask |
		ButtonReleaseMask | PointerMotionMask | FocusChangeMask | StructureNotifyMask;

	ctx->window = XCreateWindow(app->display, root, 0, 0, frame->size.w, frame->size.h, 0, app->vis->depth,
		InputOutput, app->vis->visual, CWColormap | CWEventMask, &swa);

	ctx->ic = XCreateIC(app->im, XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
		XNClientWindow, ctx->window, NULL);

	XMapRaised(app->display, ctx->window);
	XMoveWindow(app->display, ctx->window, frame->x, frame->y);

	if (frame->type & MTY_WINDOW_MAXIMIZED)
		window_wm_event(app->display, ctx->window, _NET_WM_STATE_ADD,
			"_NET_WM_STATE_MAXIMIZED_HORZ", "_NET_WM_STATE_MAXIMIZED_VERT");

	if (frame->type & MTY_WINDOW_FULLSCREEN)
		window_wm_event(app->display, ctx->window, _NET_WM_STATE_ADD, "_NET_WM_STATE_FULLSCREEN", NULL);

	MTY_WindowSetTitle(app, window, title ? title : "MTY_Window");

	window_set_up_wm(app, ctx->window);

	ctx->info.display = app->display;
	ctx->info.vis = app->vis;
	ctx->info.window = ctx->window;

	except:

	if (!r) {
		MTY_WindowDestroy(app, window);
		window = -1;
	}

	return window;
}

void MTY_WindowDestroy(MTY_App *app, MTY_Window window)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return;

	if (ctx->ic)
		XDestroyIC(ctx->ic);

	XDestroyWindow(app->display, ctx->window);

	mty_webview_destroy(&ctx->cmn.webview);

	MTY_Free(ctx);
	app->windows[window] = NULL;
}

MTY_Size MTY_WindowGetSize(MTY_App *app, MTY_Window window)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return (MTY_Size) {0};

	XWindowAttributes attr = {0};
	XGetWindowAttributes(app->display, ctx->window, &attr);

	return (MTY_Size) {
		.w = attr.width,
		.h = attr.height,
	};
}

static void window_adjust_frame(Display *display, Window window, MTY_Frame *frame)
{
	Atom type;
	int format;
	unsigned char *value = NULL;
	unsigned long bytes, n;

	if (XGetWindowProperty(display, window, XInternAtom(display, "_NET_FRAME_EXTENTS", False),
		0, 4, False, AnyPropertyType, &type, &format, &n, &bytes, &value) == Success)
	{
		if (n == 4) {
			long *extents = (long *) value;
			frame->x -= extents[0]; // Left
			frame->y -= extents[2]; // Top
		}
	}
}

MTY_Frame MTY_WindowGetFrame(MTY_App *app, MTY_Window window)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return (MTY_Frame) {0};

	XWindowAttributes attr = {0};
	XGetWindowAttributes(app->display, ctx->window, &attr);

	int32_t screen = XScreenNumberOfScreen(attr.screen);

	MTY_Frame frame = {0};

	uint8_t wm_state = window_wm_state(app->display, ctx->window);

	if (wm_state != 0) {
		frame = ctx->frame;

		if (wm_state & 0x1)
			frame.type |= MTY_WINDOW_FULLSCREEN;

		if (wm_state & 0x6)
			frame.type |= MTY_WINDOW_MAXIMIZED;

	} else {
		frame.size.w = attr.width;
		frame.size.h = attr.height;

		Window child = None;
		XTranslateCoordinates(app->display, ctx->window, attr.root, 0, 0, &frame.x, &frame.y, &child);
	}

	window_adjust_frame(app->display, ctx->window, &frame);

	snprintf(frame.screen, MTY_SCREEN_MAX, "%d", screen);

	return window_normalize_frame(&frame, app->scale);
}

void MTY_WindowSetFrame(MTY_App *app, MTY_Window window, const MTY_Frame *frame)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return;

	MTY_Frame dframe = window_denormalize_frame(frame, app->scale);

	XWindowAttributes attr = {0};
	XGetWindowAttributes(app->display, ctx->window, &attr);

	// X11 can not programmatically move windows between screens
	if (XScreenNumberOfScreen(attr.screen) != atoi(dframe.screen))
		return;

	XMoveResizeWindow(app->display, ctx->window, dframe.x, dframe.y, dframe.size.w, dframe.size.h);

	if (dframe.type & MTY_WINDOW_MAXIMIZED)
		window_wm_event(app->display, ctx->window, _NET_WM_STATE_ADD,
			"_NET_WM_STATE_MAXIMIZED_HORZ", "_NET_WM_STATE_MAXIMIZED_VERT");

	if (dframe.type & MTY_WINDOW_FULLSCREEN)
		window_wm_event(app->display, ctx->window, _NET_WM_STATE_ADD, "_NET_WM_STATE_FULLSCREEN", NULL);

	XSync(app->display, False);
}

void MTY_WindowSetMinSize(MTY_App *app, MTY_Window window, uint32_t minWidth, uint32_t minHeight)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return;

	XSizeHints *shints = XAllocSizeHints();
	shints->flags = PMinSize;
	shints->min_width = minWidth;
	shints->min_height = minHeight;

	XSetWMProperties(app->display, ctx->window, NULL, NULL, NULL, 0, shints, NULL, NULL);

	XFree(shints);
}

MTY_Size MTY_WindowGetScreenSize(MTY_App *app, MTY_Window window)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return (MTY_Size) {0};

	XWindowAttributes attr = {0};
	XGetWindowAttributes(app->display, ctx->window, &attr);

	return (MTY_Size) {
		.w = XWidthOfScreen(attr.screen),
		.h = XHeightOfScreen(attr.screen),
	};
}

float MTY_WindowGetScreenScale(MTY_App *app, MTY_Window window)
{
	return app->scale;
}

void MTY_WindowSetTitle(MTY_App *app, MTY_Window window, const char *title)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return;

	XStoreName(app->display, ctx->window, title);
}

bool MTY_WindowIsVisible(MTY_App *app, MTY_Window window)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return false;

	XWindowAttributes attr = {0};
	XGetWindowAttributes(app->display, ctx->window, &attr);

	return attr.map_state == IsViewable;
}

bool MTY_WindowIsActive(MTY_App *app, MTY_Window window)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return false;

	Window w = 0;
	int32_t revert = 0;
	XGetInputFocus(app->display, &w, &revert);

	return ctx->window == w;
}

void MTY_WindowActivate(MTY_App *app, MTY_Window window, bool active)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return;

	if (active) {
		XMapRaised(app->display, ctx->window);

		XWindowAttributes attr = {0};
		XGetWindowAttributes(app->display, ctx->window, &attr);

		XEvent evt = {0};
		evt.type = ClientMessage;
		evt.xclient.message_type = XInternAtom(app->display, "_NET_ACTIVE_WINDOW", False);
		evt.xclient.format = 32;
		evt.xclient.window = ctx->window;
		evt.xclient.data.l[0] = 1;
		evt.xclient.data.l[1] = CurrentTime;

		XSendEvent(app->display, XRootWindowOfScreen(attr.screen), 0,
			SubstructureNotifyMask | SubstructureRedirectMask, &evt);

		if (attr.map_state == IsViewable)
			XSetInputFocus(app->display, ctx->window, RevertToNone, CurrentTime);

	} else {
		XWithdrawWindow(app->display, ctx->window);
	}

	XSync(app->display, False);
}

bool MTY_WindowExists(MTY_App *app, MTY_Window window)
{
	return app_get_window(app, window) != NULL;
}

bool MTY_WindowIsFullscreen(MTY_App *app, MTY_Window window)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return false;

	return window_wm_state(app->display, ctx->window) & 0x1;
}

void MTY_WindowSetFullscreen(MTY_App *app, MTY_Window window, bool fullscreen)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return;

	if (fullscreen != MTY_WindowIsFullscreen(app, window))
		window_wm_event(app->display, ctx->window, _NET_WM_STATE_TOGGLE, "_NET_WM_STATE_FULLSCREEN", NULL);
}

void MTY_WindowWarpCursor(MTY_App *app, MTY_Window window, uint32_t x, uint32_t y)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return;

	MTY_AppSetRelativeMouse(app, false);

	XWarpPointer(app->display, None, ctx->window, 0, 0, 0, 0, x, y);
	XSync(app->display, False);
}

MTY_ContextState MTY_WindowGetContextState(MTY_App *app, MTY_Window window)
{
	return MTY_CONTEXT_STATE_NORMAL;
}

void *MTY_WindowGetNative(MTY_App *app, MTY_Window window)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return NULL;

	return (void *) &ctx->info;
}


// App, Window Private

MTY_EventFunc mty_app_get_event_func(MTY_App *app, void **opaque)
{
	*opaque = app->opaque;

	return app->event_func;
}

MTY_Hash *mty_app_get_hotkey_hash(MTY_App *app)
{
	return app->hotkey;
}

struct window_common *mty_window_get_common(MTY_App *app, MTY_Window window)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return NULL;

	return &ctx->cmn;
}


// Misc

static MTY_Atomic32 APP_GLOCK;
static char APP_KEYS[MTY_KEY_MAX][16];

MTY_Frame MTY_MakeDefaultFrame(int32_t x, int32_t y, uint32_t w, uint32_t h, float maxHeight)
{
	Display *display = XOpenDisplay(NULL);
	Screen *screen = XDefaultScreenOfDisplay(display);

	float scale = app_get_scale(display);

	uint32_t screen_h = XHeightOfScreen(screen);
	uint32_t screen_w = XWidthOfScreen(screen);

	XCloseDisplay(display);

	return mty_window_adjust(screen_w, screen_h, scale, maxHeight, x, y, w, h);
}

static void app_hotkey_init(void)
{
	if (MTY_Atomic32Get(&APP_GLOCK) == 0) {
		MTY_GlobalLock(&APP_GLOCK);

		Display *display = XOpenDisplay(NULL);
		XIM xim = XOpenIM(display, 0, 0, 0);
		XIC xic = XCreateIC(xim, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, NULL);

		for (MTY_Key sc = 0; sc < MTY_KEY_MAX; sc++) {
			unsigned int keycode = APP_KEY_MAP[sc];
			if (keycode == 0)
				continue;

			XKeyPressedEvent evt = {0};
			evt.type = KeyPress;
			evt.display = display;
			evt.keycode = keycode;

			KeySym sym = XLookupKeysym(&evt, 0);
			if (sym == NoSymbol)
				continue;

			KeySym lsym = sym, usym = sym;
			XConvertCase(sym, &lsym, &usym);

			char utf8_str[16] = {0};
			bool lookup_ok = false;

			// FIXME symbols >= 0x80 seem to crash Xutf8LookupString -- why?
			if (sym < 0x80) {
				evt.state = sym != usym ? ShiftMask : 0;

				KeySym ignore;
				Status status;
				lookup_ok = Xutf8LookupString(xic, &evt, utf8_str, 16, &ignore, &status) > 0;
			}

			if (!lookup_ok) {
				const char *sym_str = XKeysymToString(usym);
				if (sym_str)
					snprintf(utf8_str, 16, "%s", sym_str);
			}

			snprintf(APP_KEYS[sc], 16, "%s", utf8_str);
		}

		XDestroyIC(xic);
		XCloseIM(xim);
		XCloseDisplay(display);

		MTY_GlobalUnlock(&APP_GLOCK);
	}
}

void MTY_HotkeyToString(MTY_Mod mod, MTY_Key key, char *str, size_t len)
{
	memset(str, 0, len);

	if (!libX11_global_init())
		return;

	app_hotkey_init();

	MTY_Strcat(str, len, (mod & MTY_MOD_WIN) ? "Super+" : "");
	MTY_Strcat(str, len, (mod & MTY_MOD_CTRL) ? "Ctrl+" : "");
	MTY_Strcat(str, len, (mod & MTY_MOD_ALT) ? "Alt+" : "");
	MTY_Strcat(str, len, (mod & MTY_MOD_SHIFT) ? "Shift+" : "");

	MTY_Strcat(str, len, APP_KEYS[key]);
}

void MTY_SetAppID(const char *id)
{
}

void *MTY_GLGetProcAddress(const char *name)
{
	if (!libX11_global_init())
		return NULL;

	return glXGetProcAddress((const GLubyte *) name);
}

void MTY_RunAndYield(MTY_IterFunc iter, void *opaque)
{
	while (iter(opaque));
}
