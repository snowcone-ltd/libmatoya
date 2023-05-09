// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"
#include "net.h"

#include <stdio.h>
#include <string.h>

#include "jnih.h"

struct net {
	jobject s;
	jobject out;
	jobject in;
	uint32_t timeout;
	int32_t b;
};

bool mty_net_parse_url(const char *url, bool *secure, char **host, uint16_t *port, char **path)
{
	bool r = true;

	JNIEnv *env = MTY_GetJNIEnv();

	jstring jurl = mty_jni_strdup(env, url);
	jobject uri = mty_jni_static_obj(env, "android/net/Uri", "parse",
		"(Ljava/lang/String;)Landroid/net/Uri;", jurl);

	jstring jhost = mty_jni_obj(env, uri, "getHost", "()Ljava/lang/String;");
	jstring jscheme = mty_jni_obj(env, uri, "getScheme", "()Ljava/lang/String;");
	jstring jpath = mty_jni_obj(env, uri, "getPath", "()Ljava/lang/String;");
	jstring jquery = mty_jni_obj(env, uri, "getQuery", "()Ljava/lang/String;");

	if (!jhost || !jscheme) {
		r = false;
		goto except;
	}

	char *scheme = mty_jni_cstrmov(env, jscheme);
	jscheme = NULL;

	bool _secure = !MTY_Strcasecmp(scheme, "https");
	MTY_Free(scheme);

	if (secure)
		*secure = _secure;

	jint jport = mty_jni_int(env, uri, "getPort", "()I");
	jport = jport == -1 && _secure ? 443 : jport == -1 && !_secure ? 80 : jport;

	if (port)
		*port = jport;

	if (host) {
		*host = mty_jni_cstrmov(env, jhost);
		jhost = NULL;
	}

	if (!jpath)
		jpath = mty_jni_strdup(env, "/");

	if (!jquery)
		jquery = mty_jni_strdup(env, "");

	if (path) {
		char *path0 = mty_jni_cstrmov(env, jpath);
		jpath = NULL;

		char *path1 = mty_jni_cstrmov(env, jquery);
		jquery = NULL;

		if (path1[0]) {
			size_t size = strlen(path0) + strlen(path1) + 2;
			*path = MTY_Alloc(size, 1);
			snprintf(*path, size, "%s?%s", path0, path1);
			MTY_Free(path0);

		} else {
			*path = path0;
		}

		MTY_Free(path1);
	}

	except:

	mty_jni_free(env, jhost);
	mty_jni_free(env, jscheme);
	mty_jni_free(env, jpath);
	mty_jni_free(env, jquery);
	mty_jni_free(env, uri);
	mty_jni_free(env, jurl);

	return r;
}

struct net *mty_net_connect(const char *url, const char *proxy, uint32_t timeout)
{
	// TODO Proxy setting

	uint16_t port = 0;
	char *host = NULL;
	bool secure = true;
	if (!mty_net_parse_url(url, &secure, &host, &port, NULL))
		return NULL;

	if (!secure) {
		MTY_Log("Insecure WebSocket connections on Android are unsupported");
		MTY_Free(host);
		return NULL;
	}

	struct net *ctx = MTY_Alloc(1, sizeof(struct net));
	ctx->b = -1;

	bool r = true;

	JNIEnv *env = MTY_GetJNIEnv();

	jstring jhost = mty_jni_strdup(env, host);

	// Create TCP socket and TLS context
	jobject factory = mty_jni_static_obj(env, "javax/net/ssl/SSLSocketFactory", "getDefault",
		"()Ljavax/net/SocketFactory;");
	r = mty_jni_ok(env);
	if (!r)
		goto except;

	ctx->s = mty_jni_obj(env, factory, "createSocket", "(Ljava/lang/String;I)Ljava/net/Socket;", jhost, port);
	r = mty_jni_ok(env);
	if (!r)
		goto except;

	mty_jni_retain(env, &ctx->s);

	// Connect timeout
	// TODO Does this affect the underlying TCP "connect" call?
	mty_jni_void(env, ctx->s, "setSoTimeout", "(I)V", timeout);

	// Perform connection/handshake
	mty_jni_void(env, ctx->s, "startHandshake", "()V");
	r = mty_jni_ok(env);
	if (!r)
		goto except;

	// Input/output streams
	ctx->out = mty_jni_obj(env, ctx->s, "getOutputStream", "()Ljava/io/OutputStream;");
	r = mty_jni_ok(env);
	if (!r)
		goto except;

	mty_jni_retain(env, &ctx->out);

	ctx->in = mty_jni_obj(env, ctx->s, "getInputStream", "()Ljava/io/InputStream;");
	r = mty_jni_ok(env);
	if (!r)
		goto except;

	mty_jni_retain(env, &ctx->in);

	except:

	mty_jni_free(env, factory);
	mty_jni_free(env, jhost);
	MTY_Free(host);

	if (!r)
		mty_net_destroy(&ctx);

	return ctx;
}

void mty_net_destroy(struct net **net)
{
	if (!net || !*net)
		return;

	struct net *ctx = *net;

	JNIEnv *env = MTY_GetJNIEnv();

	if (ctx->s) {
		mty_jni_void(env, ctx->s, "close", "()V");
		mty_jni_catch(env);
	}

	mty_jni_release(env, &ctx->in);
	mty_jni_release(env, &ctx->out);
	mty_jni_release(env, &ctx->s);

	MTY_Free(ctx);
	*net = NULL;
}

static void mty_net_prep_read(struct net *ctx, JNIEnv *env, uint32_t timeout)
{
	if (timeout != ctx->timeout) {
		mty_jni_void(env, ctx->s, "setSoTimeout", "(I)V", timeout);
		ctx->timeout = timeout;
	}
}

MTY_Async mty_net_poll(struct net *ctx, uint32_t timeout)
{
	JNIEnv *env = MTY_GetJNIEnv();

	mty_net_prep_read(ctx, env, timeout);

	if (ctx->b != -1)
		return MTY_ASYNC_OK;

	// Try to read one byte -- a timeout throws an exception
	ctx->b = mty_jni_int(env, ctx->in, "read", "()I");
	if (!mty_jni_catch(env)) {
		ctx->b = -1;
		return MTY_ASYNC_CONTINUE;
	}

	return ctx->b != -1 ? MTY_ASYNC_OK : MTY_ASYNC_ERROR;
}

bool mty_net_write(struct net *ctx, const void *buf, size_t size)
{
	JNIEnv *env = MTY_GetJNIEnv();

	jbyteArray jbuf = mty_jni_dup(env, buf, size);
	mty_jni_void(env, ctx->out, "write", "([B)V", jbuf);
	mty_jni_void(env, ctx->out, "flush", "()V");

	mty_jni_free(env, jbuf);

	return mty_jni_ok(env);
}

bool mty_net_read(struct net *ctx, void *buf, size_t size, uint32_t timeout)
{
	JNIEnv *env = MTY_GetJNIEnv();

	mty_net_prep_read(ctx, env, timeout);

	uint8_t *buf8 = buf;

	for (size_t total = 0; total < size; total++) {
		// First add the cached byte from mty_net_poll
		if (ctx->b != -1) {
			buf8[total] = (uint8_t) ctx->b;
			ctx->b = -1;
			continue;
		}

		// Read one byte at a time
		int32_t b = mty_jni_int(env, ctx->in, "read", "()I");
		if (!mty_jni_catch(env))
			return false;

		if (b == -1)
			return false;

		buf8[total] = (uint8_t) b;
	}

	return true;
}
