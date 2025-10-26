// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "webview.h"

#include <stdio.h>
#include <string.h>

#include "steam.h"


// C++ base class

// struct CCallbackBase {
//     uint8_t m_nCallbackFlags;
//     int m_iCallback;
//     virtual void Run(void *pvParam) = 0;
//     virtual void Run(void *pvParam, bool bIOFailure, SteamAPICall_t hSteamAPICall) = 0;
//     virtual int GetCallbackSizeBytes() = 0;
// };


// Main

struct sbase {
	// C++ base class
	const void **vtable;
	uint8_t m_nCallbackFlags;
	int m_iCallback;

	// Extended
	struct webview *ctx;
	SteamAPICall_t call;
};

struct webview {
	MTY_App *app;
	MTY_Window window;
	WEBVIEW_READY ready_func;
	WEBVIEW_TEXT text_func;
	WEBVIEW_KEY key_func;
	MTY_Queue *pushq;
	MTY_Mutex *mutex;
	ISteamHTMLSurface *surface;
	EHTMLKeyModifiers smods;
	HHTMLBrowser browser;
	char *source;
	bool debug;
	bool ready;
	bool visible;
	bool passthrough;

	void *bgra;
	bool bgra_dirty;
	size_t bgra_size;
	MTY_RenderDesc desc;

	struct sbase browser_ready;
	struct sbase needs_paint;
	struct sbase finished_request;
	struct sbase js_alert;
	struct sbase set_cursor;
};


// HTML_BrowserReady_t

static void webview_update_size(struct webview *ctx)
{
	MTY_Size size = MTY_WindowGetSize(ctx->app, ctx->window);

	SteamAPI_ISteamHTMLSurface_SetSize(ctx->surface, ctx->browser, size.w, size.h);
}

static void webview_on_browser_ready(struct webview *ctx, HTML_BrowserReady_t *params, bool failure)
{
	if (failure) {
		MTY_Log("'HTML_BrowserReady_t' event failed");
		return;
	}

	ctx->browser = params->unBrowserHandle;

	webview_update_size(ctx);

	if (ctx->source)
		SteamAPI_ISteamHTMLSurface_LoadURL(ctx->surface, ctx->browser, ctx->source, NULL);

	if (ctx->debug)
		SteamAPI_ISteamHTMLSurface_OpenDeveloperTools(ctx->surface, ctx->browser);
}

static void browser_ready_run0(void *This, void *pvParam)
{
	struct sbase *base = This;
	struct webview *ctx = base->ctx;
	HTML_BrowserReady_t *params = pvParam;

	webview_on_browser_ready(ctx, params, false);
}

static void browser_ready_run1(void *This, void *pvParam, bool failure, SteamAPICall_t call)
{
	struct sbase *base = This;
	struct webview *ctx = base->ctx;
	HTML_BrowserReady_t *params = pvParam;

	if (base->call == call)
		webview_on_browser_ready(ctx, params, failure);
}

static int browser_ready_get_size(void *This)
{
	return sizeof(HTML_BrowserReady_t);
}

static const void *BROWSER_READY_VTABLE[3] = {
	browser_ready_run0,
	browser_ready_run1,
	browser_ready_get_size,
};


// HTML_NeedsPaint_t

static void needs_paint_run0(void *This, void *pvParam)
{
	struct sbase *base = This;
	struct webview *ctx = base->ctx;
	HTML_NeedsPaint_t *params = pvParam;

	if (params->pBGRA) {
		MTY_MutexLock(ctx->mutex);

		size_t size = params->unWide * params->unTall * 4;

		if (size > ctx->bgra_size) {
			ctx->bgra = MTY_Realloc(ctx->bgra, size, 1);
			ctx->bgra_size = size;
		}

		memcpy(ctx->bgra, params->pBGRA, size);
		ctx->bgra_dirty = true;

		ctx->desc.filter = MTY_FILTER_NEAREST;
		ctx->desc.imageWidth = params->unWide;
		ctx->desc.imageHeight = params->unTall;
		ctx->desc.cropWidth = params->unWide;
		ctx->desc.cropHeight = params->unTall;
		ctx->desc.aspectRatio = (float) params->unWide / params->unTall;
		ctx->desc.layer = 1;

		MTY_MutexUnlock(ctx->mutex);
	}
}

static void needs_paint_run1(void *This, void *pvParam, bool _dummy0, SteamAPICall_t _dummy1)
{
	needs_paint_run0(This, pvParam);
}

static int needs_paint_get_size(void *This)
{
	return sizeof(HTML_NeedsPaint_t);
}

