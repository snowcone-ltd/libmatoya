// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <stdlib.h>
#include <stdio.h>

#include <windows.h>
#include <winhttp.h>

#include "net-common.h"

#define WS_ALLOC_CHUNK   2048
#define WS_PING_INTERVAL 60000

struct MTY_WebSocket {
	HINTERNET ws;

	MTY_Waitable *read_event;
	MTY_Waitable *write_event;
	MTY_Waitable *close_event;

	bool closed;
	bool complete;
	uint8_t *buf;
	DWORD pos;
	DWORD len;
};

static HINTERNET ws_open(const char *proxy, const WCHAR *ua, DWORD flags)
{
	// Proxy
	DWORD access_type = WINHTTP_ACCESS_TYPE_DEFAULT_PROXY;
	WCHAR *wproxy = WINHTTP_NO_PROXY_NAME;

	if (proxy) {
		access_type = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
		wproxy = MTY_MultiToWideD(proxy);
	}

	// Context initialization
	HINTERNET session = WinHttpOpen(ua ? ua : MTY_USER_AGENTW, access_type, wproxy, WINHTTP_NO_PROXY_BYPASS, flags);

	if (session) {
		// Attempt to force TLS 1.2, ignore failure
		DWORD opt = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
		WinHttpSetOption(session, WINHTTP_OPTION_SECURE_PROTOCOLS, &opt, sizeof(DWORD));
	}

	if (wproxy != WINHTTP_NO_PROXY_NAME)
		MTY_Free(wproxy);

	return session;
}

static bool ws_get_status_code(HINTERNET request, uint16_t *status_code)
{
	*status_code = 0;

	WCHAR wheader[128];
	DWORD buf_len = 128 * sizeof(WCHAR);
	if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE, NULL, wheader, &buf_len, NULL))
		return false;

	*status_code = (uint16_t) _wtoi(wheader);

	return true;
}

static void WINAPI ws_async(HINTERNET hInternet, DWORD_PTR dwContext, DWORD dwInternetStatus,
	LPVOID lpvStatusInformation, DWORD dwStatusInformationLength)
{
	MTY_WebSocket *ctx = (MTY_WebSocket *) dwContext;
	if (!ctx)
		return;

	switch (dwInternetStatus) {
		case WINHTTP_CALLBACK_STATUS_HANDLE_CREATED:
		case WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING:
		case WINHTTP_CALLBACK_STATUS_RESOLVING_NAME:
		case WINHTTP_CALLBACK_STATUS_NAME_RESOLVED:
		case WINHTTP_CALLBACK_STATUS_CONNECTING_TO_SERVER:
		case WINHTTP_CALLBACK_STATUS_CONNECTED_TO_SERVER:
		case WINHTTP_CALLBACK_STATUS_SENDING_REQUEST:
		case WINHTTP_CALLBACK_STATUS_REQUEST_SENT:
		case WINHTTP_CALLBACK_STATUS_RECEIVING_RESPONSE:
		case WINHTTP_CALLBACK_STATUS_RESPONSE_RECEIVED:
			break;

		// The connection to the host has been made and the inital HTTP request has been sent
		case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE:
			MTY_WaitableSignal(ctx->write_event);
			break;

		// The response has been received and headers are available
		case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE:
			MTY_WaitableSignal(ctx->read_event);
			break;

		// Fired after a send is complete
		case WINHTTP_CALLBACK_STATUS_WRITE_COMPLETE:
			MTY_WaitableSignal(ctx->write_event);
			break;

		// Fired after a message is read into supplied buffers
		case WINHTTP_CALLBACK_STATUS_READ_COMPLETE: {
			const WINHTTP_WEB_SOCKET_STATUS *status = lpvStatusInformation;

			// Add additional buffer space
			if (status->dwBytesTransferred == ctx->len - ctx->pos) {
				ctx->len += WS_ALLOC_CHUNK;
				ctx->buf = MTY_Realloc(ctx->buf, ctx->len, 1);
			}

			// Discard binary data
			switch (status->eBufferType) {
				case WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE:
				case WINHTTP_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE:
					ctx->pos += status->dwBytesTransferred;
					break;
				case WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE:
				case WINHTTP_WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE:
					break;
				case WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE:
					ctx->closed = true;
					break;
			}

			// Non-fragment message types mean the message is complete
			ctx->complete = status->eBufferType == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE;
			MTY_WaitableSignal(ctx->read_event);
			break;
		}

		// Fired after close hadshake with server complete
		case WINHTTP_CALLBACK_STATUS_CLOSE_COMPLETE:
			MTY_WaitableSignal(ctx->close_event);
			break;

		case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR: {
			const WINHTTP_ASYNC_RESULT *result = lpvStatusInformation;

			// "The connection with the server has been reset or terminated, or an incompatible SSL protocol was encountered."
			if (result->dwError == ERROR_WINHTTP_CONNECTION_ERROR)
				ctx->closed = true;

			if (!ctx->closed)
				MTY_Log("WebSocket error: dwResult:%u, dwError:0x%X", result->dwResult, result->dwError);
			break;
		}
		default:
			MTY_Log("Unknown WebSocket callback 0x%X", dwInternetStatus);
			break;
	}
}

