// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "app.h"

#include <stdio.h>
#include <math.h>

#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <shellscalingapi.h>
#include <dbt.h>

#include "xip.h"
#include "hid/hid.h"

#define APP_CLASS_NAME L"MTY_Window"
#define APP_RI_MAX     (32 * 1024)

struct window {
	struct window_common cmn;
	MTY_App *app;
	MTY_Window window;
	MTY_Frame frame;
	HWND hwnd;
	bool was_zoomed;
	uint32_t min_width;
	uint32_t min_height;
	RAWINPUT *ri;
};

struct MTY_App {
	MTY_AppFunc app_func;
	MTY_EventFunc event_func;
	MTY_WMsgFunc wmsg_func;
	void *opaque;

	WNDCLASSEX wc;
	ATOM class;
	UINT tb_msg;
	RECT clip;
	HICON cursor;
	HICON custom_cursor;
	HINSTANCE instance;
	HHOOK kbhook;
	DWORD cb_seq;
	bool pen_in_range;
	bool pen_enabled;
	bool touch_active;
	bool relative;
	bool kbgrab;
	bool mgrab;
	bool hide_cursor;
	bool ghk_disabled;
	bool filter_move;
	uint64_t prev_state;
	uint64_t state;
	uint32_t timeout;
	int32_t last_x;
	int32_t last_y;
	struct hid *hid;
	struct xip *xip;
	MTY_AppFlag flags;
	MTY_Cursor scursor;
	MTY_Button buttons;
	MTY_DetachState detach;
	MTY_Hash *hotkey;
	MTY_Hash *ghotkey;
	MTY_Hash *deduper;

	struct window *windows[MTY_WINDOW_MAX];

	struct {
		HMENU menu;
		NOTIFYICONDATA nid;
		MTY_Time ts;
		MTY_Time rctimer;
		bool init;
		bool want;
		bool menu_open;

		struct menu_item {
			wchar_t *label;
			uint32_t trayID;
			MTY_MenuItemCheckedFunc checked;
		} *items;

		uint32_t len;
	} tray;

	BOOL (WINAPI *AdjustWindowRectExForDpi)(LPRECT lpRect, DWORD dwStyle, BOOL bMenu,
		DWORD dwExStyle, UINT dpi);
};


// MTY -> VK mouse map

static const int APP_MOUSE_MAP[] = {
	[MTY_BUTTON_LEFT]   = VK_LBUTTON,
	[MTY_BUTTON_RIGHT]  = VK_RBUTTON,
	[MTY_BUTTON_MIDDLE] = VK_MBUTTON,
	[MTY_BUTTON_X1]     = VK_XBUTTON1,
	[MTY_BUTTON_X2]     = VK_XBUTTON2,
};

#define APP_MOUSE_MAX (sizeof(APP_MOUSE_MAP) / sizeof(int))


// Low level keyboard hook needs a global HWND

static HWND APP_KB_HWND;
static MTY_Mod APP_KB_LWIN;
static MTY_Mod APP_KB_RWIN;


// App Static Helpers

static HWND app_get_main_hwnd(MTY_App *app)
{
	if (!app->windows[0])
		return NULL;

	return app->windows[0]->hwnd;
}

static struct window *app_get_main_window(MTY_App *app)
{
	return app->windows[0];
}

static struct window *app_get_window(MTY_App *app, MTY_Window window)
{
	return window < 0 ? NULL : app->windows[window];
}

static struct window *app_get_focus_window(MTY_App *app)
{
	for (MTY_Window x = 0; x < MTY_WINDOW_MAX; x++)
		if (MTY_WindowIsActive(app, x))
			return app_get_window(app, x);

	return NULL;
}

static MTY_Window app_find_open_window(MTY_App *app, MTY_Window req)
{
	if (req >= 0 && req < MTY_WINDOW_MAX && !app->windows[req])
		return req;

	for (MTY_Window x = 0; x < MTY_WINDOW_MAX; x++)
		if (!app->windows[x])
			return x;

	return -1;
}

static bool app_hwnd_visible(HWND hwnd)
{
	return IsWindowVisible(hwnd) && !IsIconic(hwnd);
}

static bool app_hwnd_active(HWND hwnd)
{
	return GetForegroundWindow() == hwnd && app_hwnd_visible(hwnd);
}

static void app_adjust_window_rect(MTY_App *app, float scale, RECT *r)
{
	if (app->AdjustWindowRectExForDpi) {
		app->AdjustWindowRectExForDpi(r, WS_OVERLAPPEDWINDOW, FALSE, 0, lrint(scale * 96));

	} else {
		AdjustWindowRect(r, WS_OVERLAPPEDWINDOW, FALSE);
	}
}


// Tray messages

#define APP_TRAY_CALLBACK (WM_USER + 510)
#define APP_TRAY_UID      1337
#define APP_TRAY_FIRST    1000

static HMENU app_tray_menu(struct menu_item *items, uint32_t len, void *opaque)
{
	HMENU menu = CreatePopupMenu();

	for (uint32_t x = 0; x < len; x++) {
		if (items[x].label[0]) {
			MENUITEMINFO item = {0};
			item.cbSize = sizeof(MENUITEMINFO);
			item.fMask = MIIM_ID | MIIM_TYPE | MIIM_STATE | MIIM_DATA;
			item.wID = APP_TRAY_FIRST + x;
			item.dwTypeData = items[x].label;
			item.dwItemData = (ULONG_PTR) items[x].trayID;

			if (items[x].checked && items[x].checked(opaque))
				item.fState |= MFS_CHECKED;

			InsertMenuItem(menu, APP_TRAY_FIRST + x, TRUE, &item);

		} else {
			InsertMenu(menu, APP_TRAY_FIRST + x, MF_SEPARATOR, TRUE, L"");
		}
	}

	return menu;
}

static bool app_tray_get_location(HWND hwnd, uint32_t *x, uint32_t *y)
{
	NOTIFYICONIDENTIFIER id = {0};
	id.cbSize = sizeof(NOTIFYICONIDENTIFIER);
	id.hWnd = hwnd;
	id.uID = APP_TRAY_UID;
	id.guidItem = GUID_NULL;

	RECT r = {0};
	if (Shell_NotifyIconGetRect(&id, &r) == S_OK) {
		if (x)
			*x = r.left + (r.right - r.left) / 2;

		if (y)
			*y = r.top + (r.bottom - r.top) / 2;

		return true;
	}

	return false;
}

static void app_tray_set_nid(NOTIFYICONDATA *nid, UINT uflags, HWND hwnd, HICON smicon, HICON lgicon)
{
	nid->cbSize = sizeof(NOTIFYICONDATA);
	nid->hWnd = hwnd;
	nid->uID = APP_TRAY_UID;
	nid->uFlags = NIF_MESSAGE | uflags;
	nid->uCallbackMessage = APP_TRAY_CALLBACK;
	nid->hIcon = smicon;
	nid->hBalloonIcon = lgicon;
	nid->dwInfoFlags = NIIF_USER | NIIF_LARGE_ICON;

	memset(nid->szInfoTitle, 0, sizeof(nid->szInfoTitle));
	memset(nid->szInfo, 0, sizeof(nid->szInfo));
}

static void app_tray_recreate(MTY_App *app, struct window *ctx)
{
	app_tray_set_nid(&app->tray.nid, NIF_TIP | NIF_ICON, ctx->hwnd, app->wc.hIconSm, app->wc.hIcon);

	if (app->tray.init)
		Shell_NotifyIcon(NIM_DELETE, &app->tray.nid);

	app->tray.init = Shell_NotifyIcon(NIM_ADD, &app->tray.nid);
}

static void app_tray_retry(MTY_App *app, struct window *ctx)
{
	if (!app->tray.init && app->tray.want) {
		MTY_Time now = MTY_GetTime();

		if (MTY_TimeDiff(app->tray.rctimer, now) > 2000.0f) {
			app_tray_recreate(app, ctx);
			app->tray.rctimer = now;
		}
	}
}

