// Copyright (c) Ronald Huveneers <ronald@huuf.info>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#ifdef _WIN32
#include <windows.h>
#endif

#define header_agent "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:87.0) Gecko/20100101 Firefox/87.0"

static const char *ORIGINS[] = {
	"http://127.0.0.1:8080",
};

#define NUM_ORIGINS (sizeof(ORIGINS) / sizeof(const char *))

static bool net_websocket_echo(void)
{
	uint16_t us = 0;
	MTY_WebSocket *ws = MTY_WebSocketConnect("echo.websocket.events", 443, true, "/",
		"Origin: https://echo.websocket.events", 10000, &us);
	test_cmp("MTY_WebSocketConnect", ws != NULL);
	test_cmp("Upgrade Status", us == 101);

	// "echo.websocket.events sponsored by Lob.com"
	char buffer[200] = {0};
	MTY_Async s = MTY_WebSocketRead(ws, 5000, buffer, 200);
	test_cmp("MTY_WebSocketRead", s == MTY_ASYNC_OK);

	// Echo
	memset(buffer, 0, 200);
	snprintf(buffer, 200, "Hello");
	test_cmp("MTY_WebSocketWrite", MTY_WebSocketWrite(ws, buffer));

	memset(buffer, 0, 200);
	s = MTY_WebSocketRead(ws, 5000, buffer, 6);
	test_cmp("MTY_WebSocketRead", s == MTY_ASYNC_OK);
	test_cmp("MTY_WebSocketRead", strcmp(buffer, "Hello") == 0);

	MTY_WebSocketDestroy(&ws);
	test_cmp("MTY_WebSocketDestroy", ws == NULL);

	return true;
}

struct test_websocket_data {
	MTY_WebSocket *ws_server;
	MTY_WebSocket *ws_child;
};

static void *test_websocket_accept_thread(void *opaque)
{
	struct test_websocket_data *data = (struct test_websocket_data *)opaque;

	data->ws_child = MTY_WebSocketAccept(data->ws_server, ORIGINS, NUM_ORIGINS, false, 2000);
	if (!data->ws_child)
		printf("%s\n", MTY_GetLog());

	return NULL;
}

static bool net_websocket(void)
{
	struct test_websocket_data data = {0};
	data.ws_server = MTY_WebSocketListen("127.0.0.1", 5359);
	test_cmp("MTY_WebSocketListen", data.ws_server != NULL);

	MTY_Thread *t_test = calloc(1, sizeof(MTY_Thread *));
	t_test = MTY_ThreadCreate(test_websocket_accept_thread, &data);
	uint16_t us = 0;
	MTY_WebSocket *client = MTY_WebSocketConnect("127.0.0.1", 5359, false, "/", "Origin: http://127.0.0.1:8080", 1000, &us);

	MTY_ThreadDestroy(&t_test);
	test_cmp("MTY_WebSocketListen", data.ws_child != NULL);
	test_cmp("MTY_WebSocketListen", client != NULL);


	MTY_WebSocketDestroy(&data.ws_child);
	MTY_WebSocketDestroy(&client);
	MTY_WebSocketDestroy(&data.ws_server);
	test_cmp("MTY_WebSocketDestroy", data.ws_child == NULL);
	test_cmp("MTY_WebSocketDestroy", client == NULL);
	test_cmp("MTY_WebSocketDestroy", data.ws_server == NULL);


	return true;
}

#define badssl_test(host, should_fail) \
	ok = MTY_HttpRequest(host, 0, true, "GET", "/", header_agent, NULL, 0, 10000, &resp, &resp_size, &resp_code); \
	MTY_Free(resp); \
	test_print_cmps("MTY_HttpRequest", should_fail ? !ok : ok, ": " host); \
	result = result && (should_fail ? !ok : ok);

