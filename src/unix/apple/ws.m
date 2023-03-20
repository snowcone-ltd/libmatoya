// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <Foundation/NSURLRequest.h>
#include <Foundation/Foundation.h>

#include "objc.h"
#include "http.h"

#define WS_PING_INTERVAL 60000.0
#define WS_PONG_TO       (WS_PING_INTERVAL * 3)

struct MTY_WebSocket {
	NSURLSession *session;
	NSURLSessionWebSocketTask *task;
	MTY_Waitable *read;
	MTY_Waitable *conn;
	MTY_Waitable *write;
	MTY_Time last_ping;
	MTY_Time last_pong;
	char *msg;
	bool read_started;
	bool read_error;
	bool closed;
};

struct ws_parse_args {
	NSMutableURLRequest *req;
	bool ua_found;
};


// Class: WebSocket

static void websocket_URLSession_task_didCompleteWithError(id self, SEL _cmd, NSURLSession *session,
	NSURLSessionTask *task, NSError *error)
{
	if (error)
		MTY_Log("'WebSocket:didCompleteWithError' fired with code %ld\n", error.code);
}

static void websocket_URLSession_webSocketTask_didOpenWithProtocol(id self, SEL _cmd, NSURLSession *session,
	NSURLSessionWebSocketTask *webSocketTask, NSString *protocol)
{
	MTY_WebSocket *ctx = OBJC_CTX();

	MTY_WaitableSignal(ctx->conn);
}

static void websocket_URLSession_webSocketTask_didCloseWithCode_reason(id self, SEL _cmd, NSURLSession *session,
	NSURLSessionWebSocketTask *webSocketTask, NSURLSessionWebSocketCloseCode closeCode, NSData *reason)
{
	MTY_WebSocket *ctx = OBJC_CTX();

	ctx->closed = true;

	if (closeCode != NSURLSessionWebSocketCloseCodeNormalClosure)
		MTY_Log("'WebSocket:didCloseWithCode' fired with closeCode %ld\n", closeCode);
}

static void websocket_URLSession_didBecomeInvalidWithError(id self, SEL _cmd, NSURLSession *session, NSError *error)
{
	MTY_WebSocket *ctx = OBJC_CTX();

	MTY_WaitableSignal(ctx->conn);
}

static Class websocket_class(void)
{
	Class cls = objc_getClass(WEBSOCKET_CLASS_NAME);
	if (cls)
		return cls;

	cls = OBJC_ALLOCATE("NSObject", WEBSOCKET_CLASS_NAME);

	// NSURLSessionTaskDelegate
	Protocol *proto = OBJC_PROTOCOL(cls, @protocol(NSURLSessionTaskDelegate));
	if (proto) {
		OBJC_POVERRIDE(cls, proto, NO, @selector(URLSession:task:didCompleteWithError:),
			websocket_URLSession_task_didCompleteWithError);
		OBJC_POVERRIDE(cls, proto, NO, @selector(URLSession:didBecomeInvalidWithError:),
			websocket_URLSession_didBecomeInvalidWithError);
	}

	// NSURLSessionWebSocketDelegate
	proto = OBJC_PROTOCOL(cls, @protocol(NSURLSessionWebSocketDelegate));
	if (proto) {
		OBJC_POVERRIDE(cls, proto, NO, @selector(URLSession:webSocketTask:didOpenWithProtocol:),
			websocket_URLSession_webSocketTask_didOpenWithProtocol);
		OBJC_POVERRIDE(cls, proto, NO, @selector(URLSession:webSocketTask:didCloseWithCode:reason:),
			websocket_URLSession_webSocketTask_didCloseWithCode_reason);
	}

	objc_registerClassPair(cls);

	return cls;
}


// Public

static void ws_parse_headers(const char *key, const char *val, void *opaque)
{
	struct ws_parse_args *pargs = opaque;

	if (!MTY_Strcasecmp(key, "User-Agent"))
		pargs->ua_found = true;

	[pargs->req setValue:[NSString stringWithUTF8String:val]
		forHTTPHeaderField:[NSString stringWithUTF8String:key]];
}

MTY_WebSocket *MTY_WebSocketConnect(const char *host, uint16_t port, bool secure, const char *path,
	const char *headers, uint32_t timeout, uint16_t *upgradeStatus)
{
	MTY_WebSocket *ctx = MTY_Alloc(1, sizeof(MTY_WebSocket));

	ctx->conn = MTY_WaitableCreate();
	ctx->read = MTY_WaitableCreate();
	ctx->write = MTY_WaitableCreate();
	ctx->last_ping = ctx->last_pong = MTY_GetTime();

	NSURLSessionConfiguration *cfg = [NSURLSessionConfiguration defaultSessionConfiguration];
	NSMutableURLRequest *req = [NSMutableURLRequest new];

	// Don't cache
	[req setCachePolicy:NSURLRequestReloadIgnoringLocalCacheData];

	// Timeout
	[req setTimeoutInterval:timeout / 1000.0];

	// Method
	[req setHTTPMethod:@"GET"];

	// URL (scheme, host, port, path)
	const char *scheme = secure ? "wss" : "ws";
	port = port > 0 ? port : secure ? 443 : 80;

	bool std_port = (secure && port == 443) || (!secure && port == 80);

	const char *url =  std_port ? MTY_SprintfDL("%s://%s%s", scheme, host, path) :
		MTY_SprintfDL("%s://%s:%u%s", scheme, host, port, path);

	[req setURL:[NSURL URLWithString:[NSString stringWithUTF8String:url]]];

	// Request headers
	struct ws_parse_args pargs = {.req = req};

	if (headers)
		mty_http_parse_headers(headers, ws_parse_headers, &pargs);

	if (!pargs.ua_found)
		[req setValue:@MTY_USER_AGENT forHTTPHeaderField:@"User-Agent"];

	pargs.req = nil;

	// Proxy
	const char *proxy = mty_http_get_proxy();

	if (proxy) {
		NSURLComponents *comps = [NSURLComponents componentsWithString:[NSString stringWithUTF8String:proxy]];

		if (comps) {
			cfg.connectionProxyDictionary = @{
				@"HTTPEnable": @YES,
				@"HTTPProxy": comps.host,
				@"HTTPPort": comps.port,
				@"HTTPSEnable": @YES,
				@"HTTPSProxy": comps.host,
				@"HTTPSPort": comps.port,
			};
		}
	}

	// Connect
	ctx->session = [NSURLSession sessionWithConfiguration:cfg
		delegate:OBJC_NEW(websocket_class(), ctx) delegateQueue:nil];

	ctx->task = [ctx->session webSocketTaskWithRequest:req];

	[ctx->task resume];

	if (!MTY_WaitableWait(ctx->conn, timeout))
		MTY_WebSocketDestroy(&ctx);

	return ctx;
}