static void app_tray_msg(MTY_App *app, UINT msg, WPARAM wparam, LPARAM lparam, MTY_Event *evt)
{
	struct window *ctx = app_get_main_window(app);
	if (!ctx)
		return;

	// Tray interaction
	if (msg == APP_TRAY_CALLBACK) {
		switch (lparam) {
			case NIN_BALLOONUSERCLICK:
				MTY_AppActivate(app, true);
				break;
			case WM_LBUTTONUP:
				MTY_Time now = MTY_GetTime();

				if (MTY_TimeDiff(app->tray.ts, now) > GetDoubleClickTime() * 2) {
					MTY_AppActivate(app, true);
					app->tray.ts = now;
				}
				break;
			case WM_RBUTTONUP:
				if (app->tray.menu)
					DestroyMenu(app->tray.menu);

				app->tray.menu = app_tray_menu(app->tray.items, app->tray.len, app->opaque);

				if (app->tray.menu) {
					SetForegroundWindow(ctx->hwnd);
					SendMessage(ctx->hwnd, WM_INITMENUPOPUP, (WPARAM) app->tray.menu, 0);

					uint32_t x = 0;
					uint32_t y = 0;
					if (app_tray_get_location(ctx->hwnd, &x, &y)) {
						app->tray.menu_open = true;
						WORD cmd = (WORD) TrackPopupMenu(app->tray.menu, TPM_LEFTALIGN | TPM_RIGHTBUTTON |
							TPM_RETURNCMD | TPM_NONOTIFY, x, y, 0, ctx->hwnd, NULL);

						app->tray.menu_open = false;
						SendMessage(ctx->hwnd, WM_COMMAND, cmd, 0);
						PostMessage(ctx->hwnd, WM_NULL, 0, 0);
					}
				}
				break;
		}

	// Recreation on taskbar recreation (DPI changes)
	} else if (msg == app->tb_msg) {
		if (app->tray.init)
			app_tray_recreate(app, ctx);

	// Menu command
	} else if (msg == WM_COMMAND && wparam >= APP_TRAY_FIRST && app->tray.menu) {
		MENUITEMINFO item = {0};
		item.cbSize = sizeof(MENUITEMINFO);
		item.fMask = MIIM_ID | MIIM_DATA;

		if (GetMenuItemInfo(app->tray.menu, (UINT) wparam, FALSE, &item)) {
			evt->type = MTY_EVENT_TRAY;
			evt->trayID = (uint32_t) item.dwItemData;
		}
	}
}


// Message processing

static LRESULT CALLBACK app_hwnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

static void app_set_button_coords(HWND hwnd, MTY_Event *evt)
{
	POINT p = {0};
	GetCursorPos(&p);
	ScreenToClient(hwnd, &p);

	evt->button.x = p.x;
	evt->button.y = p.y;
}

static void app_ri_relative_mouse(MTY_App *app, HWND hwnd, const RAWINPUT *ri, MTY_Event *evt)
{
	const RAWMOUSE *mouse = &ri->data.mouse;

	if (mouse->lLastX != 0 || mouse->lLastY != 0) {
		if (mouse->usFlags & MOUSE_MOVE_ABSOLUTE) {
			int32_t x = mouse->lLastX;
			int32_t y = mouse->lLastY;

			// It seems that touch input reports lastX and lastY as screen coordinates,
			// not normalized coordinates between 0-65535 as the documentation says
			if (!app->touch_active) {
				bool virt = (mouse->usFlags & MOUSE_VIRTUAL_DESKTOP) == MOUSE_VIRTUAL_DESKTOP;
				int32_t w = GetSystemMetrics(virt ? SM_CXVIRTUALSCREEN : SM_CXSCREEN);
				int32_t h = GetSystemMetrics(virt ? SM_CYVIRTUALSCREEN : SM_CYSCREEN);
				x = (int32_t) (((float) mouse->lLastX / 65535.0f) * w);
				y = (int32_t) (((float) mouse->lLastY / 65535.0f) * h);
			}

			if (app->last_x != -1 && app->last_y != -1) {
				evt->type = MTY_EVENT_MOTION;
				evt->motion.relative = true;
				evt->motion.synth = true;
				evt->motion.x = (int32_t) (x - app->last_x);
				evt->motion.y = (int32_t) (y - app->last_y);
			}

			app->last_x = x;
			app->last_y = y;

		} else {
			evt->type = MTY_EVENT_MOTION;
			evt->motion.relative = true;
			evt->motion.synth = false;
			evt->motion.x = mouse->lLastX;
			evt->motion.y = mouse->lLastY;
		}
	}

	ULONG b = mouse->usButtonFlags;

	if (b & RI_MOUSE_LEFT_BUTTON_DOWN)   app_hwnd_proc(hwnd, WM_LBUTTONDOWN, 0, 0);
	if (b & RI_MOUSE_LEFT_BUTTON_UP)     app_hwnd_proc(hwnd, WM_LBUTTONUP, 0, 0);
	if (b & RI_MOUSE_MIDDLE_BUTTON_DOWN) app_hwnd_proc(hwnd, WM_MBUTTONDOWN, 0, 0);
	if (b & RI_MOUSE_MIDDLE_BUTTON_UP)   app_hwnd_proc(hwnd, WM_MBUTTONUP, 0, 0);
	if (b & RI_MOUSE_RIGHT_BUTTON_DOWN)  app_hwnd_proc(hwnd, WM_RBUTTONDOWN, 0, 0);
	if (b & RI_MOUSE_RIGHT_BUTTON_UP)    app_hwnd_proc(hwnd, WM_RBUTTONUP, 0, 0);
	if (b & RI_MOUSE_BUTTON_4_DOWN)      app_hwnd_proc(hwnd, WM_XBUTTONDOWN, XBUTTON1 << 16, 0);
	if (b & RI_MOUSE_BUTTON_4_UP)        app_hwnd_proc(hwnd, WM_XBUTTONUP, XBUTTON1 << 16, 0);
	if (b & RI_MOUSE_BUTTON_5_DOWN)      app_hwnd_proc(hwnd, WM_XBUTTONDOWN, XBUTTON2 << 16, 0);
	if (b & RI_MOUSE_BUTTON_5_UP)        app_hwnd_proc(hwnd, WM_XBUTTONUP, XBUTTON2 << 16, 0);
	if (b & RI_MOUSE_WHEEL)              app_hwnd_proc(hwnd, WM_MOUSEWHEEL, mouse->usButtonData << 16, 0);
	if (b & RI_MOUSE_HWHEEL)             app_hwnd_proc(hwnd, WM_MOUSEHWHEEL, mouse->usButtonData << 16, 0);
}

static MTY_Mod app_get_keymod(void)
{
	return
		((GetKeyState(VK_LSHIFT)   & 0x8000) >> 15) |
		((GetKeyState(VK_RSHIFT)   & 0x8000) >> 14) |
		((GetKeyState(VK_LCONTROL) & 0x8000) >> 13) |
		((GetKeyState(VK_RCONTROL) & 0x8000) >> 12) |
		((GetKeyState(VK_LMENU)    & 0x8000) >> 11) |
		((GetKeyState(VK_RMENU)    & 0x8000) >> 10) |
		((GetKeyState(VK_LWIN)     & 0x8000) >> 9)  |
		((GetKeyState(VK_RWIN)     & 0x8000) >> 8)  |
		((GetKeyState(VK_CAPITAL)  & 0x0001) << 8)  |
		((GetKeyState(VK_NUMLOCK)  & 0x0001) << 9)  |
		APP_KB_LWIN | APP_KB_RWIN;
}

static LRESULT CALLBACK app_ll_keyboard_proc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode == HC_ACTION && APP_KB_HWND) {
		const KBDLLHOOKSTRUCT *p = (const KBDLLHOOKSTRUCT *) lParam;

		bool intercept = ((p->flags & LLKHF_ALTDOWN) && p->vkCode == VK_TAB) || // ALT + TAB
			(p->vkCode == VK_LWIN || p->vkCode == VK_RWIN) || // Windows Key
			(p->vkCode == VK_APPS) || // Application Key
			(p->vkCode == VK_ESCAPE); // ESC

		if (intercept) {
			bool up = p->flags & LLKHF_UP;

			// Windows low level keyboard hook interferes with the Win key modifier state, track globally
			if (p->vkCode == VK_LWIN)
				APP_KB_LWIN = up ? MTY_MOD_NONE : MTY_MOD_LWIN;

			if (p->vkCode == VK_RWIN)
				APP_KB_RWIN = up ? MTY_MOD_NONE : MTY_MOD_RWIN;

			LPARAM wproc_lparam = 0;
			wproc_lparam |= (p->scanCode & 0xFF) << 16;
			wproc_lparam |= (p->flags & LLKHF_EXTENDED) ? (1 << 24) : 0;
			wproc_lparam |= up ? (1 << 31) : 0;

			SendMessage(APP_KB_HWND, (UINT) wParam, 0, wproc_lparam);
			return 1;
		}
	}

	return CallNextHookEx(NULL, nCode, wParam, lParam);
}

