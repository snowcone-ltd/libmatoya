// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include "app.h"
#include "jnih.h"

#define MTY_USER_AGENT "libmatoya/v" MTY_VERSION_STRING

bool MTY_HttpRequest(const char *host, uint16_t port, bool secure, const char *method,
	const char *path, const char *headers, const void *body, size_t bodySize,
	uint32_t timeout, void **response, size_t *responseSize, uint16_t *status)
{
	*responseSize = 0;
	*response = NULL;

	jobject obj = mty_app_get_obj();
	if (!obj)
		return false;

	JNIEnv *env = MTY_GetJNIEnv();

	const char *scheme = secure ? "https" : "http";
	port = port > 0 ? port : secure ? 443 : 80;

	const char *url = MTY_SprintfDL("%s://%s:%u%s", scheme, host, port, path);

	jstring jmethod = mty_jni_strdup(env, method);
	jstring jurl = mty_jni_strdup(env, url);
	jstring jheaders = headers ? mty_jni_strdup(env, headers) : NULL;
	jstring jdua = mty_jni_strdup(env, MTY_USER_AGENT);
	jbyteArray jbody = body ? mty_jni_dup(env, body, bodySize) : NULL;
	jintArray jcode = mty_jni_alloc_int(env, 1);

	jbyteArray jdata = mty_jni_obj(env, obj, "httpRequest",
		// Args
		"("
		"Ljava/lang/String;"
		"Ljava/lang/String;"
		"Ljava/lang/String;"
		"Ljava/lang/String;"
		"I"
		"[B"
		"[I"
		")"

		"[B", // Return value

		jmethod,
		jurl,
		jheaders,
		jdua,
		timeout,
		jbody,
		jcode
	);

	*status = mty_jni_array_get_int(env, jcode, 0);

	if (jdata) {
		*responseSize = mty_jni_array_get_size(env, jdata);

		*response = MTY_Alloc(*responseSize, 1);
		mty_jni_memcpy(env, *response, jdata, *responseSize);
	}

	mty_jni_free(env, jcode);
	mty_jni_free(env, jbody);
	mty_jni_free(env, jdua);
	mty_jni_free(env, jheaders);
	mty_jni_free(env, jurl);
	mty_jni_free(env, jmethod);

	return jdata != NULL;
}