void MTY_WebSocketDestroy(MTY_WebSocket **webSocket)
{
	if (!webSocket || !*webSocket)
		return;

	MTY_WebSocket *ctx = *webSocket;

	if (ctx->task)
		[ctx->task cancelWithCloseCode:NSURLSessionWebSocketCloseCodeNormalClosure reason:nil];

	if (ctx->session) {
		[ctx->session finishTasksAndInvalidate];

		if (ctx->conn)
			MTY_WaitableWait(ctx->conn, INT32_MAX);
	}

	ctx->session = nil;
	ctx->task = nil;

	MTY_WaitableDestroy(&ctx->read);
	MTY_WaitableDestroy(&ctx->conn);
	MTY_WaitableDestroy(&ctx->write);

	MTY_Free(ctx->msg);
	MTY_Free(ctx);
	*webSocket = NULL;
}

MTY_Async MTY_WebSocketRead(MTY_WebSocket *ctx, uint32_t timeout, char *msg, size_t size)
{
	// Implicit ping handler
	MTY_Time now = MTY_GetTime();

	if (MTY_TimeDiff(ctx->last_ping, now) > WS_PING_INTERVAL) {
		[ctx->task sendPingWithPongReceiveHandler:^(NSError *e) {
			if (e) {
				MTY_Log("NSURLSessionWebSocketTask:sendPingWithPongReceiveHandler failed: %s",
					[e.localizedDescription UTF8String]);

			} else {
				ctx->last_pong = MTY_GetTime();
			}
		}];

		ctx->last_ping = now;
	}

	// If we haven't gotten a pong within WS_PONG_TO, error
	if (MTY_TimeDiff(ctx->last_pong, now) > WS_PONG_TO)
		return MTY_ASYNC_ERROR;

	// WebSocket is already closed
	if (ctx->closed || ctx->task.closeCode != NSURLSessionWebSocketCloseCodeInvalid)
		return MTY_ASYNC_DONE;

	// Set completion handler and sempaphore
	if (!ctx->read_started) {
		ctx->read_started = true;
		ctx->read_error = false;

		MTY_Free(ctx->msg);
		ctx->msg = NULL;

		[ctx->task receiveMessageWithCompletionHandler:^(NSURLSessionWebSocketMessage *ws_msg, NSError *e) {
			if (e) {
				MTY_Log("NSURLSessionWebSocketTask:receiveMessage failed: %s", [e.localizedDescription UTF8String]);
				ctx->read_error = true;

			} else {
				if (ws_msg.type == NSURLSessionWebSocketMessageTypeString)
					ctx->msg = MTY_Strdup([ws_msg.string UTF8String]);
			}

			MTY_WaitableSignal(ctx->read);
		}];
	}

	// Wait for a new message
	if (MTY_WaitableWait(ctx->read, timeout)) {
		ctx->read_started = false;

		// Error
		if (ctx->read_error)
			return MTY_ASYNC_ERROR;

		if (ctx->msg) {
			size_t ws_msg_size = strlen(ctx->msg) + 1;

			if (size < ws_msg_size)
				return MTY_ASYNC_ERROR;

			memcpy(msg, ctx->msg, ws_msg_size);

			MTY_Free(ctx->msg);
			ctx->msg = NULL;

			return MTY_ASYNC_OK;
		}
	}

	return MTY_ASYNC_CONTINUE;
}

bool MTY_WebSocketWrite(MTY_WebSocket *ctx, const char *msg)
{
	__block bool r = true;

	NSURLSessionWebSocketMessage *ws_msg = [[NSURLSessionWebSocketMessage alloc]
		initWithString:[NSString stringWithUTF8String:msg]];

	[ctx->task sendMessage:ws_msg completionHandler:^(NSError *e) {
		if (e) {
			r = false;
			MTY_Log("NSURLSessionWebSocketTask:sendMessage failed with error %d", (int32_t) [e code]);
		}

		MTY_WaitableSignal(ctx->write);
	}];

	return MTY_WaitableWait(ctx->write, 1000);
}

uint16_t MTY_WebSocketGetCloseCode(MTY_WebSocket *ctx)
{
	return ctx->task.closeCode;
}