static void app_apply_clip(MTY_App *app, bool focus)
{
	if (focus) {
		if (app->relative && app->detach != MTY_DETACH_STATE_FULL) {
			ClipCursor(&app->clip);

		} else if (app->mgrab && app->detach == MTY_DETACH_STATE_NONE) {
			struct window *ctx = app_get_focus_window(app);
			if (ctx) {
				RECT r = {0};
				if (GetClientRect(ctx->hwnd, &r)) {
					ClientToScreen(ctx->hwnd, (POINT *) &r.left);
					ClientToScreen(ctx->hwnd, (POINT *) &r.right);
					ClipCursor(&r);
				}
			}
		} else {
			ClipCursor(NULL);
		}
	} else {
		ClipCursor(NULL);
	}
}

static void app_apply_cursor(MTY_App *app, bool focus)
{
	if (focus && (app->hide_cursor || (app->relative && app->detach == MTY_DETACH_STATE_NONE))) {
		app->cursor = NULL;

	} else {
		const WCHAR *scursor = NULL;

		switch (app->scursor) {
			case MTY_CURSOR_ARROW: scursor = IDC_ARROW; break;
			case MTY_CURSOR_HAND:  scursor = IDC_HAND;  break;
			case MTY_CURSOR_IBEAM: scursor = IDC_IBEAM; break;
		}

		app->cursor = scursor ? LoadCursor(NULL, scursor) : app->custom_cursor ? app->custom_cursor :
			LoadCursor(NULL, IDC_ARROW);
	}

	if (GetCursor() != app->cursor)
		SetCursor(app->cursor);
}

static void app_apply_mouse_ri(MTY_App *app, bool focus)
{
	RAWINPUTDEVICE rid = {
		.usUsagePage = 0x01,
		.usUsage = 0x02,
	};

	if (app->relative && !app->pen_in_range && focus && app->detach != MTY_DETACH_STATE_FULL) {
		rid.dwFlags = RIDEV_NOLEGACY;

	} else {
		// Exiting raw input generates a single WM_MOUSEMOVE, filter it
		app->filter_move = true;
		rid.dwFlags = RIDEV_REMOVE;
	}

	if (!RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE)))
		MTY_Log("'RegisterRawInputDevices' failed with error 0x%X", GetLastError());
}

static void app_apply_keyboard_state(MTY_App *app, bool focus)
{
	if (focus && app->kbgrab && app->detach == MTY_DETACH_STATE_NONE) {
		if (!app->kbhook) {
			struct window *ctx = app_get_focus_window(app);
			if (ctx) {
				APP_KB_HWND = ctx->hwnd; // Unfortunately this needs to be global
				app->kbhook = SetWindowsHookEx(WH_KEYBOARD_LL, app_ll_keyboard_proc, app->instance, 0);
				if (!app->kbhook) {
					APP_KB_HWND = NULL;
					MTY_Log("'SetWindowsHookEx' failed with error 0x%X", GetLastError());
				}
			}
		}

	} else {
		if (app->kbhook) {
			if (!UnhookWindowsHookEx(app->kbhook))
				MTY_Log("'UnhookWindowsHookEx' failed with error 0x%X", GetLastError());

			APP_KB_HWND = NULL;
			app->kbhook = NULL;

			APP_KB_LWIN = APP_KB_RWIN = MTY_MOD_NONE;
		}
	}
}

static bool app_button_is_pressed(MTY_Button button)
{
	return (GetAsyncKeyState(APP_MOUSE_MAP[button]) & 0x8000) == 0x8000;
}

static void app_fix_mouse_buttons(MTY_App *ctx)
{
	if (ctx->buttons == 0)
		return;

	bool flipped = GetSystemMetrics(SM_SWAPBUTTON);
	MTY_Button buttons = ctx->buttons;

	for (MTY_Button x = 0; buttons > 0 && x < APP_MOUSE_MAX; x++) {
		if (buttons & 1) {
			MTY_Button b = flipped && x == MTY_BUTTON_LEFT ? MTY_BUTTON_RIGHT :
				flipped && x == MTY_BUTTON_RIGHT ? MTY_BUTTON_LEFT : x;

			if (!app_button_is_pressed(b)) {
				MTY_Event evt = {0};
				evt.type = MTY_EVENT_BUTTON;
				evt.button.button = x;

				ctx->event_func(&evt, ctx->opaque);
				ctx->buttons &= ~(1 << x);
			}
		}

		buttons >>= 1;
	}
}

