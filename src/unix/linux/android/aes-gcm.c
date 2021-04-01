// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <string.h>
#include <stdio.h>

#include "jnih.h"

#define AES_GCM_NUM_BUFS 6
#define AES_GCM_MAX      (8 * 1024)

#define AES_GCM_ENCRYPT  0x00000001
#define AES_GCM_DECRYPT  0x00000002

struct MTY_AESGCM {
	jobject gcm;
	jobject key;

	jclass cls_gps;
	jclass cls_cipher;

	jmethodID m_gps_constructor;
	jmethodID m_cipher_init;
	jmethodID m_cipher_do_final;

	jbyteArray buf[AES_GCM_NUM_BUFS];
};

MTY_AESGCM *MTY_AESGCMCreate(const void *key)
{
	MTY_AESGCM *ctx = MTY_Alloc(1, sizeof(MTY_AESGCM));

	JNIEnv *env = MTY_GetJNIEnv();

	// The cipher handle
	jstring jalg = mty_jni_strdup(env, "AES/GCM/NoPadding");
	ctx->gcm = mty_jni_static_obj(env, "javax/crypto/Cipher", "getInstance", "(Ljava/lang/String;)Ljavax/crypto/Cipher;", jalg);
	mty_jni_retain(env, &ctx->gcm);

	// 128 bit AES key
	jbyteArray jkey = mty_jni_dup(env, key, 16);
	jstring jalg_key = mty_jni_strdup(env, "AES");
	ctx->key = mty_jni_new(env, "javax/crypto/spec/SecretKeySpec", "([BLjava/lang/String;)V", jkey, jalg_key);
	mty_jni_retain(env, &ctx->key);

	// Preload classes for performance, these need global references so the method IDs are valid
	ctx->cls_gps = (*env)->FindClass(env, "javax/crypto/spec/GCMParameterSpec");
	mty_jni_retain(env, &ctx->cls_gps);

	ctx->cls_cipher = (*env)->FindClass(env, "javax/crypto/Cipher");
	mty_jni_retain(env, &ctx->cls_cipher);

	// Preload methods for performance
	ctx->m_gps_constructor = (*env)->GetMethodID(env, ctx->cls_gps, "<init>", "(I[BII)V");
	ctx->m_cipher_init = (*env)->GetMethodID(env, ctx->cls_cipher, "init", "(ILjava/security/Key;Ljava/security/spec/AlgorithmParameterSpec;)V");

	ctx->m_cipher_do_final = (*env)->GetMethodID(env, ctx->cls_cipher, "doFinal", "([BII[B)I");

	// Preallocate byte buffers
	for (uint8_t x = 0; x < AES_GCM_NUM_BUFS; x++) {
		ctx->buf[x] = (*env)->NewByteArray(env, AES_GCM_MAX);
		mty_jni_retain(env, &ctx->buf[x]);
	}

	mty_jni_free(env, jalg_key);
	mty_jni_free(env, jkey);
	mty_jni_free(env, jalg);

	return ctx;
}

void MTY_AESGCMDestroy(MTY_AESGCM **aesgcm)
{
	if (!aesgcm || !*aesgcm)
		return;

	MTY_AESGCM *ctx = *aesgcm;

	JNIEnv *env = MTY_GetJNIEnv();

	for (uint8_t x = 0; x < AES_GCM_NUM_BUFS; x++)
		mty_jni_release(env, &ctx->buf[x]);

	mty_jni_release(env, &ctx->cls_gps);
	mty_jni_release(env, &ctx->cls_cipher);
	mty_jni_release(env, &ctx->key);
	mty_jni_release(env, &ctx->gcm);

	MTY_Free(ctx);
	*aesgcm = NULL;
}

bool MTY_AESGCMEncrypt(MTY_AESGCM *ctx, const void *nonce, const void *plainText, size_t size,
	void *tag, void *cipherText)
{
	JNIEnv *env = MTY_GetJNIEnv();

	(*env)->SetByteArrayRegion(env, ctx->buf[0], 0, 12, nonce);
	(*env)->SetByteArrayRegion(env, ctx->buf[1], 0, size, plainText);

	jobject spec = (*env)->NewObject(env, ctx->cls_gps, ctx->m_gps_constructor, 128, ctx->buf[0], 0, 12);

	(*env)->CallVoidMethod(env, ctx->gcm, ctx->m_cipher_init, AES_GCM_ENCRYPT, ctx->key, spec);
	(*env)->CallIntMethod(env, ctx->gcm, ctx->m_cipher_do_final, ctx->buf[1], 0, size, ctx->buf[2]);

	bool r = mty_jni_ok(env);
	if (r) {
		(*env)->GetByteArrayRegion(env, ctx->buf[2], 0, size, cipherText);
		(*env)->GetByteArrayRegion(env, ctx->buf[2], size, 16, tag);
	}

	mty_jni_free(env, spec);

	return r;
}

bool MTY_AESGCMDecrypt(MTY_AESGCM *ctx, const void *nonce, const void *cipherText, size_t size,
	const void *tag, void *plainText)
{
	JNIEnv *env = MTY_GetJNIEnv();

	(*env)->SetByteArrayRegion(env, ctx->buf[3], 0, 12, nonce);
	(*env)->SetByteArrayRegion(env, ctx->buf[4], 0, size, cipherText);
	(*env)->SetByteArrayRegion(env, ctx->buf[4], size, 16, tag);

	jobject spec = (*env)->NewObject(env, ctx->cls_gps, ctx->m_gps_constructor, 128, ctx->buf[3], 0, 12);

	(*env)->CallVoidMethod(env, ctx->gcm, ctx->m_cipher_init, AES_GCM_DECRYPT, ctx->key, spec);
	(*env)->CallIntMethod(env, ctx->gcm, ctx->m_cipher_do_final, ctx->buf[4], 0, size + 16, ctx->buf[5]);

	bool r = mty_jni_ok(env);
	if (r)
		(*env)->GetByteArrayRegion(env, ctx->buf[5], 0, size, plainText);

	mty_jni_free(env, spec);

	return r;
}