static const void *NEEDS_PAINT_VTABLE[3] = {
	needs_paint_run0,
	needs_paint_run1,
	needs_paint_get_size,
};


// HTML_FinishedRequest_t

static void finished_request_run0(void *This, void *pvParam)
{
	struct sbase *base = This;
	struct webview *ctx = base->ctx;
	HTML_FinishedRequest_t *params = pvParam;

	const char *script =
		"const __MTY_MSGS = [];"

		"let __MTY_WEBVIEW = b64 => {"
			"__MTY_MSGS.push(b64);"
		"};"

		"window.MTY_NativeSendText = text => {"
			"alert('T' + text);"
		"};"

		"alert('R');"

		"const __MTY_INTERVAL = setInterval(() => {"
			"if (window.MTY_NativeListener) {"
				"__MTY_WEBVIEW = b64 => {window.MTY_NativeListener(atob(b64));};"

				"for (let msg = __MTY_MSGS.shift(); msg; msg = __MTY_MSGS.shift())"
					"__MTY_WEBVIEW(msg);"

				"clearInterval(__MTY_INTERVAL);"
			"}"
		"}, 100);";

	SteamAPI_ISteamHTMLSurface_ExecuteJavascript(ctx->surface, params->unBrowserHandle, script);
}

static void finished_request_run1(void *This, void *pvParam, bool _dummy0, SteamAPICall_t _dummy1)
{
	finished_request_run0(This, pvParam);
}

static int finished_request_get_size(void *This)
{
	return sizeof(HTML_FinishedRequest_t);
}

static const void *FINISHED_REQUEST_VTABLE[3] = {
	finished_request_run0,
	finished_request_run1,
	finished_request_get_size,
};


// HTML_JSAlert_t

static void js_alert_run0(void *This, void *pvParam)
{
	struct sbase *base = This;
	struct webview *ctx = base->ctx;
	HTML_JSAlert_t *params = pvParam;

	SteamAPI_ISteamHTMLSurface_JSDialogResponse(ctx->surface, ctx->browser, true);

	const char *str = params->pchMessage;

	switch (str[0]) {
		// MTY_EVENT_WEBVIEW_READY
		case 'R':
			ctx->ready = true;

			// Send any queued messages before the WebView became ready
			for (char *msg = NULL; MTY_QueuePopPtr(ctx->pushq, 0, (void **) &msg, NULL);) {
				mty_webview_send_text(ctx, msg);
				MTY_Free(msg);
			}

			ctx->ready_func(ctx->app, ctx->window);
			break;

		// MTY_EVENT_WEBVIEW_TEXT
		case 'T':
			ctx->text_func(ctx->app, ctx->window, str + 1);
			break;
	}
}

static void js_alert_run1(void *This, void *pvParam, bool _dummy0, SteamAPICall_t _dummy1)
{
	js_alert_run0(This, pvParam);
}

static int js_alert_get_size(void *This)
{
	return sizeof(HTML_JSAlert_t);
}

static const void *JS_ALERT_VTABLE[3] = {
	js_alert_run0,
	js_alert_run1,
	js_alert_get_size,
};


// HTML_SetCursor_t

static void set_cursor_run0(void *This, void *pvParam)
{
	struct sbase *base = This;
	struct webview *ctx = base->ctx;
	HTML_SetCursor_t *params = pvParam;

	switch (params->eMouseCursor) {
		case dc_hand:  MTY_AppSetCursor(ctx->app, MTY_CURSOR_HAND);  break;
		case dc_ibeam: MTY_AppSetCursor(ctx->app, MTY_CURSOR_IBEAM); break;
		default:
			MTY_AppSetCursor(ctx->app, MTY_CURSOR_ARROW);
			break;
	}
}

static void set_cursor_run1(void *This, void *pvParam, bool _dummy0, SteamAPICall_t _dummy1)
{
	set_cursor_run0(This, pvParam);
}

static int set_cursor_get_size(void *This)
{
	return sizeof(HTML_SetCursor_t);
}

static const void *SET_CURSOR_VTABLE[3] = {
	set_cursor_run0,
	set_cursor_run1,
	set_cursor_get_size,
};


// Public

static void webview_class(struct webview *ctx, SteamAPICall_t call, int callback,
	const void **vtable, struct sbase *sbase)
{
	sbase->ctx = ctx;
	sbase->call = call;
	sbase->m_iCallback = callback;
	sbase->vtable = vtable;

	if (call == 0) {
		SteamAPI_RegisterCallback(sbase, callback);

	} else {
		SteamAPI_RegisterCallResult(sbase, call);
	}
}

