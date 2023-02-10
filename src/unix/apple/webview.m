// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "webview.h"

#include <WebKit/WebKit.h>

#include "web/keymap.h"

struct webview {
	MTY_App *app;
	MTY_Window window;
	MTY_Hash *keys;
	WEBVIEW_READY ready_func;
	WEBVIEW_TEXT text_func;
	WEBVIEW_KEY key_func;
	MTY_Queue *pushq;
	WKWebView *webview;
	bool ready;
	bool passthrough;
	bool hidden_during_keydown;
};

@interface MTY_WebView : WKWebView
@end

@implementation MTY_WebView
	- (void)noResponderFor:(SEL)eventSelector
	{
	}
@end

@interface WebViewMessageHandler : NSObject <WKScriptMessageHandler>
	@property struct webview *webview;
@end

@implementation WebViewMessageHandler
	- (void)userContentController:(WKUserContentController *)userContentController
		didReceiveScriptMessage:(WKScriptMessage *)message
	{
		struct webview *ctx = self.webview;

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

				bool visible = mty_webview_is_visible(ctx);

				ctx->key_func(ctx->app, ctx->window, str[0] == 'D', key, mods);

				ctx->hidden_during_keydown = visible && !mty_webview_is_visible(ctx);
				break;
		}

		MTY_JSONDestroy(&j);
	}
@end

struct webview *mty_webview_create(MTY_App *app, MTY_Window window, const char *dir,
	const char *source, MTY_WebViewFlag flags, WEBVIEW_READY ready_func, WEBVIEW_TEXT text_func,
	WEBVIEW_KEY key_func)
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

	ctx->webview = (WKWebView *) [[MTY_WebView alloc] initWithFrame:view.frame];
	[view addSubview:ctx->webview];

	ctx->webview.hidden = YES;

	// Settings
	NSNumber *debug = flags & MTY_WEBVIEW_FLAG_DEBUG ? @YES : @NO;

	[ctx->webview setValue:@NO forKey:@"drawsBackground"];
	[ctx->webview.configuration.preferences setValue:debug forKey:@"developerExtrasEnabled"];

	// Message handler
	WebViewMessageHandler *handler = [WebViewMessageHandler alloc];
	handler.webview = ctx;

	[ctx->webview.configuration.userContentController addScriptMessageHandler:handler name:@"native"];

	// MTY javascript shim
	WKUserScript *script = [[WKUserScript alloc] initWithSource:
		@"let __MTY_WEBVIEW = b64 => {};"

		@"function MTY_NativeSendText(text) {"
			@"window.webkit.messageHandlers.native.postMessage('T' + text);"
		@"}"

		@"function MTY_NativeAddListener(func) {"
			@"__MTY_WEBVIEW = b64 => func(atob(b64));"
			@"window.webkit.messageHandlers.native.postMessage('R');"
		@"}"

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

	// Load source
	NSString *osource = [NSString stringWithUTF8String:source];

	if (flags & MTY_WEBVIEW_FLAG_URL) {
		NSURL *url = [[NSURL alloc] initWithString:osource];

		NSURLRequest *req = [[NSURLRequest alloc] initWithURL:url];
		[ctx->webview loadRequest:req];

	} else {
		[ctx->webview loadHTMLString:osource baseURL:nil];
	}

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

void mty_webview_show(struct webview *ctx, bool show)
{
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

void mty_webview_set_input_passthrough(struct webview *ctx, bool passthrough)
{
	ctx->passthrough = passthrough;
}

void mty_webview_update_size(struct webview *ctx)
{
	#if TARGET_OS_OSX
		ctx->webview.frame = ctx->webview.window.contentView.frame;

	#else
		ctx->webview.frame = ctx->webview.window.frame;
	#endif
}

bool mty_webview_was_hidden_during_keydown(struct webview *ctx)
{
	bool r = ctx->hidden_during_keydown;
	ctx->hidden_during_keydown = false;

	return r;
}
