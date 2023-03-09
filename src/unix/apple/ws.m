// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <Foundation/NSURLRequest.h>
#include <Foundation/Foundation.h>

#include "http.h"

#define WS_PING_INTERVAL 60000.0
#define WS_PONG_TO       (WS_PING_INTERVAL * 3)

@interface WebSocket : NSObject <NSURLSessionTaskDelegate, NSURLSessionWebSocketDelegate>
	@property NSURLSessionWebSocketTask *task;
	@property NSURLSessionWebSocketMessage *msg;
	@property dispatch_semaphore_t read;
	@property dispatch_semaphore_t conn;
	@property MTY_Time last_ping;
	@property MTY_Time last_pong;
	@property bool closed;
@end

@implementation WebSocket : NSObject
	- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task
		didCompleteWithError:(NSError *)error
	{
		if (error)
			MTY_Log("'WebSocket:didCompleteWithError' fired with code %ld\n", error.code);
	}

	- (void)URLSession:(NSURLSession *)session webSocketTask:(NSURLSessionWebSocketTask *)webSocketTask
		didOpenWithProtocol:(NSString *)protocol
	{
		dispatch_semaphore_signal(self.conn);
	}

	- (void)URLSession:(NSURLSession *)session webSocketTask:(NSURLSessionWebSocketTask *)webSocketTask
		didCloseWithCode:(NSURLSessionWebSocketCloseCode)closeCode reason:(NSData *)reason
	{
		self.closed = true;

		if (closeCode != 1000)
			MTY_Log("'WebSocket:didCloseWithCode' fired with closeCode %ld\n", closeCode);
	}
@end

struct ws_parse_args {
	NSMutableURLRequest *req;
	bool ua_found;
};

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
	WebSocket *ctx = [WebSocket new];
	MTY_WebSocket *webSocket = (__bridge_retained MTY_WebSocket *) ctx;

	ctx.conn = dispatch_semaphore_create(0);
	ctx.last_ping = ctx.last_pong = MTY_GetTime();

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
	NSURLSession *session = [NSURLSession sessionWithConfiguration:cfg
		delegate:ctx delegateQueue:nil];

	ctx.task = [session webSocketTaskWithRequest:req];

	[ctx.task resume];

	intptr_t e = dispatch_semaphore_wait(ctx.conn, dispatch_time(DISPATCH_TIME_NOW, timeout * NSEC_PER_MSEC));
	if (e != 0)
		MTY_WebSocketDestroy(&webSocket);

	return webSocket;
}

void MTY_WebSocketDestroy(MTY_WebSocket **webSocket)
{
	if (!webSocket || !*webSocket)
		return;

	WebSocket *ctx = (__bridge_transfer WebSocket *) *webSocket;

	if (ctx.task)
		[ctx.task cancelWithCloseCode:NSURLSessionWebSocketCloseCodeNormalClosure reason:nil];

	ctx.task = nil;
	ctx.conn = nil;
	ctx.read = nil;
	ctx.msg = nil;
	ctx = nil;

	*webSocket = NULL;
}

MTY_Async MTY_WebSocketRead(MTY_WebSocket *webSocket, uint32_t timeout, char *msg, size_t size)
{
	WebSocket *ctx = (__bridge WebSocket *) webSocket;

	// Implicit ping handler
	MTY_Time now = MTY_GetTime();

	if (MTY_TimeDiff(ctx.last_ping, now) > WS_PING_INTERVAL) {
		[ctx.task sendPingWithPongReceiveHandler:^(NSError *e) {
			if (e) {
				MTY_Log("NSURLSessionWebSocketTask:sendPingWithPongReceiveHandler failed: %s",
					[e.localizedDescription UTF8String]);

			} else {
				ctx.last_pong = MTY_GetTime();
			}
		}];

		ctx.last_ping = now;
	}

	// If we haven't gotten a pong within WS_PONG_TO, error
	if (MTY_TimeDiff(ctx.last_pong, now) > WS_PONG_TO)
		return MTY_ASYNC_ERROR;

	// WebSocket is already closed
	if (ctx.closed || ctx.task.closeCode != NSURLSessionWebSocketCloseCodeInvalid)
		return MTY_ASYNC_DONE;

	// Set completion handler and sempaphore
	if (!ctx.read) {
		ctx.read = dispatch_semaphore_create(0);

		[ctx.task receiveMessageWithCompletionHandler:^(NSURLSessionWebSocketMessage *ws_msg, NSError *e) {
			if (e) {
				MTY_Log("NSURLSessionWebSocketTask:receiveMessage failed: %s", [e.localizedDescription UTF8String]);

			} else {
				ctx.msg = ws_msg;
			}

			dispatch_semaphore_signal(ctx.read);
		}];
	}

	// Wait for a new message
	intptr_t e = dispatch_semaphore_wait(ctx.read, dispatch_time(DISPATCH_TIME_NOW, timeout * NSEC_PER_MSEC));

	if (e == 0) {
		ctx.read = nil;

		// Error
		if (!ctx.msg)
			return MTY_ASYNC_ERROR;

		NSURLSessionWebSocketMessage *ws_msg = ctx.msg;
		ctx.msg = nil;

		if (ws_msg.type == NSURLSessionWebSocketMessageTypeString) {
			const char *ws_msg_c = [ws_msg.string UTF8String];
			size_t ws_msg_size = strlen(ws_msg_c) + 1;

			if (size < ws_msg_size)
				return MTY_ASYNC_ERROR;

			memcpy(msg, ws_msg_c, ws_msg_size);
			return MTY_ASYNC_OK;
		}
	}

	return MTY_ASYNC_CONTINUE;
}

bool MTY_WebSocketWrite(MTY_WebSocket *webSocket, const char *msg)
{
	WebSocket *ctx = (__bridge WebSocket *) webSocket;

	__block bool r = true;

	NSURLSessionWebSocketMessage *ws_msg = [[NSURLSessionWebSocketMessage alloc]
		initWithString:[NSString stringWithUTF8String:msg]];

	dispatch_semaphore_t s = dispatch_semaphore_create(0);

	[ctx.task sendMessage:ws_msg completionHandler:^(NSError *e) {
		if (e) {
			r = false;
			MTY_Log("NSURLSessionWebSocketTask:sendMessage failed with error %d", (int32_t) [e code]);
		}

		dispatch_semaphore_signal(s);
	}];

	dispatch_semaphore_wait(s, dispatch_time(DISPATCH_TIME_NOW, 1000 * NSEC_PER_MSEC));

	return r;
}

uint16_t MTY_WebSocketGetCloseCode(MTY_WebSocket *webSocket)
{
	WebSocket *ctx = (__bridge WebSocket *) webSocket;

	return ctx.task.closeCode;
}