static LRESULT app_custom_hwnd_proc(struct window *ctx, HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	MTY_App *app = ctx->app;

	LRESULT r = 0;
	bool creturn = false;

	// Custom window message handler
	if (app->wmsg_func) {
		r = app->wmsg_func(app, ctx->window, hwnd, msg, wparam, lparam, &creturn, app->opaque);

		if (creturn)
			return r;
	}

	MTY_Event evt = {0};
	evt.window = ctx->window;

	bool defreturn = false;
	bool pen_active = app->pen_enabled && app->pen_in_range;
	char drop_name[MTY_PATH_MAX];

	switch (msg) {
		case WM_CLOSE:
			evt.type = MTY_EVENT_CLOSE;
			break;
		case WM_SIZE:
			app->state++;
			evt.type = MTY_EVENT_SIZE;
			break;
		case WM_MOVE:
			evt.type = MTY_EVENT_MOVE;
			break;
		case WM_SETCURSOR:
			if (LOWORD(lparam) == HTCLIENT) {
				app_apply_cursor(app, MTY_AppIsActive(app));
				creturn = true;
				r = TRUE;
			}
			break;
		case WM_DISPLAYCHANGE: {
			// A display change can bork DXGI ResizeBuffers, forcing a dummy 1px move puts it back to normal
			RECT wrect = {0};
			GetWindowRect(hwnd, &wrect);
			SetWindowPos(hwnd, 0, wrect.left + 1, wrect.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
			SetWindowPos(hwnd, 0, wrect.left, wrect.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
			break;
		}
		case WM_SETFOCUS:
		case WM_KILLFOCUS:
			evt.type = MTY_EVENT_FOCUS;
			evt.focus = msg == WM_SETFOCUS;
			app->state++;
			break;
		case WM_QUERYENDSESSION:
		case WM_ENDSESSION:
			evt.type = MTY_EVENT_SHUTDOWN;
			break;
		case WM_GETMINMAXINFO: {
			MINMAXINFO *info = (MINMAXINFO *) lparam;
			info->ptMinTrackSize.x = ctx->min_width;
			info->ptMinTrackSize.y = ctx->min_height;
			creturn = true;
			r = 0;
			break;
		}
		case WM_KEYUP:
		case WM_KEYDOWN:
		case WM_SYSKEYUP:
		case WM_SYSKEYDOWN:
			evt.type = MTY_EVENT_KEY;
			evt.key.mod = app_get_keymod();
			evt.key.pressed = !(lparam >> 31);
			evt.key.key = lparam >> 16 & 0xFF;
			evt.key.vkey = (uint32_t) wparam;
			if (lparam >> 24 & 0x01)
				evt.key.key |= 0x0100;

			// Print Screen needs a synthesized WM_KEYDOWN
			if (!evt.key.pressed && evt.key.key == MTY_KEY_PRINT_SCREEN)
				app_custom_hwnd_proc(ctx, hwnd, WM_KEYDOWN, wparam, lparam & 0x7FFFFFFF);
			break;
		case WM_MOUSEMOVE:
			if (!app->filter_move && !pen_active && (!app->relative || app_hwnd_active(hwnd))) {
				evt.type = MTY_EVENT_MOTION;
				evt.motion.relative = false;
				evt.motion.synth = false;
				evt.motion.x = GET_X_LPARAM(lparam);
				evt.motion.y = GET_Y_LPARAM(lparam);
			}

			app->filter_move = false;
			break;
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
			if (!pen_active) {
				evt.type = MTY_EVENT_BUTTON;
				evt.button.button = MTY_BUTTON_LEFT;
				evt.button.pressed = msg == WM_LBUTTONDOWN;
			}
			break;
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
			// When the pen is active, this will fire for the barrel button
			evt.type = MTY_EVENT_BUTTON;
			evt.button.button = MTY_BUTTON_RIGHT;
			evt.button.pressed = msg == WM_RBUTTONDOWN;
			break;
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
			evt.type = MTY_EVENT_BUTTON;
			evt.button.button = MTY_BUTTON_MIDDLE;
			evt.button.pressed = msg == WM_MBUTTONDOWN;
			break;
		case WM_XBUTTONDOWN:
		case WM_XBUTTONUP: {
			UINT button = GET_XBUTTON_WPARAM(wparam);
			evt.type = MTY_EVENT_BUTTON;
			evt.button.button = button == XBUTTON1 ? MTY_BUTTON_X1 : MTY_BUTTON_X2;
			evt.button.pressed = msg == WM_XBUTTONDOWN;
			break;
		}
		case WM_MOUSEWHEEL:
			evt.type = MTY_EVENT_SCROLL;
			evt.scroll.x = 0;
			evt.scroll.y = GET_WHEEL_DELTA_WPARAM(wparam);
			break;
		case WM_MOUSEHWHEEL:
			evt.type = MTY_EVENT_SCROLL;
			evt.scroll.x = GET_WHEEL_DELTA_WPARAM(wparam);
			evt.scroll.y = 0;
			break;
		case WM_POINTERENTER: {
			UINT32 id = GET_POINTERID_WPARAM(wparam);

			POINTER_INPUT_TYPE type = PT_POINTER;
			if (GetPointerType(id, &type)) {
				app->pen_in_range = type == PT_PEN;
				app->touch_active = type == PT_TOUCH || type == PT_TOUCHPAD;
			}
			break;
		}
		case WM_POINTERLEAVE:
			app->pen_in_range = false;
			app->touch_active = false;
			break;
		case WM_POINTERUPDATE:
			if (!app->pen_enabled)
				break;

			UINT32 id = GET_POINTERID_WPARAM(wparam);

			POINTER_INPUT_TYPE type = PT_POINTER;
			if (!GetPointerType(id, &type) || type != PT_PEN)
				break;

			POINTER_PEN_INFO ppi = {0};
			if (!GetPointerPenInfo(id, &ppi))
				break;

			POINTER_INFO *pi = &ppi.pointerInfo;
			evt.type = MTY_EVENT_PEN;
			evt.pen.pressure = (uint16_t) ppi.pressure;
			evt.pen.rotation = (uint16_t) ppi.rotation;
			evt.pen.tiltX = (int8_t) ppi.tiltX;
			evt.pen.tiltY = (int8_t) ppi.tiltY;

			ScreenToClient(hwnd, &pi->ptPixelLocation);

			if (pi->ptPixelLocation.x < 0)
				pi->ptPixelLocation.x = 0;

			if (pi->ptPixelLocation.y < 0)
				pi->ptPixelLocation.y = 0;

			evt.pen.x = (uint16_t) pi->ptPixelLocation.x;
			evt.pen.y = (uint16_t) pi->ptPixelLocation.y;

			evt.pen.flags = 0;

			if (!(pi->pointerFlags & POINTER_FLAG_INRANGE))
				evt.pen.flags |= MTY_PEN_FLAG_LEAVE;

			if (pi->pointerFlags & POINTER_FLAG_INCONTACT)
				evt.pen.flags |= MTY_PEN_FLAG_TOUCHING;

			if (ppi.penFlags & PEN_FLAG_INVERTED)
				evt.pen.flags |= MTY_PEN_FLAG_INVERTED;

			if (ppi.penFlags & PEN_FLAG_ERASER)
				evt.pen.flags |= MTY_PEN_FLAG_ERASER;

			defreturn = true;
			break;
		case WM_HOTKEY:
			if (!app->ghk_disabled) {
				evt.type = MTY_EVENT_HOTKEY;
				evt.hotkey = (uint32_t) (uintptr_t) MTY_HashGetInt(app->ghotkey, (uint32_t) wparam);
			}
			break;
		case WM_CHAR: {
			wchar_t wstr[2] = {0};
			wstr[0] = (wchar_t) wparam;
			if (MTY_WideToMulti(wstr, evt.text, 8))
				evt.type = MTY_EVENT_TEXT;
			break;
		}
		case WM_CLIPBOARDUPDATE: {
			DWORD cb_seq = GetClipboardSequenceNumber();
			if (cb_seq != app->cb_seq) {
				evt.type = MTY_EVENT_CLIPBOARD;
				app->cb_seq = cb_seq;
			}
			break;
		}
		case WM_DROPFILES:
			wchar_t namew[MTY_PATH_MAX];

			if (DragQueryFile((HDROP) wparam, 0, namew, MTY_PATH_MAX)) {
				SetForegroundWindow(hwnd);

				MTY_WideToMulti(namew, drop_name, MTY_PATH_MAX);
				evt.drop.name = drop_name;
				evt.drop.buf = MTY_ReadFile(drop_name, &evt.drop.size);

				if (evt.drop.buf)
					evt.type = MTY_EVENT_DROP;
			}

			DragFinish((HDROP) wparam);
			break;
		case WM_INPUT:
			UINT rsize = APP_RI_MAX;
			UINT e = GetRawInputData((HRAWINPUT) lparam, RID_INPUT, ctx->ri, &rsize, sizeof(RAWINPUTHEADER));
			if (e == 0 || e == 0xFFFFFFFF) {
				MTY_Log("'GetRawInputData' failed with error 0x%X", e);
				break;
			}

			RAWINPUTHEADER *header = &ctx->ri->header;
			if (header->dwType == RIM_TYPEMOUSE) {
				app_ri_relative_mouse(app, hwnd, ctx->ri, &evt);

			} else if (header->dwType == RIM_TYPEHID) {
				mty_hid_win32_report(app->hid, (intptr_t) ctx->ri->header.hDevice,
					ctx->ri->data.hid.bRawData, ctx->ri->data.hid.dwSizeHid);
			}
			break;
		case WM_INPUT_DEVICE_CHANGE:
			if (wparam == GIDC_ARRIVAL)
				xip_device_arival(app->xip);

			mty_hid_win32_device_change(app->hid, wparam, lparam);
			break;
		case WM_DEVICECHANGE:
			// This is an add bug related to the tray menu
			// https://social.msdn.microsoft.com/Forums/vstudio/en-US/ab973de0-da5f-4e26-8f7c-ac099396c830
			if (wparam == DBT_DEVNODES_CHANGED && app->tray.menu_open)
				EndMenu();
			break;
	}

	// Tray
	app_tray_msg(app, msg, wparam, lparam, &evt);

	// Transform keyboard into hotkey
	if (evt.type == MTY_EVENT_KEY)
		mty_app_kb_to_hotkey(app, &evt, MTY_EVENT_HOTKEY);

	// Record pressed buttons
	if (evt.type == MTY_EVENT_BUTTON) {
		app_set_button_coords(hwnd, &evt);

		if (evt.button.pressed) {
			app->buttons |= 1 << evt.button.button;

		} else {
			app->buttons &= ~(1 << evt.button.button);
		}
	}

	// Process the message
	if (evt.type != MTY_EVENT_NONE) {
		if (!ctx->cmn.webview || !mty_webview_event(ctx->cmn.webview, &evt))
			app->event_func(&evt, app->opaque);

		if (evt.type == MTY_EVENT_DROP)
			MTY_Free((void *) evt.drop.buf);

		if (!defreturn)
			return creturn ? r : 0;
	}

	return creturn ? r : DefWindowProc(hwnd, msg, wparam, lparam);
}

static LRESULT CALLBACK app_hwnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg) {
		case WM_NCCREATE:
			SetWindowLongPtr(hwnd, 0, (LONG_PTR) ((CREATESTRUCT *) lparam)->lpCreateParams);
			break;
		case WM_DESTROY:
			PostQuitMessage(0);
			break;
		default:
			struct window *ctx = (struct window *) GetWindowLongPtr(hwnd, 0);
			if (ctx && hwnd)
				return app_custom_hwnd_proc(ctx, hwnd, msg, wparam, lparam);
	}

	return DefWindowProc(hwnd, msg, wparam, lparam);
}


// Monitor helpers

struct monitor_cb_info {
	HMONITOR mon;
	wchar_t screen[MTY_SCREEN_MAX];
};

static float monitor_get_scale(HMONITOR mon)
{
	UINT x = 0;
	UINT y = 0;
	HRESULT e = GetDpiForMonitor(mon, MDT_EFFECTIVE_DPI, &x, &y);
	if (e != S_OK || x == 0)
		return 1.0f;

	return x / 96.0f;
}

static HMONITOR monitor_primary(void)
{
	POINT pt = {0};

	return MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
}

static HMONITOR monitor_from_hwnd(HWND hwnd)
{
	return MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
}

static MONITORINFOEX monitor_get_info(HMONITOR mon)
{
	MONITORINFOEX info = {.cbSize = sizeof(MONITORINFOEX)};
	GetMonitorInfo(mon, (MONITORINFO *) &info);

	return info;
}

static BOOL WINAPI monitor_enum(HMONITOR mon, HDC p2, RECT *p3, LPARAM p4)
{
	struct monitor_cb_info *info = (struct monitor_cb_info *) p4;
	MONITORINFOEX mi = monitor_get_info(mon);

	if (!wcscmp(info->screen, mi.szDevice)) {
		info->mon = mon;
		return FALSE;
	}

	return TRUE;
}

