// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "app.h"


// App

MTY_App *MTY_AppCreate(MTY_AppFunc appFunc, MTY_EventFunc eventFunc, void *opaque)
{
	return NULL;
}

void MTY_AppDestroy(MTY_App **app)
{
	if (!app || !*app)
		return;

	MTY_App *ctx = *app;

	MTY_Free(ctx);
	*app = NULL;
}

void MTY_AppRun(MTY_App *ctx)
{
}

void MTY_AppSetTimeout(MTY_App *ctx, uint32_t timeout)
{
}

bool MTY_AppIsActive(MTY_App *ctx)
{
	return false;
}

void MTY_AppActivate(MTY_App *ctx, bool active)
{
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
	return NULL;
}

void MTY_AppSetClipboard(MTY_App *ctx, const char *text)
{
}

void MTY_AppStayAwake(MTY_App *ctx, bool enable)
{
}

MTY_DetachState MTY_AppGetDetachState(MTY_App *ctx)
{
	return MTY_DETACH_STATE_NONE;
}

void MTY_AppSetDetachState(MTY_App *ctx, MTY_DetachState state)
{
}

bool MTY_AppIsMouseGrabbed(MTY_App *ctx)
{
	return false;
}

void MTY_AppGrabMouse(MTY_App *ctx, bool grab)
{
}

bool MTY_AppGetRelativeMouse(MTY_App *ctx)
{
	return false;
}

void MTY_AppSetRelativeMouse(MTY_App *ctx, bool relative)
{
}

void MTY_AppSetPNGCursor(MTY_App *ctx, const void *image, size_t size, uint32_t hotX, uint32_t hotY)
{
}

void MTY_AppUseDefaultCursor(MTY_App *ctx, bool useDefault)
{
}

void MTY_AppShowCursor(MTY_App *ctx, bool show)
{
}

bool MTY_AppCanWarpCursor(MTY_App *ctx)
{
	return false;
}

bool MTY_AppIsKeyboardGrabbed(MTY_App *ctx)
{
	return false;
}

void MTY_AppGrabKeyboard(MTY_App *ctx, bool grab)
{
}

uint32_t MTY_AppGetHotkey(MTY_App *ctx, MTY_Scope scope, MTY_Mod mod, MTY_Key key)
{
	return 0;
}

void MTY_AppSetHotkey(MTY_App *ctx, MTY_Scope scope, MTY_Mod mod, MTY_Key key, uint32_t id)
{
}

void MTY_AppRemoveHotkeys(MTY_App *ctx, MTY_Scope scope)
{
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

MTY_Window MTY_WindowCreate(MTY_App *app, const MTY_WindowDesc *desc)
{
	return -1;
}

void MTY_WindowDestroy(MTY_App *app, MTY_Window window)
{
}

bool MTY_WindowGetSize(MTY_App *app, MTY_Window window, uint32_t *width, uint32_t *height)
{
	return false;
}

void MTY_WindowGetPosition(MTY_App *app, MTY_Window window, int32_t *x, int32_t *y)
{
}

bool MTY_WindowGetScreenSize(MTY_App *app, MTY_Window window, uint32_t *width, uint32_t *height)
{
	return false;
}

float MTY_WindowGetScreenScale(MTY_App *app, MTY_Window window)
{
	return 1.0f;
}

void MTY_WindowSetTitle(MTY_App *app, MTY_Window window, const char *title)
{
}

bool MTY_WindowIsVisible(MTY_App *app, MTY_Window window)
{
	return false;
}

bool MTY_WindowIsActive(MTY_App *app, MTY_Window window)
{
	return false;
}

void MTY_WindowActivate(MTY_App *app, MTY_Window window, bool active)
{
}

bool MTY_WindowExists(MTY_App *app, MTY_Window window)
{
	return false;
}

bool MTY_WindowIsFullscreen(MTY_App *app, MTY_Window window)
{
	return false;
}

void MTY_WindowSetFullscreen(MTY_App *app, MTY_Window window, bool fullscreen)
{
}

void MTY_WindowWarpCursor(MTY_App *app, MTY_Window window, uint32_t x, uint32_t y)
{
}

MTY_ContextState MTY_WindowGetContextState(MTY_App *app, MTY_Window window)
{
	return MTY_CONTEXT_STATE_NORMAL;
}


// Window Private

void mty_window_set_gfx(MTY_App *app, MTY_Window window, MTY_GFX api, struct gfx_ctx *gfx_ctx)
{
}

MTY_GFX mty_window_get_gfx(MTY_App *app, MTY_Window window, struct gfx_ctx **gfx_ctx)
{
	return MTY_GFX_NONE;
}

void *mty_window_get_native(MTY_App *app, MTY_Window window)
{
	return NULL;
}


// Misc

void MTY_HotkeyToString(MTY_Mod mod, MTY_Key key, char *str, size_t len)
{
}

void MTY_SetAppID(const char *id)
{
}

void *MTY_GLGetProcAddress(const char *name)
{
	return NULL;
}
