// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "webview.h"

#include <stdio.h>

#include <windows.h>
#include <ole2.h>

#define COBJMACROS
#include "webview2.h"

#include "unix/web/keymap.h"

typedef HRESULT (WINAPI *WEBVIEW_CREATE_FUNC)(uintptr_t _unknown0, uintptr_t _unknown1,
	const WCHAR *wdir, ICoreWebView2EnvironmentOptions *opts,
	ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *handler);

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
	HMODULE lib;
	ICoreWebView2Controller2 *controller;
	ICoreWebView2 *webview;
	struct webview_handler0 handler0;
	struct webview_handler1 handler1;
	struct webview_handler2 handler2;
	ICoreWebView2EnvironmentOptions opts;
	WCHAR *source;
	bool url;
	bool debug;
	bool passthrough;
	bool ready;
};


// Generic COM shims

static bool com_check_riid(REFIID in, REFIID check)
{
	return (!memcmp(in, &IID_IUnknown, sizeof(IID)) || !memcmp(in, check, sizeof(IID)));
}

static ULONG STDMETHODCALLTYPE com_AddRef(void *This)
{
	return 1;
}

static ULONG STDMETHODCALLTYPE com_Release(void *This)
{
	return 0;
}


// ICoreWebView2WebMessageReceivedEventHandler

static HRESULT STDMETHODCALLTYPE h2_QueryInterface(void *This,
	REFIID riid, _COM_Outptr_ void **ppvObject)
{
	if (com_check_riid(riid, &IID_ICoreWebView2WebMessageReceivedEventHandler)) {
		*ppvObject = This;
		return S_OK;
	}

	return E_NOINTERFACE;
}