static HMONITOR monitor_from_screen(const char *screen)
{
	if (!screen || !screen[0])
		return monitor_primary();

	struct monitor_cb_info info = {0};
	MTY_MultiToWide(screen, info.screen, MTY_SCREEN_MAX);

	EnumDisplayMonitors(NULL, NULL, monitor_enum, (LPARAM) &info);

	return info.mon ? info.mon : monitor_primary();
}


// App

static void app_hid_connect(struct hid_dev *device, void *opaque)
{
	MTY_App *ctx = opaque;

	mty_hid_driver_init(device);

	MTY_Event evt = {0};
	evt.type = MTY_EVENT_CONNECT;
	evt.controller.vid = mty_hid_device_get_vid(device);
	evt.controller.pid = mty_hid_device_get_pid(device);
	evt.controller.id = mty_hid_device_get_id(device);

	ctx->event_func(&evt, ctx->opaque);
}

static void app_hid_disconnect(struct hid_dev *device, void *opaque)
{
	MTY_App *ctx = opaque;

	MTY_Event evt = {0};
	evt.type = MTY_EVENT_DISCONNECT;
	evt.controller.vid = mty_hid_device_get_vid(device);
	evt.controller.pid = mty_hid_device_get_pid(device);
	evt.controller.id = mty_hid_device_get_id(device);

	ctx->event_func(&evt, ctx->opaque);
}

static void app_hid_report(struct hid_dev *device, const void *buf, size_t size, void *opaque)
{
	MTY_App *ctx = opaque;

	MTY_Event evt = {0};
	evt.type = MTY_EVENT_CONTROLLER;

	if (mty_hid_driver_state(device, buf, size, &evt.controller)) {
		if (ctx->flags & MTY_APP_FLAG_HID_EVENTS)
			ctx->event_func(&(MTY_Event) {
				.type = MTY_EVENT_HID,
				.hid.size = size,
				.hid.report = buf,
				.hid.type = evt.controller.type,
				.hid.vid = evt.controller.vid,
				.hid.pid = evt.controller.pid,
				.hid.id = evt.controller.id,
			}, ctx->opaque);

		// Prevent gamepad input while in the background, dedupe
		if (MTY_AppIsActive(ctx) && mty_hid_dedupe(ctx->deduper, &evt.controller))
			ctx->event_func(&evt, ctx->opaque);
	}
}

MTY_App *MTY_AppCreate(MTY_AppFlag flags, MTY_AppFunc appFunc, MTY_EventFunc eventFunc, void *opaque)
{
	bool r = true;

	MTY_App *ctx = MTY_Alloc(1, sizeof(MTY_App));
	ctx->flags = flags;
	ctx->app_func = appFunc;
	ctx->event_func = eventFunc;
	ctx->opaque = opaque;
	ctx->hotkey = MTY_HashCreate(0);
	ctx->ghotkey = MTY_HashCreate(0);
	ctx->deduper = MTY_HashCreate(0);
	ctx->instance = GetModuleHandle(NULL);
	if (!ctx->instance) {
		r = false;
		MTY_Log("'GetModuleHandle' failed with error 0x%X", GetLastError());
		goto except;
	}

	ctx->wc.cbSize = sizeof(WNDCLASSEX);
	ctx->wc.cbWndExtra = sizeof(struct window *);
	ctx->wc.lpfnWndProc = app_hwnd_proc;
	ctx->wc.hInstance = ctx->instance;
	ctx->wc.lpszClassName = APP_CLASS_NAME;

	ctx->cursor = LoadCursor(NULL, IDC_ARROW);

	wchar_t path[MTY_PATH_MAX];
	GetModuleFileName(ctx->instance, path, MTY_PATH_MAX);
	ExtractIconEx(path, 0, &ctx->wc.hIcon, &ctx->wc.hIconSm, 1);

	ctx->class = RegisterClassEx(&ctx->wc);
	if (ctx->class == 0) {
		r = false;
		MTY_Log("'RegisterClassEx' failed with error 0x%X", GetLastError());
		goto except;
	}

	ctx->tb_msg = RegisterWindowMessage(L"TaskbarCreated");

	// Requires Windows 10, version 1607
	HMODULE user32 = GetModuleHandle(L"user32.dll");
	ctx->AdjustWindowRectExForDpi = (void *) GetProcAddress(user32, "AdjustWindowRectExForDpi");

	ImmDisableIME(0);

	ctx->xip = xip_create();
	ctx->hid = mty_hid_create(app_hid_connect, app_hid_disconnect, app_hid_report, NULL, ctx);

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

	if (ctx->custom_cursor)
		DestroyIcon(ctx->custom_cursor);

	MTY_AppRemoveTray(ctx);
	MTY_AppStayAwake(ctx, false);
	MTY_AppRemoveHotkeys(ctx, MTY_SCOPE_GLOBAL);

	MTY_HashDestroy(&ctx->hotkey, NULL);
	MTY_HashDestroy(&ctx->ghotkey, NULL);
	MTY_HashDestroy(&ctx->deduper, NULL);

	for (MTY_Window x = 0; x < MTY_WINDOW_MAX; x++)
		MTY_WindowDestroy(ctx, x);

	if (ctx->instance && ctx->class != 0)
		UnregisterClass(APP_CLASS_NAME, ctx->instance);

	mty_hid_destroy(&ctx->hid);
	xip_destroy(&ctx->xip);

	APP_KB_HWND = NULL;
	APP_KB_LWIN = APP_KB_RWIN = MTY_MOD_NONE;

	MTY_Free(ctx);
	*app = NULL;
}