struct webview *mty_webview_create(MTY_App *app, MTY_Window window, const char *dir,
	bool debug, WEBVIEW_READY ready_func, WEBVIEW_TEXT text_func, WEBVIEW_KEY key_func)
{
	struct webview *ctx = MTY_Alloc(1, sizeof(struct webview));
	ctx->app = app;
	ctx->window = window;
	ctx->mutex = MTY_MutexCreate();
	ctx->ready_func = ready_func;
	ctx->text_func = text_func;
	ctx->key_func = key_func;
	ctx->debug = debug;

	steam_global_init(dir ? dir : ".");

	ctx->pushq = MTY_QueueCreate(50, 0);

	bool r = SteamAPI_InitSafe();
	if (!r) {
		MTY_Log("'SteamAPI_InitSafe' failed");
		goto except;
	}

	ctx->surface = SteamAPI_SteamHTMLSurface_v005();
	if (!ctx->surface) {
		MTY_Log("'SteamAPI_SteamHTMLSurface_v005' failed");
		r = false;
		goto except;
	}

	r = SteamAPI_ISteamHTMLSurface_Init(ctx->surface);
	if (!r) {
		MTY_Log("'SteamAPI_ISteamHTMLSurface_Init' failed");
		goto except;
	}

	SteamAPICall_t call = SteamAPI_ISteamHTMLSurface_CreateBrowser(ctx->surface, NULL, NULL);

	webview_class(ctx, call, HTML_BrowserReady_t_k_iCallback, BROWSER_READY_VTABLE, &ctx->browser_ready);
	webview_class(ctx, 0, HTML_NeedsPaint_t_k_iCallback, NEEDS_PAINT_VTABLE, &ctx->needs_paint);
	webview_class(ctx, 0, HTML_FinishedRequest_t_k_iCallback, FINISHED_REQUEST_VTABLE, &ctx->finished_request);
	webview_class(ctx, 0, HTML_JSAlert_t_k_iCallback, JS_ALERT_VTABLE, &ctx->js_alert);
	webview_class(ctx, 0, HTML_SetCursor_t_k_iCallback, SET_CURSOR_VTABLE, &ctx->set_cursor);

	except:

	if (!r)
		mty_webview_destroy(&ctx);

	return ctx;
}

void mty_webview_destroy(struct webview **webview)
{
	if (!webview || !*webview)
		return;

	struct webview *ctx = *webview;

	if (ctx->surface) {
		if (ctx->browser != 0)
			SteamAPI_ISteamHTMLSurface_RemoveBrowser(ctx->surface, ctx->browser);

		SteamAPI_ISteamHTMLSurface_Shutdown(ctx->surface);
	}

	SteamAPI_Shutdown();

	if (ctx->pushq)
		MTY_QueueFlush(ctx->pushq, MTY_Free);

	MTY_QueueDestroy(&ctx->pushq);
	MTY_MutexDestroy(&ctx->mutex);
	MTY_Free(ctx->source);
	MTY_Free(ctx->bgra);

	steam_global_destroy();

	MTY_Free(ctx);
	*webview = NULL;
}

void mty_webview_navigate(struct webview *ctx, const char *source, bool url)
{
	if (ctx->browser == 0) {
		MTY_Free(ctx->source);
		ctx->source = MTY_Strdup(source);

	} else {
		SteamAPI_ISteamHTMLSurface_LoadURL(ctx->surface, ctx->browser, source, NULL);
	}
}

void mty_webview_show(struct webview *ctx, bool show)
{
	if (ctx->browser == 0)
		return;

	ctx->visible = show;

	if (show) {
		MTY_AppShowCursor(ctx->app, true);

	} else {
		MTY_AppSetCursor(ctx->app, MTY_CURSOR_NONE);
	}

	SteamAPI_ISteamHTMLSurface_SetKeyFocus(ctx->surface, ctx->browser, show);
	SteamAPI_ISteamHTMLSurface_SetBackgroundMode(ctx->surface, ctx->browser, !show);
}

bool mty_webview_is_visible(struct webview *ctx)
{
	return ctx->visible;
}

void mty_webview_send_text(struct webview *ctx, const char *msg)
{
	if (!ctx->ready) {
		MTY_QueuePushPtr(ctx->pushq, MTY_Strdup(msg), 0);

	} else {
		if (ctx->browser == 0)
			return;

		size_t msg_size = strlen(msg);
		size_t size = msg_size * 4 + 64;
		char *script = MTY_Alloc(size, 1);

		memcpy(script, "__MTY_WEBVIEW('", 15);

		MTY_BytesToBase64(msg, msg_size, script + 15, size - 15);

		size_t end = strlen(script);
		memcpy(script + end, "');", 3);

		SteamAPI_ISteamHTMLSurface_ExecuteJavascript(ctx->surface, ctx->browser, script);
		MTY_Free(script);
	}
}

