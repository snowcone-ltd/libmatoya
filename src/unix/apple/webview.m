// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "webview.h"

#include <WebKit/WebKit.h>

#include "objc.h"
#include "web/keymap.h"

struct webview {
	MTY_App *app;
	MTY_Window window;
	MTY_Hash *keys;
	MTY_Time ts;
	WEBVIEW_READY ready_func;
	WEBVIEW_TEXT text_func;
	WEBVIEW_KEY key_func;
	MTY_Queue *pushq;
	WKWebView *webview;
	bool ready;
	bool passthrough;
};


// Class: Webview

static void webview_noResponderFor(id self, SEL _cmd, SEL eventSelector)
{
}

static Class webview_class(void)
{
	Class cls = objc_getClass(WEBVIEW_CLASS_NAME);
	if (cls)
		return cls;

	cls = OBJC_ALLOCATE("WKWebView", WEBVIEW_CLASS_NAME);

	// Overrides
	OBJC_OVERRIDE(cls, @selector(noResponderFor:), webview_noResponderFor);

	objc_registerClassPair(cls);

	return cls;
}


// Class: MsgHandler

static void msg_handler_userContentController_didReceiveScriptMessage(id self, SEL _cmd,
	WKUserContentController *userContentController, WKScriptMessage *message)
{
	struct webview *ctx = OBJC_CTX();

	const char *str = [message.body UTF8String];
	MTY_JSON *j = NULL;

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
}

static Class msg_handler_class(void)
{
	Class cls = objc_getClass(MSG_HANDLER_CLASS_NAME);
	if (cls)
		return cls;

	cls = OBJC_ALLOCATE("NSObject", MSG_HANDLER_CLASS_NAME);

	// WKScriptMessageHandler
	Protocol *proto = OBJC_PROTOCOL(cls, @protocol(WKScriptMessageHandler));
	if (proto)
		OBJC_POVERRIDE(cls, proto, YES, @selector(userContentController:didReceiveScriptMessage:),
			msg_handler_userContentController_didReceiveScriptMessage);

	objc_registerClassPair(cls);

	return cls;
}


// Public

struct webview *mty_webview_create(MTY_App *app, MTY_Window window, const char *dir,
	bool debug, WEBVIEW_READY ready_func, WEBVIEW_TEXT text_func, WEBVIEW_KEY key_func)
{
	struct webview *ctx = MTY_Alloc(1, sizeof(struct webview));

	ctx->app = app;
	ctx->window = window;
	ctx->ready_func = ready_func;
	ctx->text_func = text_func;
	ctx->key_func = key_func;

	ctx->keys = web_keymap_hash();
	ctx->pushq = MTY_QueueCreate(50, 0);

	// WKWebView creation, start hidden
	#if TARGET_OS_OSX
		NSWindow *nsw = (__bridge NSWindow *) MTY_WindowGetNative(app, window);
		NSView *view = nsw.contentView;

	#else
		UIWindow *view = (__bridge UIWindow *) MTY_WindowGetNative(app, window);
	#endif

	ctx->webview = [OBJC_NEW(webview_class(), NULL) initWithFrame:view.frame];
	[view addSubview:ctx->webview];

	ctx->webview.hidden = YES;

	// Settings
	NSNumber *ndebug = debug ? @YES : @NO;

	[ctx->webview setValue:@NO forKey:@"drawsBackground"];
	[ctx->webview.configuration.preferences setValue:ndebug forKey:@"developerExtrasEnabled"];
	[ctx->webview.configuration.preferences setValue:@YES forKey:@"tabFocusesLinks"];
	[ctx->webview.configuration.preferences setValue:@YES forKey:@"textInteractionEnabled"];

	// Message handler
	NSObject <WKScriptMessageHandler> *handler = OBJC_NEW(msg_handler_class(), ctx);

	[ctx->webview.configuration.userContentController addScriptMessageHandler:handler name:@"native"];

	// MTY javascript shim
	WKUserScript *script = [[WKUserScript alloc] initWithSource:
		@"const __MTY_MSGS = [];"

		@"let __MTY_WEBVIEW = b64 => {"
			@"__MTY_MSGS.push(b64);"
		@"};"

		@"window.MTY_NativeSendText = text => {"
			@"window.webkit.messageHandlers.native.postMessage('T' + text);"
		@"};"

		@"window.webkit.messageHandlers.native.postMessage('R');"

		@"const __MTY_INTERVAL = setInterval(() => {"
			@"if (window.MTY_NativeListener) {"
				@"__MTY_WEBVIEW = b64 => {window.MTY_NativeListener(atob(b64));};"

				@"for (let msg = __MTY_MSGS.shift(); msg; msg = __MTY_MSGS.shift())"
					@"__MTY_WEBVIEW(msg);"

				@"clearInterval(__MTY_INTERVAL);"
			@"}"
		@"}, 100);"

		@"function __mty_key_to_json(evt) {"
			@"let mods = 0;"

			@"if (evt.shiftKey) mods |= 0x01;"
			@"if (evt.ctrlKey)  mods |= 0x02;"
			@"if (evt.altKey)   mods |= 0x04;"
			@"if (evt.metaKey)  mods |= 0x08;"

			@"if (evt.getModifierState('CapsLock')) mods |= 0x10;"
			@"if (evt.getModifierState('NumLock')) mods |= 0x20;"

			@"let cmd = evt.type == 'keydown' ? 'D' : 'U';"
			@"let json = JSON.stringify({'code':evt.code,'mods':mods});"

			@"window.webkit.messageHandlers.native.postMessage(cmd + json);"
		@"}"

		@"document.addEventListener('keydown', __mty_key_to_json);"
		@"document.addEventListener('keyup', __mty_key_to_json);"

		injectionTime:WKUserScriptInjectionTimeAtDocumentStart
		forMainFrameOnly:YES
	];

	[ctx->webview.configuration.userContentController addUserScript:script];

	return ctx;
}