void MTY_AppRun(MTY_App *ctx)
{
	for (bool cont = true; cont;) {
		struct window *window = app_get_main_window(ctx);
		if (!window)
			break;

		bool focus = MTY_AppIsActive(ctx);

		// Keyboard, mouse state changes
		if (ctx->prev_state != ctx->state) {
			app_apply_clip(ctx, focus);
			app_apply_cursor(ctx, focus);
			app_apply_mouse_ri(ctx, focus);
			app_apply_keyboard_state(ctx, focus);

			ctx->prev_state = ctx->state;
		}

		// XInput
		if (focus)
			xip_state(ctx->xip, ctx->deduper, ctx->event_func, ctx->opaque);

		// Tray retry in case of failure
		app_tray_retry(ctx, window);

		// Poll messages belonging to the current (main) thread
		for (MSG msg; PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		// Mouse button state reconciliation
		app_fix_mouse_buttons(ctx);

		// WebView main thread upkeep (Steam callbacks)
		if (window->cmn.webview)
			mty_webview_run(window->cmn.webview);

		cont = ctx->app_func(ctx->opaque);

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
	bool r = false;

	for (MTY_Window x = 0; x < MTY_WINDOW_MAX; x++)
		r = r || MTY_WindowIsActive(ctx, x);

	return r;
}

void MTY_AppActivate(MTY_App *ctx, bool active)
{
	MTY_WindowActivate(ctx, 0, active);
}

void MTY_AppSetTray(MTY_App *ctx, const char *tooltip, const MTY_MenuItem *items, uint32_t len)
{
	MTY_AppRemoveTray(ctx);

	ctx->tray.want = true;
	ctx->tray.rctimer = MTY_GetTime();
	ctx->tray.items = MTY_Alloc(len, sizeof(struct menu_item));
	ctx->tray.len = len;

	for (uint32_t x = 0; x < len; x++) {
		ctx->tray.items[x].label = MTY_MultiToWideD(items[x].label);
		ctx->tray.items[x].checked = items[x].checked;
		ctx->tray.items[x].trayID = items[x].trayID;
	}

	HWND hwnd = app_get_main_hwnd(ctx);
	if (hwnd) {
		app_tray_set_nid(&ctx->tray.nid, NIF_TIP | NIF_ICON, hwnd, ctx->wc.hIconSm, ctx->wc.hIcon);
		MTY_MultiToWide(tooltip, ctx->tray.nid.szTip, sizeof(ctx->tray.nid.szTip) / sizeof(wchar_t));

		ctx->tray.init = Shell_NotifyIcon(NIM_ADD, &ctx->tray.nid);
		if (!ctx->tray.init)
			MTY_Log("'Shell_NotifyIcon' failed");
	}
}

void MTY_AppRemoveTray(MTY_App *ctx)
{
	for (uint32_t x = 0; x < ctx->tray.len; x++)
		MTY_Free(ctx->tray.items[x].label);

	MTY_Free(ctx->tray.items);

	ctx->tray.items = NULL;
	ctx->tray.want = false;
	ctx->tray.len = 0;

	HWND hwnd = app_get_main_hwnd(ctx);
	if (hwnd) {
		app_tray_set_nid(&ctx->tray.nid, NIF_TIP | NIF_ICON, hwnd, ctx->wc.hIconSm, ctx->wc.hIcon);

		if (ctx->tray.init)
			Shell_NotifyIcon(NIM_DELETE, &ctx->tray.nid);
	}

	ctx->tray.init = false;
	memset(&ctx->tray.nid, 0, sizeof(NOTIFYICONDATA));
}

void MTY_AppSendNotification(MTY_App *ctx, const char *title, const char *msg)
{
	if (!title && !msg)
		return;

	HWND hwnd = app_get_main_hwnd(ctx);
	if (!hwnd)
		return;

	app_tray_set_nid(&ctx->tray.nid, NIF_INFO | NIF_REALTIME, hwnd, ctx->wc.hIconSm, ctx->wc.hIcon);

	if (title)
		MTY_MultiToWide(title, ctx->tray.nid.szInfoTitle, sizeof(ctx->tray.nid.szInfoTitle) / sizeof(wchar_t));

	if (msg)
		MTY_MultiToWide(msg, ctx->tray.nid.szInfo, sizeof(ctx->tray.nid.szInfo) / sizeof(wchar_t));

	Shell_NotifyIcon(NIM_MODIFY, &ctx->tray.nid);
}

char *MTY_AppGetClipboard(MTY_App *ctx)
{
	struct window *win = app_get_main_window(ctx);
	if (!win)
		return NULL;

	char *text = NULL;

	if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
		if (OpenClipboard(win->hwnd)) {
			HGLOBAL clipbuffer = GetClipboardData(CF_UNICODETEXT);

			if (clipbuffer) {
				wchar_t *locked = GlobalLock(clipbuffer);

				if (locked) {
					text = MTY_WideToMultiD(locked);
					GlobalUnlock(clipbuffer);
				}
			}

			CloseClipboard();
		}
	}

	return text;
}

void MTY_AppSetClipboard(MTY_App *ctx, const char *text)
{
	struct window *win = app_get_main_window(ctx);
	if (!win)
		return;

	if (OpenClipboard(win->hwnd)) {
		wchar_t *wtext = MTY_MultiToWideD(text);
		size_t size = (wcslen(wtext) + 1) * sizeof(wchar_t);

		HGLOBAL mem =  GlobalAlloc(GMEM_MOVEABLE, size);
		if (mem) {
			wchar_t *locked = GlobalLock(mem);

			if (locked) {
				memcpy(locked, wtext, size);
				GlobalUnlock(mem);

				EmptyClipboard();
				SetClipboardData(CF_UNICODETEXT, mem);
			}
		}

		MTY_Free(wtext);
		CloseClipboard();
		ctx->cb_seq = GetClipboardSequenceNumber();
	}
}

void MTY_AppStayAwake(MTY_App *ctx, bool enable)
{
	if (!SetThreadExecutionState(ES_CONTINUOUS | (enable ? ES_DISPLAY_REQUIRED : 0)))
		MTY_Log("'SetThreadExecutionState' failed");
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
	if (relative && !ctx->relative) {
		ctx->relative = true;
		ctx->last_x = ctx->last_y = -1;

		POINT p = {0};
		GetCursorPos(&p);
		ctx->clip.left = p.x;
		ctx->clip.right = p.x + 1;
		ctx->clip.top = p.y;
		ctx->clip.bottom = p.y + 1;

	} else if (!relative && ctx->relative) {
		ctx->relative = false;
		ctx->last_x = ctx->last_y = -1;
	}

	bool focus = MTY_AppIsActive(ctx);
	app_apply_mouse_ri(ctx, focus);
	app_apply_cursor(ctx, focus);
	app_apply_clip(ctx, focus);
}

static void app_set_png_cursor(MTY_App *app, const void *image, size_t size, uint32_t hotX, uint32_t hotY)
{
	HDC dc = NULL;
	ICONINFO ii = {0};
	void *mask = NULL;

	uint32_t width = 0;
	uint32_t height = 0;
	uint8_t *rgba = MTY_DecompressImage(image, size, &width, &height);
	if (!rgba)
		goto except;

	size_t pad = sizeof(size_t) * 8;
	size_t mask_len = width + ((pad - width % pad) / 8) * height;
	mask = MTY_Alloc(mask_len, 1);
	memset(mask, 0xFF, mask_len);

	dc = GetDC(NULL);
	if (!dc) {
		MTY_Log("'GetDC' failed");
		goto except;
	}

	BITMAPV4HEADER bi = {0};
	bi.bV4Size = sizeof(BITMAPV4HEADER);
	bi.bV4Width = width;
	bi.bV4Height = -((int32_t) height);
	bi.bV4Planes = 1;
	bi.bV4BitCount = 32;
	bi.bV4V4Compression = BI_BITFIELDS;
	bi.bV4AlphaMask = 0xFF000000;
	bi.bV4RedMask   = 0x00FF0000;
	bi.bV4GreenMask = 0x0000FF00;
	bi.bV4BlueMask  = 0x000000FF;

	uint8_t *mem = NULL;
	ii.xHotspot = hotX;
	ii.yHotspot = hotY;
	ii.hbmColor = CreateDIBSection(dc, (BITMAPINFO *) &bi, DIB_RGB_COLORS, &mem, NULL, 0);
	if (!ii.hbmColor) {
		MTY_Log("'CreateDIBSection' failed to create color bitmap");
		goto except;
	}

	ii.hbmMask = CreateBitmap(width, height, 1, 1, mask);
	if (!ii.hbmMask) {
		MTY_Log("'CreateBitmap' failed to create mask bitmap");
		goto except;
	}

	uint32_t pitch = width * 4;
	for (uint32_t y = 0; y < height; y++) {
		for (uint32_t x = 0; x < pitch; x += 4) {
			mem[y * pitch + x + 0] = rgba[y * pitch + x + 2];
			mem[y * pitch + x + 1] = rgba[y * pitch + x + 1];
			mem[y * pitch + x + 2] = rgba[y * pitch + x + 0];
			mem[y * pitch + x + 3] = rgba[y * pitch + x + 3];
		}
	}

	app->custom_cursor = CreateIconIndirect(&ii);
	if (!app->custom_cursor) {
		MTY_Log("'CreateIconIndirect' failed with error 0x%X", GetLastError());
		goto except;
	}

	except:

	if (dc)
		ReleaseDC(NULL, dc);

	if (ii.hbmColor)
		DeleteObject(ii.hbmColor);

	if (ii.hbmMask)
		DeleteObject(ii.hbmMask);

	MTY_Free(mask);
	MTY_Free(rgba);
}

void MTY_AppSetPNGCursor(MTY_App *ctx, const void *image, size_t size, uint32_t hotX, uint32_t hotY)
{
	if (ctx->custom_cursor) {
		DestroyIcon(ctx->custom_cursor);
		ctx->custom_cursor = NULL;
		ctx->state++;
	}

	if (image && size > 0) {
		app_set_png_cursor(ctx, image, size, hotX, hotY);
		ctx->state++;
	}
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

void MTY_AppGrabKeyboard(MTY_App *ctx, bool grab)
{
	if (ctx->kbgrab != grab) {
		ctx->kbgrab = grab;
		ctx->state++;
	}
}

static void app_unregister_global_hotkeys(MTY_App *app)
{
	HWND hwnd = app_get_main_hwnd(app);
	if (!hwnd)
		return;

	uint64_t i = 0;
	int64_t key = 0;

	while (MTY_HashGetNextKeyInt(app->ghotkey, &i, &key))
		if (key > 0)
			if (!UnregisterHotKey(hwnd, (int32_t) key))
				MTY_Log("'UnregisterHotKey' failed with error 0x%X", GetLastError());
}

static uint32_t app_global_hotkey_lookup(MTY_Mod mod, MTY_Key key)
{
	UINT wmod = MOD_NOREPEAT |
		((mod & MTY_MOD_ALT) ? MOD_ALT : 0) |
		((mod & MTY_MOD_CTRL) ? MOD_CONTROL : 0) |
		((mod & MTY_MOD_SHIFT) ? MOD_SHIFT : 0) |
		((mod & MTY_MOD_WIN) ? MOD_WIN : 0);

	UINT vk = MapVirtualKey(key & 0xFF, MAPVK_VSC_TO_VK);

	return vk > 0 ? ((wmod << 8) | vk) : 0;
}

uint32_t MTY_AppGetHotkey(MTY_App *ctx, MTY_Scope scope, MTY_Mod mod, MTY_Key key)
{
	mod &= 0xFF;

	if (scope == MTY_SCOPE_GLOBAL)
		return (uint32_t) (uintptr_t) MTY_HashGetInt(ctx->ghotkey, app_global_hotkey_lookup(mod, key));

	return (uint32_t) (uintptr_t) MTY_HashGetInt(ctx->hotkey, (mod << 16) | key);
}

void MTY_AppSetHotkey(MTY_App *ctx, MTY_Scope scope, MTY_Mod mod, MTY_Key key, uint32_t id)
{
	mod &= 0xFF;

	if (scope == MTY_SCOPE_LOCAL) {
		MTY_HashSetInt(ctx->hotkey, (mod << 16) | key, (void *) (uintptr_t) id);

	} else {
		HWND hwnd = app_get_main_hwnd(ctx);
		if (!hwnd)
			return;

		uint32_t lookup = app_global_hotkey_lookup(mod, key);
		if (lookup == 0)
			return;

		if (MTY_HashGetInt(ctx->ghotkey, lookup) || id == 0) {
			if (!UnregisterHotKey(hwnd, lookup)) {
				MTY_Log("'UnregisterHotKey' failed with error 0x%X", GetLastError());
				return;
			}
		}

		if (id > 0) {
			if (!RegisterHotKey(hwnd, lookup, lookup >> 8, lookup & 0xFF)) {
				MTY_Log("'RegisterHotKey' failed with error 0x%X", GetLastError());
				return;
			}
		}

		MTY_HashSetInt(ctx->ghotkey, lookup, (void *) (uintptr_t) id);
	}
}

void MTY_AppRemoveHotkeys(MTY_App *ctx, MTY_Scope scope)
{
	if (scope == MTY_SCOPE_GLOBAL) {
		app_unregister_global_hotkeys(ctx);

		MTY_HashDestroy(&ctx->ghotkey, NULL);
		ctx->ghotkey = MTY_HashCreate(0);

	} else if (scope == MTY_SCOPE_LOCAL) {
		MTY_HashDestroy(&ctx->hotkey, NULL);
		ctx->hotkey = MTY_HashCreate(0);
	}
}

void MTY_AppEnableGlobalHotkeys(MTY_App *ctx, bool enable)
{
	ctx->ghk_disabled = !enable;
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
	if (id < 4) {
		xip_rumble(ctx->xip, id, low, high);

	} else {
		mty_hid_driver_rumble(ctx->hid, id, low, high);
	}
}

const char *MTY_AppGetControllerDeviceName(MTY_App *ctx, uint32_t id)
{
	struct hid_dev *device = mty_hid_get_device_by_id(ctx->hid, id);
	if (!device)
		return NULL;

	return mty_hid_device_get_name(device);
}

MTY_CType MTY_AppGetControllerType(MTY_App *ctx, uint32_t id)
{
	struct hid_dev *device = mty_hid_get_device_by_id(ctx->hid, id);
	if (!device)
		return MTY_CTYPE_DEFAULT;

	return hid_driver(device);
}

void MTY_AppSubmitHIDReport(MTY_App *ctx, uint32_t id, const void *report, size_t size)
{
	struct hid_dev *dev = mty_hid_get_device_by_id(ctx->hid, id);
	if (!dev)
		return;

	mty_hid_device_write(dev, report, size);
}

bool MTY_AppIsPenEnabled(MTY_App *ctx)
{
	return ctx->pen_enabled;
}

void MTY_AppEnablePen(MTY_App *ctx, bool enable)
{
	ctx->pen_enabled = enable;
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
	ctx->wmsg_func = func;
}


// Window

static void window_denormalize_rect(MTY_App *app, HMONITOR mon, RECT *r)
{
	MONITORINFOEX mi = monitor_get_info(mon);
	float scale = monitor_get_scale(mon);

	app_adjust_window_rect(app, scale, r);

	int32_t w = r->right - r->left;
	int32_t h = r->bottom - r->top;
	int32_t px_h = lrint(h * (scale - 1)) / 2;
	int32_t px_w = lrint(w * (scale - 1)) / 2;

	r->top = r->top - px_h + mi.rcWork.top;
	r->right = r->right + px_w + mi.rcWork.left;
	r->bottom = r->bottom + px_h + mi.rcWork.top;
	r->left = r->left - px_w + mi.rcWork.left;

	// Ensure the title bar is visible
	if (r->top < mi.rcWork.top)
		r->top = mi.rcWork.top;
}

static void window_normalize_rect(MTY_App *app, HMONITOR mon, RECT *r)
{
	MONITORINFOEX mi = monitor_get_info(mon);
	float scale = monitor_get_scale(mon);

	RECT ar = {0};
	app_adjust_window_rect(app, scale, &ar);

	int32_t w = r->right - r->left;
	int32_t h = r->bottom - r->top;
	int32_t px_h = lrint(h * (1 - (1 / scale))) / 2;
	int32_t px_w = lrint(w * (1 - (1 / scale))) / 2;

	r->top = r->top + px_h - mi.rcWork.top - ar.top;
	r->right = r->right - px_w - mi.rcWork.left - ar.right;
	r->bottom = r->bottom - px_h - mi.rcWork.top - ar.bottom;
	r->left = r->left + px_w - mi.rcWork.left - ar.left;
}

static void window_set_placement(MTY_App *app, HMONITOR mon, HWND hwnd, const MTY_Frame *frame)
{
	WINDOWPLACEMENT p = {.length = sizeof(WINDOWPLACEMENT)};

	p.showCmd = frame->type & MTY_WINDOW_HIDDEN ? SW_HIDE :
		frame->type & MTY_WINDOW_MAXIMIZED ? SW_MAXIMIZE : SW_NORMAL;

	p.rcNormalPosition = (RECT) {
		.top = frame->y,
		.left = frame->x,
		.right = frame->size.w + frame->x,
		.bottom = frame->size.h + frame->y,
	};

	window_denormalize_rect(app, mon, &p.rcNormalPosition);

	SetWindowPlacement(hwnd, &p);
}

MTY_Window MTY_WindowCreate(MTY_App *app, const char *title, const MTY_Frame *frame, MTY_Window index)
{
	MTY_Window window = -1;
	bool r = true;

	wchar_t *titlew = NULL;

	window = app_find_open_window(app, index);
	if (window == -1) {
		r = false;
		MTY_Log("Maximum windows (MTY_WINDOW_MAX) of %u reached", MTY_WINDOW_MAX);
		goto except;
	}

	struct window *ctx = MTY_Alloc(1, sizeof(struct window));
	app->windows[window] = ctx;

	ctx->ri = MTY_Alloc(APP_RI_MAX, 1);
	ctx->app = app;
	ctx->window = window;

	MTY_Frame dframe = {0};

	if (!frame) {
		dframe = APP_DEFAULT_FRAME();
		frame = &dframe;
	}

	ctx->frame = *frame;
	ctx->was_zoomed = frame->type & MTY_WINDOW_MAXIMIZED;

	HMONITOR mon = monitor_from_screen(frame->screen);

	DWORD style = WS_OVERLAPPEDWINDOW;
	int32_t w = CW_USEDEFAULT;
	int32_t h = CW_USEDEFAULT;
	int32_t x = CW_USEDEFAULT;
	int32_t y = CW_USEDEFAULT;

	if (frame->type & MTY_WINDOW_FULLSCREEN) {
		MONITORINFOEX info = monitor_get_info(mon);

		style = WS_POPUP;
		x = info.rcMonitor.left;
		y = info.rcMonitor.top;
		w = info.rcMonitor.right - info.rcMonitor.left;
		h = info.rcMonitor.bottom - info.rcMonitor.top;
	}

	titlew = MTY_MultiToWideD(title ? title : "MTY_Window");

	ctx->hwnd = CreateWindowEx(0, APP_CLASS_NAME, titlew, style, x, y, w, h, NULL, NULL, app->instance, ctx);
	if (!ctx->hwnd) {
		r = false;
		MTY_Log("'CreateWindowEx' failed with error 0x%X", GetLastError());
		goto except;
	}

	if (frame->type & MTY_WINDOW_FULLSCREEN) {
		if (!(frame->type & MTY_WINDOW_HIDDEN))
			ShowWindow(ctx->hwnd, SW_NORMAL);

	} else {
		window_set_placement(app, mon, ctx->hwnd, frame);
	}

	if (!(frame->type & MTY_WINDOW_HIDDEN))
		SetForegroundWindow(ctx->hwnd);

	DragAcceptFiles(ctx->hwnd, TRUE);

	if (window == 0) {
		AddClipboardFormatListener(ctx->hwnd);
		mty_hid_win32_listen(ctx->hwnd);
	}

	except:

	MTY_Free(titlew);

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

	struct window *focus = app_get_focus_window(app);
	if (ctx == focus) {
		MTY_AppGrabKeyboard(app, false);
		MTY_AppGrabMouse(app, false);
	}

	mty_webview_destroy(&ctx->cmn.webview);

	if (ctx->hwnd)
		DestroyWindow(ctx->hwnd);

	if (ctx->cmn.gfx_ctx)
		MTY_Log("Window destroyed with GFX still attached");

	MTY_Free(ctx->ri);
	MTY_Free(ctx);
	app->windows[window] = NULL;
}

MTY_Size MTY_WindowGetSize(MTY_App *app, MTY_Window window)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return (MTY_Size) {0};

	RECT r = {0};
	GetClientRect(ctx->hwnd, &r);

	return (MTY_Size) {
		.w = r.right,
		.h = r.bottom,
	};
}

static MTY_Frame window_get_placement(MTY_App *app, HWND hwnd)
{
	HMONITOR mon = monitor_from_hwnd(hwnd);
	MONITORINFOEX mi = monitor_get_info(mon);

	WINDOWPLACEMENT p = {.length = sizeof(WINDOWPLACEMENT)};
	GetWindowPlacement(hwnd, &p);

	window_normalize_rect(app, mon, &p.rcNormalPosition);

	MTY_Frame frame = {
		.y = p.rcNormalPosition.top,
		.x = p.rcNormalPosition.left,
		.size.w = p.rcNormalPosition.right - p.rcNormalPosition.left,
		.size.h = p.rcNormalPosition.bottom - p.rcNormalPosition.top,
	};

	frame.type = p.showCmd == SW_MAXIMIZE ? MTY_WINDOW_MAXIMIZED : MTY_WINDOW_NORMAL;
	snprintf(frame.screen, MTY_SCREEN_MAX, "%s", MTY_WideToMultiDL(mi.szDevice));

	return frame;
}

MTY_Frame MTY_WindowGetFrame(MTY_App *app, MTY_Window window)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return (MTY_Frame) {0};

	if (MTY_WindowIsFullscreen(app, window))
		return ctx->frame;

	return window_get_placement(app, ctx->hwnd);
}