void mty_webview_reload(struct webview *ctx)
{
	if (ctx->browser == 0)
		return;

	SteamAPI_ISteamHTMLSurface_Reload(ctx->surface, ctx->browser);
}

void mty_webview_open_dev_tools(struct webview *ctx)
{
	return false;
}

void mty_webview_set_input_passthrough(struct webview *ctx, bool passthrough)
{
	ctx->passthrough = passthrough;
}

static EHTMLKeyModifiers webview_mods(MTY_Mod mods)
{
	EHTMLKeyModifiers smods = 0;

	if (mods & MTY_MOD_SHIFT)
		smods |= k_eHTMLKeyModifier_ShiftDown;

	if (mods & MTY_MOD_ALT)
		smods |= k_eHTMLKeyModifier_AltDown;

	if (mods & MTY_MOD_CTRL)
		smods |= k_eHTMLKeyModifier_CtrlDown;

	return smods;
}

bool mty_webview_event(struct webview *ctx, MTY_Event *evt)
{
	if (ctx->browser == 0 || !ctx->visible)
		return false;

	switch (evt->type) {
		case MTY_EVENT_SIZE:
			webview_update_size(ctx);
			return false;
		case MTY_EVENT_SCROLL:
			SteamAPI_ISteamHTMLSurface_MouseWheel(ctx->surface, ctx->browser, evt->scroll.y);
			return true;
		case MTY_EVENT_MOTION:
			if (!evt->motion.relative)
				SteamAPI_ISteamHTMLSurface_MouseMove(ctx->surface, ctx->browser,
					evt->motion.x, evt->motion.y);

			return true;
		case MTY_EVENT_BUTTON: {
			EHTMLMouseButton sbutton;

			switch (evt->button.button) {
				case MTY_BUTTON_LEFT:   sbutton = eHTMLMouseButton_Left;   break;
				case MTY_BUTTON_RIGHT:  sbutton = eHTMLMouseButton_Right;  break;
				case MTY_BUTTON_MIDDLE: sbutton = eHTMLMouseButton_Middle; break;
				default:
					return true;
			}

			if (evt->button.pressed) {
				SteamAPI_ISteamHTMLSurface_MouseDown(ctx->surface, ctx->browser, sbutton);

			} else {
				SteamAPI_ISteamHTMLSurface_MouseUp(ctx->surface, ctx->browser, sbutton);
			}

			// TODO Necessary? SteamAPI_ISteamHTMLSurface_MouseDoubleClick
			return true;
		}
		case MTY_EVENT_KEY: {
			ctx->smods = webview_mods(evt->key.mod);

			if (evt->key.pressed) {
				bool system_key = false; // TODO What are the consequences here?
				SteamAPI_ISteamHTMLSurface_KeyDown(ctx->surface, ctx->browser,
					evt->key.vkey, ctx->smods, system_key);

			} else {
				SteamAPI_ISteamHTMLSurface_KeyUp(ctx->surface, ctx->browser,
					evt->key.vkey, ctx->smods);
			}

			if (ctx->passthrough)
				evt->type = MTY_EVENT_WEBVIEW_KEY;

			return !ctx->passthrough;
		}
		case MTY_EVENT_TEXT: {
			wchar_t codepoint[8] = {0};
			if (!MTY_MultiToWide(evt->text, codepoint, 8))
				break;

			SteamAPI_ISteamHTMLSurface_KeyChar(ctx->surface, ctx->browser,
				codepoint[0], ctx->smods);
			return true;
		}
	}

	return false;
}

void mty_webview_run(struct webview *ctx)
{
	if (!ctx->ready || ctx->visible)
		SteamAPI_RunCallbacks();
}

void mty_webview_render(struct webview *ctx)
{
	if (ctx->visible) {
		MTY_MutexLock(ctx->mutex);

		if (ctx->bgra_dirty) {
			ctx->desc.format = MTY_COLOR_FORMAT_BGRA;
			ctx->bgra_dirty = false;

		} else {
			ctx->desc.format = MTY_COLOR_FORMAT_UNKNOWN;
		}

		MTY_WindowDrawQuad(ctx->app, ctx->window, ctx->bgra, &ctx->desc);

		MTY_MutexUnlock(ctx->mutex);
	}
}

bool mty_webview_is_steam(void)
{
	return true;
}
