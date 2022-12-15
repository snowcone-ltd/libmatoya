// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <Foundation/NSURLRequest.h>
#include <Foundation/Foundation.h>

#include "net/http-parse.h"
#include "net/http-proxy.h"

#define MTY_USER_AGENT @"libmatoya/v" MTY_VERSION_STRING

struct request_parse_args {
	NSMutableURLRequest *req;
	bool ua_found;
};

static void request_parse_headers(const char *key, const char *val, void *opaque)
{
	struct request_parse_args *pargs = opaque;

	if (!MTY_Strcasecmp(key, "User-Agent"))
		pargs->ua_found = true;

	[pargs->req setValue:[NSString stringWithUTF8String:val]
		forHTTPHeaderField:[NSString stringWithUTF8String:key]];
}

bool MTY_HttpRequest(const char *host, uint16_t port, bool secure, const char *method,
	const char *path, const char *headers, const void *body, size_t bodySize,
	uint32_t timeout, void **response, size_t *responseSize, uint16_t *status)
{
	*responseSize = 0;
	*response = NULL;

	NSURLSessionConfiguration *cfg = [NSURLSessionConfiguration defaultSessionConfiguration];
	NSMutableURLRequest *req = [NSMutableURLRequest new];

	// Don't cache
	[req setCachePolicy:NSURLRequestReloadIgnoringLocalCacheData];

	// Timeout
	[req setTimeoutInterval:timeout / 1000.0];

	// Method
	[req setHTTPMethod:[NSString stringWithUTF8String:method]];

	// URL (scheme, host, port, path)
	const char *scheme = secure ? "https" : "http";
	port = port > 0 ? port : secure ? 443 : 80;

	bool std_port = (secure && port == 443) || (!secure && port == 80);

	const char *url =  std_port ? MTY_SprintfDL("%s://%s%s", scheme, host, path) :
		MTY_SprintfDL("%s://%s:%u%s", scheme, host, port, path);

	[req setURL:[NSURL URLWithString:[NSString stringWithUTF8String:url]]];

	// Request headers
	struct request_parse_args pargs = {.req = req};

	if (headers)
		mty_http_parse_headers(headers, request_parse_headers, &pargs);

	if (!pargs.ua_found)
		[req setValue:MTY_USER_AGENT forHTTPHeaderField:@"User-Agent"];

	// Body
	if (body && bodySize > 0)
		[req setHTTPBody:[NSData dataWithBytes:body length:bodySize]];

	// Proxy
	const char *proxy = mty_http_get_proxy();

	if (proxy) {
		NSURLComponents *comps = [NSURLComponents componentsWithString:[NSString stringWithUTF8String:proxy]];

		if (comps) {
			cfg.connectionProxyDictionary = @{
				(NSString *) kCFNetworkProxiesHTTPProxy: comps.host,
				(NSString *) kCFNetworkProxiesHTTPSProxy: comps.host,
				(NSString *) kCFNetworkProxiesHTTPPort: comps.port,
				(NSString *) kCFNetworkProxiesHTTPSPort: comps.port,
			};
		}
	}

	// Send request
	NSURLSession *session = [NSURLSession sessionWithConfiguration:cfg];

	__block bool r = false;
	dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);

	NSURLSessionDataTask *task = [session dataTaskWithRequest:req
		completionHandler:^(NSData *data, NSURLResponse *_res, NSError *e)
	{
		NSHTTPURLResponse *res = (NSHTTPURLResponse *) _res;

		if (!data || !res || e != nil) {
			MTY_Log("NSURLConnection failed with error %d", (int32_t) [e code]);

		} else {
			*status = [res statusCode];
			*responseSize = [data length];

			if (*responseSize > 0) {
				*response = MTY_Alloc(*responseSize + 1, 1);
				[data getBytes:*response length:*responseSize];
			}

			r = true;
		}

		dispatch_semaphore_signal(semaphore);
	}];

	[task resume];

	dispatch_semaphore_wait(semaphore, dispatch_time(DISPATCH_TIME_NOW, timeout * NSEC_PER_MSEC));

	return r;
}
