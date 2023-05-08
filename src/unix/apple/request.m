// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include "net-common.h"

bool MTY_HttpRequest(const char *url, const char *method, const char *headers,
	const void *body, size_t bodySize, const char *proxy, uint32_t timeout,
	void **response, size_t *responseSize, uint16_t *status)
{
	*responseSize = 0;
	*response = NULL;

	// Request
	NSMutableURLRequest *req = net_request(url, method, headers, body, bodySize, timeout);

	// Session configuration
	NSURLSessionConfiguration *cfg = net_configuration(proxy);

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

	[session invalidateAndCancel];

	return r;
}