void mty_webview_destroy(struct webview **webview)
{
	if (!webview || !*webview)
		return;

	struct webview *ctx = *webview;

	if (ctx->webview) {
		[ctx->webview removeFromSuperview];
		ctx->webview = nil;
	}

	if (ctx->pushq)
		MTY_QueueFlush(ctx->pushq, MTY_Free);

	MTY_QueueDestroy(&ctx->pushq);
	MTY_HashDestroy(&ctx->keys, NULL);

	MTY_Free(ctx);
	*webview = NULL;
}

void mty_webview_navigate(struct webview *ctx, const char *source, bool url)
{
	NSString *osource = [NSString stringWithUTF8String:source];

	if (url) {
		NSURL *ourl = [[NSURL alloc] initWithString:osource];

		NSURLRequest *req = [[NSURLRequest alloc] initWithURL:ourl];
		[ctx->webview loadRequest:req];

	} else {
		[ctx->webview loadHTMLString:osource baseURL:nil];
	}
}

void mty_webview_show(struct webview *ctx, bool show)
{
	// The way macOS bubbles key events can cause them to fire multiple times
	// between the WebView and window, potentially firing mty_webview_show
	// more than intended

	MTY_Time ts = MTY_GetTime();

	if (MTY_TimeDiff(ctx->ts, ts) < 10)
		return;

	ctx->ts = ts;
	ctx->webview.hidden = !show;

	#if TARGET_OS_OSX
	if (show) {
		[ctx->webview.window makeFirstResponder:ctx->webview];
		ctx->webview.nextResponder = nil;
	}
	#endif
}

bool mty_webview_is_visible(struct webview *ctx)
{
	return !ctx->webview.hidden;
}

void mty_webview_send_text(struct webview *ctx, const char *msg)
{
	if (!ctx->ready) {
		MTY_QueuePushPtr(ctx->pushq, MTY_Strdup(msg), 0);

	} else {
		__block NSString *omsg = [NSString stringWithUTF8String:msg];

		NSString *b64 = [[omsg dataUsingEncoding:NSUTF8StringEncoding] base64EncodedStringWithOptions:0];
		NSString *wrapped = [NSString stringWithFormat:@"__MTY_WEBVIEW('%@');", b64];

		[ctx->webview evaluateJavaScript:wrapped completionHandler:^(id object, NSError *error) {
			if (error)
				NSLog(@"'WKWebView:evaluateJavaScript' failed to evaluate string '%@'", omsg);
		}];
	}
}

void mty_webview_reload(struct webview *ctx)
{
	[ctx->webview reload];
}

bool mty_webview_open_dev_tools(struct webview *ctx)
{
	@try {
		id inspector = [ctx->webview valueForKey:@"_inspector"];
		if ([inspector respondsToSelector:@selector(show)]) {
			[inspector performSelector:@selector(show)];
		}
	} @catch (NSException *exception) {
		NSLog(@"Could not open WKWebView inspector: %@", exception);
	}
}

void mty_webview_set_input_passthrough(struct webview *ctx, bool passthrough)
{
	ctx->passthrough = passthrough;
}

bool mty_webview_event(struct webview *ctx, MTY_Event *evt)
{
	if (evt->type == MTY_EVENT_SIZE) {
		#if TARGET_OS_OSX
			ctx->webview.frame = ctx->webview.window.contentView.frame;

		#else
			ctx->webview.frame = ctx->webview.window.frame;
		#endif
	}

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