static bool net_badssl(void)
{
	//https://rsa4096.badssl.com/

	void *resp = NULL;
	size_t resp_size = 0;
	uint16_t resp_code = 0;
	bool result = true;
	bool ok = false;

	// Should fail
	badssl_test("expired.badssl.com", true);
	badssl_test("wrong.host.badssl.com", true);
	badssl_test("self-signed.badssl.com", true);
	badssl_test("untrusted-root.badssl.com", true);
	#if !defined(_WIN32) && !defined(__linux__) // FIXME
		badssl_test("revoked.badssl.com", true);
	#endif
	// FIXME badssl_test("pinning-test.badssl.com", true);
	badssl_test("no-common-name.badssl.com", true);
	badssl_test("no-subject.badssl.com", true);
	// badssl_test("incomplete-chain.badssl.com", true);
	badssl_test("reversed-chain.badssl.com", true);
	// badssl_test("cbc.badssl.com", true);
	badssl_test("rc4-md5.badssl.com", true);
	badssl_test("rc4.badssl.com", true);
	// badssl_test("3des.badssl.com", false);
	badssl_test("null.badssl.com", true);
	// badssl_test("mozilla-old.badssl.com", true);
	// badssl_test("mozilla-intermediate.badssl.com", true);
	badssl_test("dh480.badssl.com", true);
	badssl_test("dh512.badssl.com", true);
	// FIXME badssl_test("dh1024.badssl.com", true);
	badssl_test("dh-2048.badssl.com", true);
	// FIXME badssl_test("dh-small-subgroup.badssl.com", true);
	// FIXME badssl_test("dh-composite.badssl.com", true);
	// badssl_test("static-rsa.badssl.com", true);
	// FIXME badssl_test("tls-v1-0.badssl.com", true);
	// FIXME badssl_test("tls-v1-1.badssl.com", true);
	// FIXME badssl_test("no-sct.badssl.com", true);
	badssl_test("subdomain.preloaded-hsts.badssl.com", true);
	badssl_test("superfish.badssl.com", true);
	badssl_test("edellroot.badssl.com", true);
	badssl_test("dsdtestprovider.badssl.com", true);
	badssl_test("preact-cli.badssl.com", true);
	badssl_test("webpack-dev-server.badssl.com", true);
	badssl_test("captive-portal.badssl.com", true);
	badssl_test("mitm-software.badssl.com", true);
	badssl_test("sha1-2016.badssl.com", true);
	badssl_test("sha1-2017.badssl.com", true);
	badssl_test("sha1-intermediate.badssl.com", true);
	badssl_test("invalid-expected-sct.badssl.com", true);

	// Should pass

	badssl_test("sha256.badssl.com", false);
	// badssl_test("sha384.badssl.com", false);    // As of 6/14/2022, these certs seem to be invalid
	// badssl_test("sha512.badssl.com", false);    // As of 6/14/2022, these certs seem to be invalid
	// badssl_test("1000-sans.badssl.com", false); // As of 6/14/2022, these certs seem to be invalid
	// badssl_test("10000-sans.badssl.com", false);
	badssl_test("ecc256.badssl.com", false);
	badssl_test("ecc384.badssl.com", false);
	badssl_test("rsa2048.badssl.com", false);
	badssl_test("rsa4096.badssl.com", false);
	badssl_test("rsa8192.badssl.com", false);
	// badssl_test("extended-validation.badssl.com", false);
	badssl_test("mozilla-modern.badssl.com", false);
	badssl_test("tls-v1-2.badssl.com", false);
	badssl_test("hsts.badssl.com", false);
	badssl_test("upgrade.badssl.com", false);
	badssl_test("preloaded-hsts.badssl.com", false);
	badssl_test("https-everywhere.badssl.com", false);
	badssl_test("long-extended-subdomain-name-containing-many-letters-and-dashes.badssl.com", false);
	badssl_test("longextendedsubdomainnamewithoutdashesinordertotestwordwrapping.badssl.com", false);

	return result;
}

static bool net_main(void)
{
#ifdef _WIN32
	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

	if (!net_websocket())
		return false;

	if (!net_websocket_echo())
		return false;

	if (!net_badssl())
		return false;

	return true;
}
