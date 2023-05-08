// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include <Foundation/NSURLRequest.h>
#include <Foundation/Foundation.h>

#include "http.h"

struct net_parse_args {
	NSMutableURLRequest *req;
	bool ua_found;
};

static void net_parse_headers(const char *key, const char *val, void *opaque)
{
	struct net_parse_args *pargs = opaque;

	if (!MTY_Strcasecmp(key, "User-Agent"))
		pargs->ua_found = true;

	[pargs->req setValue:[NSString stringWithUTF8String:val]
		forHTTPHeaderField:[NSString stringWithUTF8String:key]];
}

static NSMutableURLRequest *net_request(const char *url, const char *method, const char *headers,
	const void *body, size_t bodySize, const char *proxy, uint32_t timeout)
{
	NSMutableURLRequest *req = [NSMutableURLRequest new];

	// Don't cache
	[req setCachePolicy:NSURLRequestReloadIgnoringLocalCacheData];

	// Timeout
	[req setTimeoutInterval:timeout / 1000.0];

	// Method
	[req setHTTPMethod:[NSString stringWithUTF8String:method]];

	// URL
	[req setURL:[NSURL URLWithString:[NSString stringWithUTF8String:url]]];

	// Request headers
	struct net_parse_args pargs = {.req = req};

	if (headers)
		mty_http_parse_headers(headers, net_parse_headers, &pargs);

	if (!pargs.ua_found)
		[req setValue:@MTY_USER_AGENT forHTTPHeaderField:@"User-Agent"];

	pargs.req = nil;

	// Body
	if (body && bodySize > 0)
		[req setHTTPBody:[NSData dataWithBytes:body length:bodySize]];

	return req;
}

static NSURLSessionConfiguration *net_configuration(const char *proxy)
{
	NSURLSessionConfiguration *cfg = [NSURLSessionConfiguration defaultSessionConfiguration];

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

	return cfg;
}
