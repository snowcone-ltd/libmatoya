// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "webview.h"

#include <stdio.h>

#include <windows.h>
#include <ole2.h>

#include "webview2s.h"

#include "unix/web/keymap.h"

struct webview_handler0 {
	ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler handler;
	void *opaque;
};

struct webview_handler1 {
	ICoreWebView2CreateCoreWebView2ControllerCompletedHandler handler;
	void *opaque;
};

struct webview_handler2 {
	ICoreWebView2WebMessageReceivedEventHandler handler;
	void *opaque;
};

struct webview {
	MTY_App *app;
	MTY_Window window;
	MTY_Hash *keys;
	WEBVIEW_READY ready_func;
	WEBVIEW_TEXT text_func;
	WEBVIEW_KEY key_func;
	MTY_Queue *pushq;
	ICoreWebView2Controller2 *controller;
	ICoreWebView2 *webview;
	struct webview_handler0 handler0;
	struct webview_handler1 handler1;
	struct webview_handler2 handler2;
	MTY_WebViewFlag flags;
	WCHAR *source;
	bool passthrough;
	bool ready;
};


// Generic COM shims

static HRESULT STDMETHODCALLTYPE main_query_interface(void *This,
	REFIID riid, _COM_Outptr_ void **ppvObject)
{
	*ppvObject = NULL;

	return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE main_add_ref(void *This)
{
	return 1;
}

static ULONG STDMETHODCALLTYPE main_release(void *This)
{
	return 0;
}


// ICoreWebView2WebMessageReceivedEventHandler

static HRESULT STDMETHODCALLTYPE main_h2_invoke(ICoreWebView2WebMessageReceivedEventHandler *This,
	ICoreWebView2 *sender, ICoreWebView2WebMessageReceivedEventArgs *args)
{
	struct webview_handler2 *handler = (struct webview_handler2 *) This;
	struct webview *ctx = handler->opaque;

	WCHAR *wstr = NULL;
	HRESULT e = ICoreWebView2WebMessageReceivedEventArgs_TryGetWebMessageAsString(args, &wstr);

	if (e == S_OK) {
		char *str = MTY_WideToMultiD(wstr);
		MTY_JSON *j = NULL;

		switch (str[0]) {
			// MTY_EVENT_WEBVIEW_READY
			case 'R':
				ctx->ready = true;

				// Send any queued messages before the WebView became ready
				for (WCHAR *wmsg = NULL; MTY_QueuePopPtr(ctx->pushq, 0, &wmsg, NULL);) {
					ICoreWebView2_PostWebMessageAsString(ctx->webview, wmsg);
					MTY_Free(wmsg);
				}

				ctx->ready_func(ctx->app, ctx->window);
				break;

			// MTY_EVENT_WEBVIEW_TEXT
			case 'T':
				ctx->text_func(ctx->app, ctx->window, str + 1);
				break;

			// MTY_EVENT_KEY
			case 'D':
			case 'U':
				if (!ctx->passthrough)
					break;

				j = MTY_JSONParse(str + 1);
				if (!j)
					break;

				const char *code = MTY_JSONObjGetStringPtr(j, "code");
				if (!code)
					break;

				uint32_t jmods = 0;
				if (!MTY_JSONObjGetInt(j, "mods", (int32_t *) &jmods))
					break;

				MTY_Key key = (MTY_Key) (uintptr_t) MTY_HashGet(ctx->keys, code) & 0xFFFF;
				if (key == MTY_KEY_NONE)
					break;

				MTY_Mod mods = web_keymap_mods(jmods);

				ctx->key_func(ctx->app, ctx->window, str[0] == 'D', key, mods);
				break;
		}

		MTY_JSONDestroy(&j);
		MTY_Free(str);
	}

	return e;
}


// ICoreWebView2CreateCoreWebView2ControllerCompletedHandler

static HRESULT STDMETHODCALLTYPE main_h1_invoke(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *This,
	HRESULT errorCode, ICoreWebView2Controller *controller)
{
	struct webview_handler1 *handler = (struct webview_handler1 *) This;
	struct webview *ctx = handler->opaque;

	HRESULT e = ICoreWebView2Controller2_QueryInterface(controller, &IID_ICoreWebView2Controller2, &ctx->controller);
	if (e != S_OK)
		return e;

	mty_webview_show(ctx, false);

	ICoreWebView2Controller2_get_CoreWebView2(ctx->controller, &ctx->webview);

	COREWEBVIEW2_COLOR bg = {0};
	ICoreWebView2Controller2_put_DefaultBackgroundColor(ctx->controller, bg);

	mty_webview_update_size(ctx);

	BOOL debug = ctx->flags & MTY_WEBVIEW_FLAG_DEBUG;

	ICoreWebView2Settings *settings = NULL;
	ICoreWebView2_get_Settings(ctx->webview, &settings);
	ICoreWebView2Settings_put_AreDevToolsEnabled(settings, debug);
	ICoreWebView2Settings_put_AreDefaultContextMenusEnabled(settings, debug);
	ICoreWebView2Settings_put_IsZoomControlEnabled(settings, FALSE);
	ICoreWebView2Settings_Release(settings);

	EventRegistrationToken token = {0};
	ICoreWebView2_add_WebMessageReceived(ctx->webview,
		(ICoreWebView2WebMessageReceivedEventHandler *) &ctx->handler2, &token);

	const WCHAR *script =
		L"function MTY_NativeSendText(text) {"
			L"window.chrome.webview.postMessage('T' + text);"
		L"}"

		L"function MTY_NativeAddListener(func) {"
			L"window.chrome.webview.addEventListener('message', evt => {"
				L"func(evt.data);"
			L"});"

			L"window.chrome.webview.postMessage('R');"
		L"}"

		L"function __mty_key_to_json(evt) {"
			L"let mods = 0;"

			L"if (evt.shiftKey) mods |= 0x01;"
			L"if (evt.ctrlKey)  mods |= 0x02;"
			L"if (evt.altKey)   mods |= 0x04;"
			L"if (evt.metaKey)  mods |= 0x08;"

			L"if (evt.getModifierState('CapsLock')) mods |= 0x10;"
			L"if (evt.getModifierState('NumLock')) mods |= 0x20;"

			L"let cmd = evt.type == 'keydown' ? 'D' : 'U';"
			L"let json = JSON.stringify({'code':evt.code,'mods':mods});"

			L"window.chrome.webview.postMessage(cmd + json);"
		L"}"

		L"document.addEventListener('keydown', __mty_key_to_json);"
		L"document.addEventListener('keyup', __mty_key_to_json);";

	ICoreWebView2_AddScriptToExecuteOnDocumentCreated(ctx->webview, script, NULL);

	if (ctx->flags & MTY_WEBVIEW_FLAG_URL) {
		ICoreWebView2_Navigate(ctx->webview, ctx->source);

	} else {
		ICoreWebView2_NavigateToString(ctx->webview, ctx->source);
	}

	return S_OK;
}


// ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler

static HRESULT STDMETHODCALLTYPE main_h0_invoke(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *This,
	HRESULT errorCode, ICoreWebView2Environment *env)
{
	struct webview_handler0 *handler = (struct webview_handler0 *) This;
	struct webview *ctx = handler->opaque;

	HWND hwnd = MTY_WindowGetNative(ctx->app, ctx->window);

	return ICoreWebView2Environment_CreateCoreWebView2Controller(env, hwnd,
		(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *) &ctx->handler1);
}


// Vtables

static ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandlerVtbl VTBL0 = {
	.QueryInterface = main_query_interface,
	.AddRef = main_add_ref,
	.Release = main_release,
	.Invoke = main_h0_invoke,
};

static ICoreWebView2CreateCoreWebView2ControllerCompletedHandlerVtbl VTBL1 = {
	.QueryInterface = main_query_interface,
	.AddRef = main_add_ref,
	.Release = main_release,
	.Invoke = main_h1_invoke,
};

static ICoreWebView2WebMessageReceivedEventHandlerVtbl VTBL2 = {
	.QueryInterface = main_query_interface,
	.AddRef = main_add_ref,
	.Release = main_release,
	.Invoke = main_h2_invoke,
};


// Public

struct webview *mty_webview_create(MTY_App *app, MTY_Window window, const char *dir,
	const char *source, MTY_WebViewFlag flags, WEBVIEW_READY ready_func, WEBVIEW_TEXT text_func,
	WEBVIEW_KEY key_func)
{
	struct webview *ctx = MTY_Alloc(1, sizeof(struct webview));

	ctx->app = app;
	ctx->window = window;
	ctx->source = MTY_MultiToWideD(source);
	ctx->flags = flags;
	ctx->ready_func = ready_func;
	ctx->text_func = text_func;
	ctx->key_func = key_func;

	ctx->keys = web_keymap_hash();
	ctx->pushq = MTY_QueueCreate(50, 0);

	ctx->handler0.handler.lpVtbl = &VTBL0;
	ctx->handler0.opaque = ctx;
	ctx->handler1.handler.lpVtbl = &VTBL1;
	ctx->handler1.opaque = ctx;
	ctx->handler2.handler.lpVtbl = &VTBL2;
	ctx->handler2.opaque = ctx;

	const WCHAR *dirw = dir ? MTY_MultiToWideDL(dir) : L"webview-data";

	HRESULT e = CreateCoreWebView2EnvironmentWithOptions(NULL, dirw, NULL,
		(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *) &ctx->handler0);

	if (e != S_OK)
		mty_webview_destroy(&ctx);

	return ctx;
}

void mty_webview_destroy(struct webview **webview)
{
	if (!webview || !*webview)
		return;

	struct webview *ctx = *webview;

	if (ctx->controller)
		ICoreWebView2Controller2_Release(ctx->controller);

	if (ctx->pushq)
		MTY_QueueFlush(ctx->pushq, MTY_Free);

	MTY_QueueDestroy(&ctx->pushq);
	MTY_HashDestroy(&ctx->keys, NULL);

	MTY_Free(ctx->source);

	MTY_Free(ctx);
	*webview = NULL;
}

void mty_webview_show(struct webview *ctx, bool show)
{
	ICoreWebView2Controller2_put_IsVisible(ctx->controller, show);

	if (show)
		ICoreWebView2Controller2_MoveFocus(ctx->controller, COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
}

bool mty_webview_is_visible(struct webview *ctx)
{
	BOOL visible = FALSE;
	ICoreWebView2Controller2_get_IsVisible(ctx->controller, &visible);

	return visible;
}

void mty_webview_send_text(struct webview *ctx, const char *msg)
{
	if (!ctx->ready) {
		MTY_QueuePushPtr(ctx->pushq, MTY_MultiToWideD(msg), 0);

	} else {
		WCHAR *wmsg = MTY_MultiToWideD(msg);
		ICoreWebView2_PostWebMessageAsString(ctx->webview, wmsg);
		MTY_Free(wmsg);
	}
}

void mty_webview_reload(struct webview *ctx)
{
	ICoreWebView2_Reload(ctx->webview);
}

void mty_webview_set_input_passthrough(struct webview *ctx, bool passthrough)
{
	ctx->passthrough = passthrough;
}

void mty_webview_update_size(struct webview *ctx)
{
	MTY_Size size = MTY_WindowGetSize(ctx->app, ctx->window);

	RECT bounds = {0};
	bounds.right = size.w;
	bounds.bottom = size.h;

	ICoreWebView2Controller2_put_Bounds(ctx->controller, bounds);
}
