// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"
#include "net.h"

#include "jnih.h"

struct net {
	jobject s;
	jobject out;
	jobject in;
	uint32_t timeout;
	int32_t b;
};

static jstring net_parse_url(const char *url, jint *port)
{
	JNIEnv *env = MTY_GetJNIEnv();

	jstring jurl = mty_jni_strdup(env, url);
	jobject uri = mty_jni_static_obj(env, "android/net/Uri", "parse",
		"(Ljava/lang/String;)Landroid/net/Uri;", jurl);

	jstring jhost = mty_jni_obj(env, uri, "getHost", "()Ljava/lang/String;");

	*port = mty_jni_int(env, uri, "getPort", "()I");
	*port = *port == -1 ? 443 : *port;

	mty_jni_free(env, uri);
	mty_jni_free(env, jurl);

	return jhost;
}

struct net *mty_net_connect(const char *url, const char *proxy, uint32_t timeout)
{
	// TODO Proxy setting

	if (MTY_Strcasestr(url, "https") != url) {
		MTY_Log("Insecure WebSocket connections on Android are unsupported");
		return NULL;
	}

	struct net *ctx = MTY_Alloc(1, sizeof(struct net));
	ctx->b = -1;

	bool r = true;

	JNIEnv *env = MTY_GetJNIEnv();

	jint port = 0;
	jstring jhost = net_parse_url(url, &port);

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
