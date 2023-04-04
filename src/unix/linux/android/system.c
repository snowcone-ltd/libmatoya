// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include "app-os.h"
#include "jnih.h"

static JavaVM *SYSTEM_JVM;


// System

const char *MTY_GetSOExtension(void)
{
	return "so";
}

uint32_t MTY_GetPlatform(void)
{
	uint32_t v = MTY_OS_ANDROID;

	int32_t level = android_get_device_api_level();

	v |= (uint32_t) level << 8;

	return v;
}

uint32_t MTY_GetPlatformNoWeb(void)
{
	return MTY_GetPlatform();
}

void MTY_HandleProtocol(const char *uri, void *token)
{
	jobject obj = mty_app_get_obj();

	if (obj) {
		JNIEnv *env = MTY_GetJNIEnv();

		jstring juri = mty_jni_strdup(env, uri);
		mty_jni_void(env, obj, "openURI", "(Ljava/lang/String;)V", juri);
		mty_jni_free(env, juri);
	}
}

const char *MTY_GetProcessPath(void)
{
	return "/app";
}

bool MTY_RestartProcess(char * const *argv)
{
	return false;
}


// JNIEnv

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved)
{
	// FIXME This assumes libmatoya is the arbiter of all JNI
	// which may not be a great assumption

	SYSTEM_JVM = vm;

	return JNI_VERSION_1_6;
}

void *MTY_GetJNIEnv(void)
{
	JNIEnv *env = NULL;

	if ((*SYSTEM_JVM)->GetEnv(SYSTEM_JVM, (void **) &env, JNI_VERSION_1_6) != JNI_OK)
		(*SYSTEM_JVM)->AttachCurrentThread(SYSTEM_JVM, &env, NULL);

	return env;
}
