// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "app.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <limits.h>

#include <unistd.h>

#include "dl/libX11.h"
#include "hid/utils.h"
#include "wsize.h"
#include "evdev.h"
#include "keymap.h"

struct window {
	Window window;
	MTY_Window index;
	XIC ic;
	MTY_GFX api;
	struct gfx_ctx *gfx_ctx;
	struct xinfo info;
};

struct MTY_App {
	Display *display;
	Cursor empty_cursor;
	Cursor custom_cursor;
	Cursor cursor;
	bool default_cursor;
	bool hide_cursor;
	char *class_name;
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
	uint32_t rate;
	MTY_Time suspend_ts;
	bool relative;
	bool suspend_ss;
	bool mgrab;
	bool kbgrab;
	void *opaque;
	char *clip;
	float scale;

	int32_t last_width;
	int32_t last_height;
	int32_t last_pos_x;
	int32_t last_pos_y;

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

	return 0;
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
		XSetSelectionOwner(ctx->display, XInternAtom(ctx->display, "CLIPBOARD", False), win0->window, CurrentTime);

		MTY_Event evt = {0};
		evt.type = MTY_EVENT_CLIPBOARD;
		ctx->event_func(&evt, ctx->opaque);
	}
}

static void app_poll_clipboard(MTY_App *ctx)
{
	Atom clip = XInternAtom(ctx->display, "CLIPBOARD", False);

	Window sel_owner = XGetSelectionOwner(ctx->display, clip);
	if (sel_owner != ctx->sel_owner) {
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
	Cursor cur = focus && (app->hide_cursor || (app->relative && app->detach == MTY_DETACH_STATE_NONE)) ?
		app->empty_cursor : app->custom_cursor && !app->default_cursor ?
		app->custom_cursor : None;

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
		ctx->event_func(&evt, ctx->opaque);
}

static void app_kb_to_hotkey(MTY_App *app, MTY_Event *evt)
{
	MTY_Mod mod = evt->key.mod & 0xFF;
	uint32_t hotkey = (uint32_t) (uintptr_t) MTY_HashGetInt(app->hotkey, (mod << 16) | evt->key.key);

	if (hotkey != 0) {
		if (evt->key.pressed) {
			evt->type = MTY_EVENT_HOTKEY;
			evt->hotkey = hotkey;

		} else {
			evt->type = MTY_EVENT_NONE;
		}
	}
}

static void app_refresh_scale(MTY_App *ctx)
{
	Display *display = XOpenDisplay(NULL);

	const char *dpi = XGetDefault(display, "Xft", "dpi");
	ctx->scale = dpi ? (float) atoi(dpi) / 96.0f : 1.0f;

	if (ctx->scale == 0.0f)
		ctx->scale = 1.0f;

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
		case ConfigureNotify:
			if (ctx->last_width != event->xconfigure.width || ctx->last_height != event->xconfigure.height) {
				evt.type = MTY_EVENT_SIZE;
				ctx->last_width = event->xconfigure.width;
				ctx->last_height = event->xconfigure.height;
			}

			if (ctx->last_pos_x != event->xconfigure.x || ctx->last_pos_y != event->xconfigure.y) {
				evt.type = MTY_EVENT_MOVE;
				ctx->last_pos_x = event->xconfigure.x;
				ctx->last_pos_y = event->xconfigure.y;
			}
			break;
		case FocusIn:
		case FocusOut:
			// Do not respond to NotifyGrab or NotifyUngrab
			if (event->xfocus.mode == NotifyNormal || event->xfocus.mode == NotifyWhileGrabbed) {
				evt.type = MTY_EVENT_FOCUS;
				evt.focus = event->type == FocusIn;
			}
			ctx->state++;
			break;
	}

	// Transform keyboard into hotkey
	if (evt.type == MTY_EVENT_KEY)
		app_kb_to_hotkey(ctx, &evt);

	// Handle the message
	if (evt.type != MTY_EVENT_NONE)
		ctx->event_func(&evt, ctx->opaque);
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

MTY_App *MTY_AppCreate(MTY_AppFunc appFunc, MTY_EventFunc eventFunc, void *opaque)
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
		// Grab / mouse state evaluation
		if (ctx->state != ctx->prev_state) {
			struct window *win = app_get_active_window(ctx);

			app_apply_keyboard_grab(ctx, win);
			app_apply_mouse_grab(ctx, win);
			app_apply_cursor(ctx, win ? true : false);
			XSync(ctx->display, False);

			ctx->prev_state = ctx->state;
		}

		// Poll selection ownership changes
		app_poll_clipboard(ctx);

		// X11 events
		for (XEvent event; XEventsQueued(ctx->display, QueuedAfterFlush) > 0;) {
			XNextEvent(ctx->display, &event);
			app_event(ctx, &event);
		}

		// evdev events
		if (ctx->evdev)
			mty_evdev_poll(ctx->evdev, app_evdev_report);

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
	return app_get_active_window(ctx) ? true : false;
}

void MTY_AppActivate(MTY_App *ctx, bool active)
{
	MTY_WindowActivate(ctx, 0, active);
}

