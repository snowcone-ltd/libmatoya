// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"
#include "jnih.h"

#include <string.h>
#include <stdio.h>


// References

void mty_jni_free(JNIEnv *env, jobject ref)
{
	if (!ref)
		return;

	(*env)->DeleteLocalRef(env, ref);
}

void mty_jni_retain(JNIEnv *env, jobject *ref)
{
	jobject old = *ref;

	*ref = (*env)->NewGlobalRef(env, old);

	mty_jni_free(env, old);
}

void mty_jni_release(JNIEnv *env, jobject *ref)
{
	if (!ref || !*ref)
		return;

	(*env)->DeleteGlobalRef(env, *ref);
	*ref = NULL;
}


// Memory

jbyteArray mty_jni_alloc(JNIEnv *env, size_t size)
{
	return (*env)->NewByteArray(env, size);
}

jintArray mty_jni_alloc_int(JNIEnv *env, size_t size)
{
	return (*env)->NewIntArray(env, size);
}

jbyteArray mty_jni_dup(JNIEnv *env, const void *buf, size_t size)
{
	if (!buf || size == 0)
		return NULL;

	jbyteArray dup = (*env)->NewByteArray(env, size);
	(*env)->SetByteArrayRegion(env, dup, 0, size, buf);

	return dup;
}

jobject mty_jni_wrap(JNIEnv *env, void *buf, size_t size)
{
	return (*env)->NewDirectByteBuffer(env, buf, size);
}

jstring mty_jni_strdup(JNIEnv *env, const char *str)
{
	return (*env)->NewStringUTF(env, str);
}

jsize mty_jni_array_get_size(JNIEnv *env, jbyteArray array)
{
	return (*env)->GetArrayLength(env, array);
}

jint mty_jni_array_get_int(JNIEnv *env, jintArray array, size_t index)
{
	jint *c = (*env)->GetIntArrayElements(env, array, JNI_FALSE);
	jint val = c[index];

	(*env)->ReleaseIntArrayElements(env, array, c, JNI_ABORT);

	return val;
}

void mty_jni_memcpy(JNIEnv *env, void *dst, jbyteArray jsrc, size_t size)
{
	jsize jsize = (*env)->GetArrayLength(env, jsrc);
	(*env)->GetByteArrayRegion(env, jsrc, 0, (size_t) jsize < size ? (size_t) jsize : size, dst);
}

void mty_jni_strcpy(JNIEnv *env, char *buf, size_t size, jstring str)
{
	const char *cstr = (*env)->GetStringUTFChars(env, str, NULL);

	snprintf(buf, size, "%s", cstr);

	(*env)->ReleaseStringUTFChars(env, str, cstr);
}

char *mty_jni_cstrdup(JNIEnv *env, jstring jstr)
{
	const char *cstr = (*env)->GetStringUTFChars(env, jstr, NULL);
	char *str = MTY_Strdup(cstr);

	(*env)->ReleaseStringUTFChars(env, jstr, cstr);
	(*env)->DeleteLocalRef(env, jstr);

	return str;
}


// Exceptions

void mty_jni_log(JNIEnv *env, jstring str)
{
	const char *cstr = (*env)->GetStringUTFChars(env, str, NULL);

	MTY_Log("%s", cstr);

	(*env)->ReleaseStringUTFChars(env, str, cstr);
}

bool mty_jni_ok(JNIEnv *env)
{
	jthrowable ex = (*env)->ExceptionOccurred(env);

	if (ex) {
		(*env)->ExceptionClear(env);

		jstring jstr = mty_jni_obj(env, ex, "toString", "()Ljava/lang/String;");

		mty_jni_log(env, jstr);
		mty_jni_free(env, jstr);

		mty_jni_free(env, ex);

		return false;
	}

	return true;
}


// Constructor

jobject mty_jni_new(JNIEnv *env, const char *name, const char *sig, ...)
{
	if ((*env)->ExceptionCheck(env))
		return NULL;

	va_list args;
	va_start(args, sig);

	jclass cls = (*env)->FindClass(env, name);
	jmethodID mid = (*env)->GetMethodID(env, cls, "<init>", sig);
	jobject obj = (*env)->NewObjectV(env, cls, mid, args);

	va_end(args);

	mty_jni_free(env, cls);

	return obj;
}


// Static methods

jobject mty_jni_static_obj(JNIEnv *env, const char *name, const char *func, const char *sig, ...)
{
	if ((*env)->ExceptionCheck(env))
		return NULL;

	va_list args;
	va_start(args, sig);

	jclass cls = (*env)->FindClass(env, name);
	jmethodID mid = (*env)->GetStaticMethodID(env, cls, func, sig);
	jobject obj = (*env)->CallStaticObjectMethodV(env, cls, mid, args);

	va_end(args);

	mty_jni_free(env, cls);

	return obj;
}


// Object methods

void mty_jni_void(JNIEnv *env, jobject obj, const char *name, const char *sig, ...)
{
	if ((*env)->ExceptionCheck(env))
		return;

	va_list args;
	va_start(args, sig);

	jclass cls = (*env)->GetObjectClass(env, obj);

	jmethodID mid = (*env)->GetMethodID(env, cls, name, sig);
	(*env)->CallVoidMethodV(env, obj, mid, args);

	va_end(args);

	mty_jni_free(env, cls);
}

jobject mty_jni_obj(JNIEnv *env, jobject obj, const char *name, const char *sig, ...)
{
	if ((*env)->ExceptionCheck(env))
		return NULL;

	va_list args;
	va_start(args, sig);

	jclass cls = (*env)->GetObjectClass(env, obj);
	jmethodID mid = (*env)->GetMethodID(env, cls, name, sig);
	jobject r = (*env)->CallObjectMethodV(env, obj, mid, args);

	va_end(args);

	mty_jni_free(env, cls);

	return r;
}

jint mty_jni_int(JNIEnv *env, jobject obj, const char *name, const char *sig, ...)
{
	if ((*env)->ExceptionCheck(env))
		return 0;

	va_list args;
	va_start(args, sig);

	jclass cls = (*env)->GetObjectClass(env, obj);
	jmethodID mid = (*env)->GetMethodID(env, cls, name, sig);
	jint r = (*env)->CallIntMethodV(env, obj, mid, args);

	va_end(args);

	mty_jni_free(env, cls);

	return r;
}

jfloat mty_jni_float(JNIEnv *env, jobject obj, const char *name, const char *sig, ...)
{
	if ((*env)->ExceptionCheck(env))
		return 0;

	va_list args;
	va_start(args, sig);

	jclass cls = (*env)->GetObjectClass(env, obj);
	jmethodID mid = (*env)->GetMethodID(env, cls, name, sig);
	jfloat r = (*env)->CallFloatMethodV(env, obj, mid, args);

	va_end(args);

	mty_jni_free(env, cls);

	return r;
}

bool mty_jni_bool(JNIEnv *env, jobject obj, const char *name, const char *sig, ...)
{
	if ((*env)->ExceptionCheck(env))
		return false;

	va_list args;
	va_start(args, sig);

	jclass cls = (*env)->GetObjectClass(env, obj);
	jmethodID mid = (*env)->GetMethodID(env, cls, name, sig);
	bool r = (*env)->CallBooleanMethodV(env, obj, mid, args);

	va_end(args);

	mty_jni_free(env, cls);

	return r;
}
