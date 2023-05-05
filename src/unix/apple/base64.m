// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <Foundation/Foundation.h>

void MTY_BytesToBase64(const void *bytes, size_t size, char *base64, size_t base64Size)
{
	NSData *input = [[NSData alloc] initWithBytes:bytes length:size];
	NSString *output = [input base64EncodedStringWithOptions:0];

	if (snprintf(base64, base64Size, "%s", [output UTF8String]) >= (ssize_t) base64Size)
		MTY_Log("'base64Size' is too small");
}
