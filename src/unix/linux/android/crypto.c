// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <string.h>
#include <stdio.h>

#include "jnih.h"


// Hash

static void crypto_hash_hmac(const char *alg, const void *input, size_t inputSize, const void *key,
	size_t keySize, void *output, size_t outputSize)
{
	JNIEnv *env = MTY_GetJNIEnv();

	jstring jalg = mty_jni_strdup(env, alg);
	jbyteArray jkey = mty_jni_dup(env, key, keySize);
	jobject okey = mty_jni_new(env, "javax/crypto/spec/SecretKeySpec", "([BLjava/lang/String;)V", jkey, jalg);

	jobject omac = mty_jni_static_obj(env, "javax/crypto/Mac", "getInstance", "(Ljava/lang/String;)Ljavax/crypto/Mac;", jalg);
	mty_jni_void(env, omac, "init", "(Ljava/security/Key;)V", okey);

	jbyteArray jin = mty_jni_dup(env, input, inputSize);
	jbyteArray jout = mty_jni_obj(env, omac, "doFinal", "([B)[B", jin);

	mty_jni_memcpy(env, output, jout, outputSize);

	mty_jni_free(env, jout);
	mty_jni_free(env, jin);
	mty_jni_free(env, omac);
	mty_jni_free(env, okey);
	mty_jni_free(env, jkey);
	mty_jni_free(env, jalg);
}

static void crypto_hash(const char *alg, const void *input, size_t inputSize, void *output, size_t outputSize)
{
	JNIEnv *env = MTY_GetJNIEnv();

	jstring jalg = mty_jni_strdup(env, alg);
	jobject obj = mty_jni_static_obj(env, "java/security/MessageDigest", "getInstance", "(Ljava/lang/String;)Ljava/security/MessageDigest;", jalg);

	jobject bb = mty_jni_wrap(env, (void *) input, inputSize);
	mty_jni_void(env, obj, "update", "(Ljava/nio/ByteBuffer;)V", bb);

	jbyteArray b = mty_jni_obj(env, obj, "digest", "()[B");
	mty_jni_memcpy(env, output, b, outputSize);

	mty_jni_free(env, b);
	mty_jni_free(env, bb);
	mty_jni_free(env, obj);
	mty_jni_free(env, jalg);
}

void MTY_CryptoHash(MTY_Algorithm algo, const void *input, size_t inputSize, const void *key,
	size_t keySize, void *output, size_t outputSize)
{
	switch (algo) {
		case MTY_ALGORITHM_SHA1:
			if (key && keySize > 0) {
				crypto_hash_hmac("HmacSHA1", input, inputSize, key, keySize, output, outputSize);
			} else {
				crypto_hash("SHA-1", input, inputSize, output, outputSize);
			}
			break;
		case MTY_ALGORITHM_SHA1_HEX: {
			uint8_t bytes[MTY_SHA1_SIZE];
			MTY_CryptoHash(MTY_ALGORITHM_SHA1, input, inputSize, key, keySize, bytes, MTY_SHA1_SIZE);
			MTY_BytesToHex(bytes, sizeof(bytes), output, outputSize);
			break;
		}
		case MTY_ALGORITHM_SHA256:
			if (key && keySize > 0) {
				crypto_hash_hmac("HmacSHA256", input, inputSize, key, keySize, output, outputSize);
			} else {
				crypto_hash("SHA-256", input, inputSize, output, outputSize);
			}
			break;
		case MTY_ALGORITHM_SHA256_HEX: {
			uint8_t bytes[MTY_SHA256_SIZE];
			MTY_CryptoHash(MTY_ALGORITHM_SHA256, input, inputSize, key, keySize, bytes, MTY_SHA256_SIZE);
			MTY_BytesToHex(bytes, sizeof(bytes), output, outputSize);
			break;
		}
	}
}


// Random

void MTY_GetRandomBytes(void *buf, size_t size)
{
	JNIEnv *env = MTY_GetJNIEnv();

	jobject obj = mty_jni_new(env, "java/security/SecureRandom", "()V");

	jbyteArray b = mty_jni_alloc(env, size);
	mty_jni_void(env, obj, "nextBytes", "([B)V", b);

	mty_jni_memcpy(env, buf, b, size);

	mty_jni_free(env, b);
	mty_jni_free(env, obj);
}
