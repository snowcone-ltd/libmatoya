// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include <jni.h>

void mty_jni_free(JNIEnv *env, jobject ref);
void mty_jni_retain(JNIEnv *env, jobject *ref);
void mty_jni_release(JNIEnv *env, jobject *ref);

jbyteArray mty_jni_alloc(JNIEnv *env, size_t size);
jintArray mty_jni_alloc_int(JNIEnv *env, size_t size);
jbyteArray mty_jni_dup(JNIEnv *env, const void *buf, size_t size);
jintArray mty_jni_dup_int(JNIEnv *env, const void *buf, size_t size);
jobject mty_jni_wrap(JNIEnv *env, void *buf, size_t size);
jstring mty_jni_strdup(JNIEnv *env, const char *str);
jsize mty_jni_array_get_size(JNIEnv *env, jbyteArray array);
jint mty_jni_array_get_int(JNIEnv *env, jintArray array, size_t index);
void mty_jni_memcpy(JNIEnv *env, void *dst, jbyteArray jsrc, size_t size);
int32_t mty_jni_strcpy(JNIEnv *env, char *buf, size_t size, jstring str);
char *mty_jni_cstrmov(JNIEnv *env, jstring jstr);

void mty_jni_log(JNIEnv *env, jstring str);
bool mty_jni_catch(JNIEnv *env);
bool mty_jni_ok(JNIEnv *env);

jobject mty_jni_new(JNIEnv *env, const char *name, const char *sig, ...);

jobject mty_jni_static_obj(JNIEnv *env, const char *name, const char *func, const char *sig, ...);

void mty_jni_void(JNIEnv *env, jobject obj, const char *name, const char *sig, ...);
jobject mty_jni_obj(JNIEnv *env, jobject obj, const char *name, const char *sig, ...);
jint mty_jni_int(JNIEnv *env, jobject obj, const char *name, const char *sig, ...);
jfloat mty_jni_float(JNIEnv *env, jobject obj, const char *name, const char *sig, ...);
bool mty_jni_bool(JNIEnv *env, jobject obj, const char *name, const char *sig, ...);