void MTY_WindowSetFrame(MTY_App *app, MTY_Window window, const MTY_Frame *frame)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return;

	HMONITOR mon = monitor_from_screen(frame->screen);

	if (frame->type & MTY_WINDOW_FULLSCREEN) {
		ctx->frame = *frame;

		MONITORINFOEX info = monitor_get_info(mon);

		uint32_t x = info.rcMonitor.left;
		uint32_t y = info.rcMonitor.top;
		uint32_t w = info.rcMonitor.right - info.rcMonitor.left;
		uint32_t h = info.rcMonitor.bottom - info.rcMonitor.top;

		SetWindowLongPtr(ctx->hwnd, GWL_STYLE, WS_VISIBLE | WS_POPUP);
		SetWindowPos(ctx->hwnd, HWND_TOP, x, y, w, h, SWP_FRAMECHANGED);

	} else {
		LONG_PTR ptr = WS_OVERLAPPEDWINDOW;
		if (frame->type & MTY_WINDOW_MAXIMIZED)
			ptr |= WS_MAXIMIZE;

		SetWindowLongPtr(ctx->hwnd, GWL_STYLE, ptr);
		window_set_placement(app, mon, ctx->hwnd, frame);

		PostMessage(ctx->hwnd, WM_SETICON, ICON_BIG, GetClassLongPtr(ctx->hwnd, GCLP_HICON));
		PostMessage(ctx->hwnd, WM_SETICON, ICON_SMALL, GetClassLongPtr(ctx->hwnd, GCLP_HICONSM));
	}
}

