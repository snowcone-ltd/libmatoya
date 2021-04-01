// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <netdb.h>

#define SOCK_ERROR       errno
#define SOCK_WOULD_BLOCK EAGAIN
#define SOCK_IN_PROGRESS EINPROGRESS

#define closesocket      close
#define INVALID_SOCKET   -1

typedef int32_t SOCKET;

static bool sock_set_nonblocking(int32_t s)
{
	return fcntl(s, F_SETFL, O_NONBLOCK) == 0;
}
