// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include "jnih.h"
#include "net/http-parse.h"

#define MTY_USER_AGENT "libmatoya/v" MTY_VERSION_STRING

struct request_parse_args {
	bool ua_found;
	jobject urlc_obj;
	JNIEnv *env;
};

static void request_parse_headers(const char *key, const char *val, void *opaque)
{
	struct request_parse_args *pargs = opaque;

	JNIEnv *env = pargs->env;

	if (!MTY_Strcasecmp(key, "User-Agent"))
		pargs->ua_found = true;

	jstring jkey = mty_jni_strdup(env, key);
	jstring jval = mty_jni_strdup(env, val);

	mty_jni_void(env, pargs->urlc_obj, "setRequestProperty",
		"(Ljava/lang/String;Ljava/lang/String;)V", jkey, jval);

	mty_jni_free(env, jkey);
	mty_jni_free(env, jval);
}

bool MTY_HttpRequest(const char *host, uint16_t port, bool secure, const char *method,
	const char *path, const char *headers, const void *body, size_t bodySize,
	uint32_t timeout, void **response, size_t *responseSize, uint16_t *status)
{
	*responseSize = 0;
	*response = NULL;

	// TODO Proxy setting?

	JNIEnv *env = MTY_GetJNIEnv();

	const char *scheme = secure ? "https" : "http";
	port = port > 0 ? port : secure ? 443 : 80;

	bool std_port = (secure && port == 443) || (!secure && port == 80);

	const char *url =  std_port ? MTY_SprintfDL("%s://%s%s", scheme, host, path) :
		MTY_SprintfDL("%s://%s:%u%s", scheme, host, port, path);

	// URL
	jstring jurl = mty_jni_strdup(env, url);
	jobject url_obj = mty_jni_new(env, "java/net/URL", "(Ljava/lang/String;)V", jurl);
	jobject urlc_obj = mty_jni_obj(env, url_obj, "openConnection", "()Ljava/net/URLConnection;");

	mty_jni_void(env, urlc_obj, "setDoInput", "(Z)V", true);

	// Timeouts
	mty_jni_void(env, urlc_obj, "setConnectTimeout", "(I)V", timeout);
	mty_jni_void(env, urlc_obj, "setReadTimeout", "(I)V", timeout);

	// Method
	jstring jmethod = mty_jni_strdup(env, method);
	mty_jni_void(env, urlc_obj, "setRequestMethod", "(Ljava/lang/String;)V", jmethod);

	// Request headers
	struct request_parse_args pargs = {
		.urlc_obj = urlc_obj,
		.env = env,
	};

	if (headers)
		mty_http_parse_headers(headers, request_parse_headers, &pargs);

	if (!pargs.ua_found)
		request_parse_headers("User-Agent", MTY_USER_AGENT, &pargs);

	// Request body
	jbyteArray jbody = NULL;

	if (body && bodySize > 0) {
		mty_jni_void(env, urlc_obj, "setDoOutput", "(Z)V", true);
		mty_jni_void(env, urlc_obj, "setChunkedStreamingMode", "(I)V", 0);

		jobject out = mty_jni_obj(env, urlc_obj, "getOutputStream", "()Ljava/io/OutputStream;");

		jbody = mty_jni_dup(env, body, bodySize);
		mty_jni_void(env, out, "write", "([B)V", jbody);
		mty_jni_void(env, out, "flush", "()V");
		mty_jni_void(env, out, "close", "()V");
	}

	// Response code
	*status = mty_jni_int(env, urlc_obj, "getResponseCode", "()I");
	bool r = mty_jni_ok(env);
	if (!r)
		goto except;

	// Response body
	jobject in = mty_jni_obj(env, urlc_obj, "getInputStream", "()Ljava/io/InputStream;");

	if (!mty_jni_ok(env))
		in = mty_jni_obj(env, urlc_obj, "getErrorStream", "()Ljava/io/InputStream;");

	jobject buf = mty_jni_new(env, "java/io/ByteArrayOutputStream", "()V");

	int32_t b = mty_jni_int(env, in, "read", "()I");

	while (b != -1) {
		mty_jni_void(env, buf, "write", "(I)V", b);
		b = mty_jni_int(env, in, "read", "()I");
	}

	jbyteArray jdata = mty_jni_obj(env, buf, "toByteArray", "()[B");

	mty_jni_void(env, buf, "close", "()V");
	mty_jni_void(env, in, "close", "()V");

	r = mty_jni_ok(env);
	if (!r)
		goto except;

	if (jdata) {
		*responseSize = mty_jni_array_get_size(env, jdata);

		*response = MTY_Alloc(*responseSize + 1, 1);
		mty_jni_memcpy(env, *response, jdata, *responseSize);
	}

	except:

	if (urlc_obj)
		mty_jni_void(env, urlc_obj, "disconnect", "()V");

	mty_jni_free(env, jbody);
	mty_jni_free(env, jmethod);
	mty_jni_free(env, jurl);

	return r;
}
