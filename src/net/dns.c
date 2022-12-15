// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"
#include "dns.h"

#include <stdint.h>

#include "net/sock.h"

bool mty_dns_query(const char *host, bool v6, char *ip, size_t size)
{
	bool r = true;

	int32_t af = v6 ? AF_INET6 : AF_INET;

	struct addrinfo hints = {0};
	hints.ai_family = af;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	struct addrinfo *servinfo = NULL;
	int32_t e = getaddrinfo(host, NULL, &hints, &servinfo);
	if (e != 0) {
		r = false;
		goto except;
	}

	const void *src = NULL;

	if (af == AF_INET6) {
		struct sockaddr_in6 *addr = (struct sockaddr_in6 *) servinfo->ai_addr;
		src = &addr->sin6_addr;

	} else {
		struct sockaddr_in *addr = (struct sockaddr_in *) servinfo->ai_addr;
		src = &addr->sin_addr;
	}

	if (!inet_ntop(af, src, ip, size)) {
		MTY_Log("'inet_ntop' failed with errno %d", errno);
		r = false;
		goto except;
	}

	except:

	if (servinfo)
		freeaddrinfo(servinfo);

	return r;
}