MTY_Frame MTY_AppTransformFrame(MTY_App *ctx, bool center, float maxHeight, const MTY_Frame *frame)
{
	MTY_Frame tframe = *frame;

	Window root = XDefaultRootWindow(ctx->display);

	XWindowAttributes attr = {0};
	XGetWindowAttributes(ctx->display, root, &attr);

	uint32_t screen_h = XHeightOfScreen(attr.screen);

	if (maxHeight > 0.0f)
		wsize_max_height(ctx->scale, maxHeight, screen_h, &tframe.size);

	if (center) {
		uint32_t screen_w = XWidthOfScreen(attr.screen);
		wsize_center(0, 0, screen_w, screen_h, &tframe);
	}

	return tframe;
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

	char *str = MTY_Strdup(ctx->clip);

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
		app_apply_cursor(ctx, win ? true : false);
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

void MTY_AppUseDefaultCursor(MTY_App *ctx, bool useDefault)
{
	if (ctx->default_cursor != useDefault) {
		ctx->default_cursor = useDefault;
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

void MTY_AppGrabKeyboard(MTY_App *ctx, bool grab)
{
	if (ctx->kbgrab != grab) {
		ctx->kbgrab = grab;
		ctx->state++;
	}
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

const void *MTY_AppGetControllerTouchpad(MTY_App *ctx, uint32_t id, size_t *size)
{
	return NULL;
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

	MTY_Frame dframe = {
		.size.w = APP_DEFAULT_WINDOW_W,
		.size.h = APP_DEFAULT_WINDOW_H,
	};

	if (!frame) {
		dframe = MTY_AppTransformFrame(app, true, 0.0f, &dframe);
		frame = &dframe;
	}

	Window root = XDefaultRootWindow(app->display);

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

MTY_Frame MTY_WindowGetPlacement(MTY_App *app, MTY_Window window)
{
	// TODO FIXME
	return (MTY_Frame) {
		.size = MTY_WindowGetSize(app, window),
	};
}

void MTY_WindowSetFrame(MTY_App *app, MTY_Window window, const MTY_Frame *frame)
{
	// TODO FIXME
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

uint32_t MTY_WindowGetRefreshRate(MTY_App *app, MTY_Window window)
{
	// TODO This will cache the rate after the first call to this function because
	// XRRGetScreenInfo is very slow. This means it will not respond to rate changes
	// during runtime. A better way of doing this would be to use XRRSelectInput
	// with RRScreenChangeNotifyMask and fetch the new refresh rate in response to
	// the event.

	if (app->rate > 0)
		return app->rate;

	struct window *ctx = app_get_window(app, window);

	if (ctx && XRRGetScreenInfo && XRRFreeScreenConfigInfo && XRRConfigCurrentRate) {
		XRRScreenConfiguration *conf = XRRGetScreenInfo(app->display, ctx->window);

		if (conf) {
			app->rate = XRRConfigCurrentRate(conf);
			XRRFreeScreenConfigInfo(conf);

			return app->rate;
		}
	}

	return 60;
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

		XSetInputFocus(app->display, ctx->window, RevertToNone, CurrentTime);

		XSync(app->display, False);

	} else {
		XWithdrawWindow(app->display, ctx->window);
	}

	XSync(app->display, False);
}

bool MTY_WindowExists(MTY_App *app, MTY_Window window)
{
	return app_get_window(app, window) ? true : false;
}

bool MTY_WindowIsFullscreen(MTY_App *app, MTY_Window window)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return false;

	Atom type;
	int format;
	unsigned char *value = NULL;
	unsigned long bytes, n;

	if (XGetWindowProperty(app->display, ctx->window, XInternAtom(app->display, "_NET_WM_STATE", False),
		0, 1024, False, XA_ATOM, &type, &format, &n, &bytes, &value) == Success)
	{
		Atom *atoms = (Atom *) value;

		for (unsigned long x = 0; x < n; x++)
			if (atoms[x] == XInternAtom(app->display, "_NET_WM_STATE_FULLSCREEN", False))
				return true;
	}

	return false;
}

void MTY_WindowSetFullscreen(MTY_App *app, MTY_Window window, bool fullscreen)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return;

	if (fullscreen != MTY_WindowIsFullscreen(app, window)) {
		XWindowAttributes attr = {0};
		XGetWindowAttributes(app->display, ctx->window, &attr);

		XEvent evt = {0};
		evt.type = ClientMessage;
		evt.xclient.message_type = XInternAtom(app->display, "_NET_WM_STATE", False);
		evt.xclient.format = 32;
		evt.xclient.window = ctx->window;
		evt.xclient.data.l[0] = _NET_WM_STATE_TOGGLE;
		evt.xclient.data.l[1] = XInternAtom(app->display, "_NET_WM_STATE_FULLSCREEN", False);

		XSendEvent(app->display, XRootWindowOfScreen(attr.screen), 0,
			SubstructureNotifyMask | SubstructureRedirectMask, &evt);

		XSync(app->display, False);
	}
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


// Window Private

void mty_window_set_gfx(MTY_App *app, MTY_Window window, MTY_GFX api, struct gfx_ctx *gfx_ctx)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return;

	ctx->api = api;
	ctx->gfx_ctx = gfx_ctx;
}

MTY_GFX mty_window_get_gfx(MTY_App *app, MTY_Window window, struct gfx_ctx **gfx_ctx)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return MTY_GFX_NONE;

	if (gfx_ctx)
		*gfx_ctx = ctx->gfx_ctx;

	return ctx->api;
}

void *mty_window_get_native(MTY_App *app, MTY_Window window)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return NULL;

	return (void *) &ctx->info;
}


// Misc

static MTY_Atomic32 APP_GLOCK;
static char APP_KEYS[MTY_KEY_MAX][16];

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
