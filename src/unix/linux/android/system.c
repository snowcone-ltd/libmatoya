// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include "app.h"
#include "jnih.h"

static JavaVM *SYSTEM_JVM;


// System

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
	jobject obj = mty_window_get_native(NULL, 0);

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

JNIEXPORT void JNICALL Java_group_matoya_lib_Matoya_system_1set_1jvm(JNIEnv *env, jobject obj)
{
	(*env)->GetJavaVM(env, &SYSTEM_JVM);
}

void *MTY_GetJNIEnv(void)
{
	JNIEnv *env = NULL;

	if ((*SYSTEM_JVM)->GetEnv(SYSTEM_JVM, (void **) &env, JNI_VERSION_1_6) != JNI_OK)
		(*SYSTEM_JVM)->AttachCurrentThread(SYSTEM_JVM, &env, NULL);

	return env;
}