static HRESULT STDMETHODCALLTYPE h2_Invoke(ICoreWebView2WebMessageReceivedEventHandler *This,
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

static HRESULT STDMETHODCALLTYPE h1_query_interface(void *This,
	REFIID riid, _COM_Outptr_ void **ppvObject)
{
	if (com_check_riid(riid, &IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler)) {
		*ppvObject = This;
		return S_OK;
	}

	return E_NOINTERFACE;
}

static void webview_update_size(struct webview *ctx)
{
	MTY_Size size = MTY_WindowGetSize(ctx->app, ctx->window);

	RECT bounds = {0};
	bounds.right = size.w;
	bounds.bottom = size.h;

	ICoreWebView2Controller2_put_Bounds(ctx->controller, bounds);
}

static void webview_navigate(struct webview *ctx, WCHAR *source, bool url)
{
	if (url) {
		ICoreWebView2_Navigate(ctx->webview, source);

	} else {
		ICoreWebView2_NavigateToString(ctx->webview, source);
	}
}

static HRESULT STDMETHODCALLTYPE h1_Invoke(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *This,
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

	webview_update_size(ctx);

	ICoreWebView2Settings *settings = NULL;
	ICoreWebView2_get_Settings(ctx->webview, &settings);
	ICoreWebView2Settings_put_AreDevToolsEnabled(settings, ctx->debug);
	ICoreWebView2Settings_put_AreDefaultContextMenusEnabled(settings, ctx->debug);
	ICoreWebView2Settings_put_IsZoomControlEnabled(settings, FALSE);
	ICoreWebView2Settings_Release(settings);

	EventRegistrationToken token = {0};
	ICoreWebView2_add_WebMessageReceived(ctx->webview,
		(ICoreWebView2WebMessageReceivedEventHandler *) &ctx->handler2, &token);

	const WCHAR *script =
		L"const __MTY_MSGS = [];"

		L"window.chrome.webview.addEventListener('message', evt => {"
			L"if (window.MTY_NativeListener) {"
				L"window.MTY_NativeListener(evt.data);"

			L"} else {"
				L"__MTY_MSGS.push(evt.data);"
			L"}"
		L"});"

		L"window.MTY_NativeSendText = text => {"
			L"window.chrome.webview.postMessage('T' + text);"
		L"};"

		L"window.chrome.webview.postMessage('R');"

		L"const __MTY_INTERVAL = setInterval(() => {"
			L"if (window.MTY_NativeListener) {"
				L"for (let msg = __MTY_MSGS.shift(); msg; msg = __MTY_MSGS.shift())"
					L"window.MTY_NativeListener(msg);"

				L"clearInterval(__MTY_INTERVAL);"
			L"}"
		L"}, 100);"

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

	if (ctx->source)
		webview_navigate(ctx, ctx->source, ctx->url);

	return S_OK;
}


// ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler

static HRESULT STDMETHODCALLTYPE h0_QueryInterface(void *This,
	REFIID riid, _COM_Outptr_ void **ppvObject)
{
	if (com_check_riid(riid, &IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler)) {
		*ppvObject = This;
		return S_OK;
	}

	return E_NOINTERFACE;
}

static HRESULT STDMETHODCALLTYPE h0_Invoke(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *This,
	HRESULT errorCode, ICoreWebView2Environment *env)
{
	struct webview_handler0 *handler = (struct webview_handler0 *) This;
	struct webview *ctx = handler->opaque;

	HWND hwnd = MTY_WindowGetNative(ctx->app, ctx->window);

	return ICoreWebView2Environment_CreateCoreWebView2Controller(env, hwnd,
		(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *) &ctx->handler1);
}


// ICoreWebView2EnvironmentOptions

static HRESULT STDMETHODCALLTYPE opts_QueryInterface(void *This,
	REFIID riid, _COM_Outptr_ void **ppvObject)
{
	if (com_check_riid(riid, &IID_ICoreWebView2EnvironmentOptions)) {
		*ppvObject = This;
		return S_OK;
	}

	return E_NOINTERFACE;
}

static HRESULT STDMETHODCALLTYPE opts_get_AdditionalBrowserArguments(
	ICoreWebView2EnvironmentOptions *This, LPWSTR *value)
{
	return E_FAIL;
}

static HRESULT STDMETHODCALLTYPE opts_put_AdditionalBrowserArguments(
	ICoreWebView2EnvironmentOptions *This, LPCWSTR value)
{
	return E_FAIL;
}

static HRESULT STDMETHODCALLTYPE opts_get_Language(
	ICoreWebView2EnvironmentOptions *This, LPWSTR *value)
{
	return E_FAIL;
}

static HRESULT STDMETHODCALLTYPE opts_put_Language(
	ICoreWebView2EnvironmentOptions *This, LPCWSTR value)
{
	return E_FAIL;
}

static HRESULT STDMETHODCALLTYPE opts_get_TargetCompatibleBrowserVersion(
	ICoreWebView2EnvironmentOptions *This, LPWSTR *value)
{
	const WCHAR *src = L"89.0.774.44";
	size_t size = (wcslen(src) + 1) * sizeof(WCHAR);
	WCHAR *dst = CoTaskMemAlloc(size);
	memcpy(dst, src, size);

	*value = dst;

	return S_OK;
}

static HRESULT STDMETHODCALLTYPE opts_put_TargetCompatibleBrowserVersion(
	ICoreWebView2EnvironmentOptions *This, LPCWSTR value)
{
	return E_FAIL;
}

static HRESULT STDMETHODCALLTYPE opts_get_AllowSingleSignOnUsingOSPrimaryAccount(
	ICoreWebView2EnvironmentOptions *This, BOOL *allow)
{
	return E_FAIL;
}

static HRESULT STDMETHODCALLTYPE opts_put_AllowSingleSignOnUsingOSPrimaryAccount(
	ICoreWebView2EnvironmentOptions *This, BOOL allow)
{
	return E_FAIL;
}


// Vtables

static ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandlerVtbl VTBL0 = {
	.QueryInterface = h0_QueryInterface,
	.AddRef = com_AddRef,
	.Release = com_Release,
	.Invoke = h0_Invoke,
};

static ICoreWebView2CreateCoreWebView2ControllerCompletedHandlerVtbl VTBL1 = {
	.QueryInterface = h1_query_interface,
	.AddRef = com_AddRef,
	.Release = com_Release,
	.Invoke = h1_Invoke,
};

static ICoreWebView2WebMessageReceivedEventHandlerVtbl VTBL2 = {
	.QueryInterface = h2_QueryInterface,
	.AddRef = com_AddRef,
	.Release = com_Release,
	.Invoke = h2_Invoke,
};

static ICoreWebView2EnvironmentOptionsVtbl VTBL3 = {
	.QueryInterface = opts_QueryInterface,
	.AddRef = com_AddRef,
	.Release = com_Release,
	.get_AdditionalBrowserArguments = opts_get_AdditionalBrowserArguments,
	.put_AdditionalBrowserArguments = opts_put_AdditionalBrowserArguments,
	.get_Language = opts_get_Language,
	.put_Language = opts_put_Language,
	.get_TargetCompatibleBrowserVersion = opts_get_TargetCompatibleBrowserVersion,
	.put_TargetCompatibleBrowserVersion = opts_put_TargetCompatibleBrowserVersion,
	.get_AllowSingleSignOnUsingOSPrimaryAccount = opts_get_AllowSingleSignOnUsingOSPrimaryAccount,
	.put_AllowSingleSignOnUsingOSPrimaryAccount = opts_put_AllowSingleSignOnUsingOSPrimaryAccount,
};


// Public

static HMODULE webview_load_dll(void)
{
	HKEY key = NULL;
	WCHAR *dll = NULL;
	HMODULE lib = NULL;

	LSTATUS r = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
		L"Software\\Microsoft\\EdgeUpdate\\ClientState\\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}",
		0, KEY_WOW64_32KEY | KEY_READ, &key);
	if (r != ERROR_SUCCESS)
		goto except;

	dll = MTY_Alloc(MAX_PATH, sizeof(WCHAR));
	DWORD size = MAX_PATH * sizeof(WCHAR);

	r = RegQueryValueEx(key, L"EBWebView", 0, NULL, (BYTE *) dll, &size);
	if (r != ERROR_SUCCESS)
		goto except;

	#if defined(_WIN64)
		const WCHAR *path = L"\\EBWebView\\x64\\EmbeddedBrowserWebView.dll";

	#else
		const WCHAR *path = L"\\EBWebView\\x86\\EmbeddedBrowserWebView.dll";
	#endif

	if (wcscat_s(dll, MAX_PATH, path) != 0)
		goto except;

	lib = LoadLibrary(dll);
	if (!lib)
		goto except;

	except:

	MTY_Free(dll);

	if (key)
		RegCloseKey(key);

	return lib;
}