void MTY_WindowSetMinSize(MTY_App *app, MTY_Window window, uint32_t minWidth, uint32_t minHeight)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return;

	RECT r = {.right = minWidth, .bottom = minHeight};
	HMONITOR mon = monitor_from_hwnd(ctx->hwnd);
	window_denormalize_rect(app, mon, &r);

	ctx->min_width = r.right - r.left;
	ctx->min_height = r.bottom - r.top;
}

MTY_Size MTY_WindowGetScreenSize(MTY_App *app, MTY_Window window)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return (MTY_Size) {0};

	MONITORINFOEX info = monitor_get_info(monitor_from_hwnd(ctx->hwnd));

	return (MTY_Size) {
		.w = info.rcMonitor.right - info.rcMonitor.left,
		.h = info.rcMonitor.bottom - info.rcMonitor.top,
	};
}

float MTY_WindowGetScreenScale(MTY_App *app, MTY_Window window)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return 1.0f;

	return monitor_get_scale(monitor_from_hwnd(ctx->hwnd));
}

void MTY_WindowSetTitle(MTY_App *app, MTY_Window window, const char *title)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return;

	wchar_t *titlew = MTY_MultiToWideD(title);

	SetWindowText(ctx->hwnd, titlew);

	MTY_Free(titlew);
}

bool MTY_WindowIsVisible(MTY_App *app, MTY_Window window)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return false;

	return app_hwnd_visible(ctx->hwnd);
}

bool MTY_WindowIsActive(MTY_App *app, MTY_Window window)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return false;

	return app_hwnd_active(ctx->hwnd);
}

void MTY_WindowActivate(MTY_App *app, MTY_Window window, bool active)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return;

	if (active) {
		if (!app_hwnd_visible(ctx->hwnd))
			ShowWindow(ctx->hwnd, ctx->was_zoomed ? SW_MAXIMIZE : SW_RESTORE);

		SetForegroundWindow(ctx->hwnd);

	} else {
		ctx->was_zoomed = IsZoomed(ctx->hwnd);
		ShowWindow(ctx->hwnd, SW_HIDE);
	}
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

	return GetWindowLongPtr(ctx->hwnd, GWL_STYLE) & WS_POPUP;
}

void MTY_WindowSetFullscreen(MTY_App *app, MTY_Window window, bool fullscreen)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return;

	if (fullscreen && !MTY_WindowIsFullscreen(app, window)) {
		MTY_Frame frame = window_get_placement(app, ctx->hwnd);
		frame.type |= MTY_WINDOW_FULLSCREEN;

		MTY_WindowSetFrame(app, window, &frame);

	} else if (!fullscreen && MTY_WindowIsFullscreen(app, window)) {
		ctx->frame.type &= ~MTY_WINDOW_FULLSCREEN;

		MTY_WindowSetFrame(app, window, &ctx->frame);
	}
}

void MTY_WindowWarpCursor(MTY_App *app, MTY_Window window, uint32_t x, uint32_t y)
{
	struct window *ctx = app_get_window(app, window);
	if (!ctx)
		return;

	ClipCursor(NULL);

	POINT p = {.x = x, .y = y};
	if (ClientToScreen(ctx->hwnd, &p))
		SetCursorPos(p.x, p.y);

	MTY_AppSetRelativeMouse(app, false);
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

	return (void *) ctx->hwnd;
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

MTY_Frame MTY_MakeDefaultFrame(int32_t x, int32_t y, uint32_t w, uint32_t h, float maxHeight)
{
	HMONITOR mon = monitor_primary();
	MONITORINFOEX info = monitor_get_info(mon);

	uint32_t screen_h = info.rcMonitor.bottom - info.rcMonitor.top;
	uint32_t screen_w = info.rcMonitor.right - info.rcMonitor.left;
	float scale = monitor_get_scale(mon);

	return mty_window_adjust(screen_w, screen_h, scale, maxHeight, x, y, w, h);
}

static bool app_key_to_str(MTY_Key key, char *str, size_t len)
{
	LONG lparam = (key & 0xFF) << 16;
	if (key & 0x100)
		lparam |= 1 << 24;

	wchar_t wstr[8] = {0};
	if (GetKeyNameText(lparam, wstr, 8))
		return MTY_WideToMulti(wstr, str, len);

	return false;
}

void MTY_HotkeyToString(MTY_Mod mod, MTY_Key key, char *str, size_t len)
{
	memset(str, 0, len);

	MTY_Strcat(str, len, (mod & MTY_MOD_WIN) ? "Win+" : "");
	MTY_Strcat(str, len, (mod & MTY_MOD_CTRL) ? "Ctrl+" : "");
	MTY_Strcat(str, len, (mod & MTY_MOD_ALT) ? "Alt+" : "");
	MTY_Strcat(str, len, (mod & MTY_MOD_SHIFT) ? "Shift+" : "");

	if (key != MTY_KEY_NONE) {
		char c[8] = {0};

		if (app_key_to_str(key, c, 8))
			MTY_Strcat(str, len, c);
	}
}

void MTY_SetAppID(const char *id)
{
	wchar_t *idw = MTY_MultiToWideD(id);

	SetCurrentProcessExplicitAppUserModelID(idw);

	MTY_Free(idw);
}

void *MTY_GLGetProcAddress(const char *name)
{
	return NULL;
}

void MTY_RunAndYield(MTY_IterFunc iter, void *opaque)
{
	while (iter(opaque));
}