MTY_WebSocket *MTY_WebSocketConnect(const char *url, const char *headers, const char *proxy,
	uint32_t timeout, uint16_t *upgradeStatus)
{
	MTY_WebSocket *ctx = MTY_Alloc(1, sizeof(MTY_WebSocket));

	ctx->read_event = MTY_WaitableCreate();
	ctx->write_event = MTY_WaitableCreate();
	ctx->close_event = MTY_WaitableCreate();

	ctx->len = WS_ALLOC_CHUNK;
	ctx->buf = MTY_Alloc(ctx->len, 1);

	bool r = true;
	HINTERNET session = NULL;
	HINTERNET connect = NULL;
	HINTERNET request = NULL;

	struct net_args nargs = {0};
	if (!net_make_args(url, headers, &nargs)) {
		r = false;
		goto except;
	}

	session = ws_open(proxy, nargs.ua, WINHTTP_FLAG_ASYNC);
	if (!session) {
		r = false;
		goto except;
	}

	if (WinHttpSetStatusCallback(session, ws_async, WINHTTP_CALLBACK_FLAG_ALL_NOTIFICATIONS, 0)) {
		r = false;
		goto except;
	}

	connect = WinHttpConnect(session, nargs.host, nargs.port, 0);
	if (!connect) {
		r = false;
		goto except;
	}

	request = WinHttpOpenRequest(connect, L"GET", nargs.path, NULL, WINHTTP_NO_REFERER,
		WINHTTP_DEFAULT_ACCEPT_TYPES, nargs.secure ? WINHTTP_FLAG_SECURE : 0);
	if (!request) {
		r = false;
		goto except;
	}

	r = WinHttpSetOption(request, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, NULL, 0);
	if (!r)
		goto except;

	DWORD hlen = nargs.headers == WINHTTP_NO_ADDITIONAL_HEADERS ? -1L : 0;
	r = WinHttpSendRequest(request, nargs.headers, hlen, NULL, 0, 0, (DWORD_PTR) ctx);
	if (!r)
		goto except;

	r = MTY_WaitableWait(ctx->write_event, timeout);
	if (!r)
		goto except;

	r = WinHttpReceiveResponse(request, 0);
	if (!r)
		goto except;

	r = MTY_WaitableWait(ctx->read_event, timeout);
	if (!r)
		goto except;

	r = ws_get_status_code(request, upgradeStatus);
	if (!r)
		goto except;

	if (*upgradeStatus != 101) {
		r = false;
		goto except;
	}

	ctx->ws = WinHttpWebSocketCompleteUpgrade(request, (DWORD_PTR) ctx);
	if (!ctx->ws) {
		r = false;
		goto except;
	}

	DWORD opt = WS_PING_INTERVAL;
	WinHttpSetOption(session, WINHTTP_OPTION_WEB_SOCKET_KEEPALIVE_INTERVAL, &opt, sizeof(DWORD));

	except:

	if (request)
		WinHttpCloseHandle(request);

	if (connect)
		WinHttpCloseHandle(connect);

	if (session)
		WinHttpCloseHandle(session);

	net_free_args(&nargs);

	if (!r)
		MTY_WebSocketDestroy(&ctx);

	return ctx;
}

void MTY_WebSocketDestroy(MTY_WebSocket **webSocket)
{
	if (!webSocket || !*webSocket)
		return;

	MTY_WebSocket *ctx = *webSocket;
	ctx->closed = true;

	if (ctx->ws) {
		WinHttpWebSocketClose(ctx->ws, WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, NULL, 0);

		if (ctx->close_event)
			MTY_WaitableWait(ctx->close_event, 1000);

		WinHttpCloseHandle(ctx->ws);
	}

	MTY_WaitableDestroy(&ctx->read_event);
	MTY_WaitableDestroy(&ctx->write_event);
	MTY_WaitableDestroy(&ctx->close_event);

	MTY_Free(ctx->buf);

	MTY_Free(ctx);
	*webSocket = NULL;
}

MTY_Async MTY_WebSocketRead(MTY_WebSocket *ctx, uint32_t timeout, char *msg, size_t size)
{
	do {
		// Server has closed the connection
		if (ctx->closed)
			return MTY_ASYNC_DONE;

		// Full message has been received
		if (ctx->complete) {
			if (ctx->pos < size) {
				memcpy(msg, ctx->buf, ctx->pos);
				msg[ctx->pos] = '\0';

				ctx->complete = false;
				ctx->pos = 0;

				return MTY_ASYNC_OK;
			}

			MTY_Log("WebSocket read buffer is not large enough for message + 1");

			return MTY_ASYNC_ERROR;
		}

		// This function will return ERROR_INVALID_OPERATION if a previous read is in progress
		DWORD e = WinHttpWebSocketReceive(ctx->ws, ctx->buf + ctx->pos, ctx->len - ctx->pos, NULL, NULL);

		if (e != NO_ERROR && e != ERROR_INVALID_OPERATION) {
			// Connection has been disrupted
			if (e == ERROR_WINHTTP_CONNECTION_ERROR) {
				ctx->closed = true;

			} else {
				MTY_Log("'WinHttpWebSocketReceive' failed with error 0x%X", e);
				return MTY_ASYNC_ERROR;
			}
		}

	} while (MTY_WaitableWait(ctx->read_event, timeout));

	return MTY_ASYNC_CONTINUE;
}

bool MTY_WebSocketWrite(MTY_WebSocket *ctx, const char *msg)
{
	DWORD e = WinHttpWebSocketSend(ctx->ws, WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,
		(char *) msg, (DWORD) strlen(msg));

	if (e != NO_ERROR)
		return false;

	return MTY_WaitableWait(ctx->write_event, 1000);
}

uint16_t MTY_WebSocketGetCloseCode(MTY_WebSocket *ctx)
{
	USHORT status = 0;
	DWORD consumed = 0;
	uint8_t reason[256];

	DWORD e = WinHttpWebSocketQueryCloseStatus(ctx->ws, &status, reason, 256, &consumed);

	return e == NO_ERROR ? status : 0;
}