struct webview *mty_webview_create(MTY_App *app, MTY_Window window, const char *dir,
	bool debug, WEBVIEW_READY ready_func, WEBVIEW_TEXT text_func, WEBVIEW_KEY key_func)
{
	struct webview *ctx = MTY_Alloc(1, sizeof(struct webview));

	ctx->app = app;
	ctx->window = window;
	ctx->debug = debug;
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
	ctx->opts.lpVtbl = &VTBL3;

	const WCHAR *dirw = dir ? MTY_MultiToWideDL(dir) : L"webview-data";

	HRESULT e = E_FAIL;
	ctx->lib = webview_load_dll();
	if (!ctx->lib)
		goto except;

	WEBVIEW_CREATE_FUNC func = (WEBVIEW_CREATE_FUNC) GetProcAddress(ctx->lib,
		"CreateWebViewEnvironmentWithOptionsInternal");
	if (!func)
		goto except;

	e = func(1, 0, dirw, &ctx->opts, (ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *) &ctx->handler0);

	except:

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

	if (ctx->lib)
		FreeLibrary(ctx->lib);

	if (ctx->pushq)
		MTY_QueueFlush(ctx->pushq, MTY_Free);

	MTY_QueueDestroy(&ctx->pushq);
	MTY_HashDestroy(&ctx->keys, NULL);

	MTY_Free(ctx->source);

	MTY_Free(ctx);
	*webview = NULL;
}

void mty_webview_navigate(struct webview *ctx, const char *source, bool url)
{
	WCHAR *wsource = MTY_MultiToWideD(source);

	if (ctx->webview) {
		webview_navigate(ctx, wsource, url);
		MTY_Free(wsource);

	} else {
		MTY_Free(ctx->source);
		ctx->source = wsource;
		ctx->url = url;
	}
}

void mty_webview_show(struct webview *ctx, bool show)
{
	if (!ctx->controller)
		return;

	ICoreWebView2Controller2_put_IsVisible(ctx->controller, show);

	if (show)
		ICoreWebView2Controller2_MoveFocus(ctx->controller, COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
}

bool mty_webview_is_visible(struct webview *ctx)
{
	if (!ctx->controller)
		return false;

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
	if (!ctx->webview)
		return;

	ICoreWebView2_Reload(ctx->webview);
}

bool mty_webview_open_dev_tools(struct webview *ctx)
{
	if (!ctx->webview)
		return false;

	ICoreWebView2_OpenDevToolsWindow(ctx->webview);
	return true;
}

void mty_webview_set_input_passthrough(struct webview *ctx, bool passthrough)
{
	ctx->passthrough = passthrough;
}

bool mty_webview_event(struct webview *ctx, MTY_Event *evt)
{
	if (evt->type == MTY_EVENT_SIZE)
		webview_update_size(ctx);

	return false;
}

void mty_webview_run(struct webview *ctx)
{
}

void mty_webview_render(struct webview *ctx)
{
}

bool mty_webview_is_steam(void)
{
	return false;
}
