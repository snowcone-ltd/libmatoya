// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#define LIBCRYPTO_EXTERN
#include "libcrypto.h"

#include "sym.h"

static MTY_Atomic32 LIBCRYPTO_LOCK;
static MTY_SO *LIBCRYPTO_SO;
static bool LIBCRYPTO_INIT;

static void __attribute__((destructor)) libcrypto_global_destroy(void)
{
	MTY_GlobalLock(&LIBCRYPTO_LOCK);

	MTY_SOUnload(&LIBCRYPTO_SO);
	LIBCRYPTO_INIT = false;

	MTY_GlobalUnlock(&LIBCRYPTO_LOCK);
}

bool mty_libcrypto_global_init(void)
{
	MTY_GlobalLock(&LIBCRYPTO_LOCK);

	if (!LIBCRYPTO_INIT) {
		bool r = true;
		LIBCRYPTO_SO = MTY_SOLoad("libcrypto.so.1.1");

		if (!LIBCRYPTO_SO)
			LIBCRYPTO_SO = MTY_SOLoad("libcrypto.so.1.0.0");

		if (!LIBCRYPTO_SO) {
			r = false;
			goto except;
		}

		LOAD_SYM(LIBCRYPTO_SO, EVP_aes_128_gcm);
		LOAD_SYM(LIBCRYPTO_SO, EVP_CIPHER_CTX_new);
		LOAD_SYM(LIBCRYPTO_SO, EVP_CIPHER_CTX_free);
		LOAD_SYM(LIBCRYPTO_SO, EVP_CipherInit_ex);
		LOAD_SYM(LIBCRYPTO_SO, EVP_EncryptUpdate);
		LOAD_SYM(LIBCRYPTO_SO, EVP_DecryptUpdate);
		LOAD_SYM(LIBCRYPTO_SO, EVP_EncryptFinal_ex);
		LOAD_SYM(LIBCRYPTO_SO, EVP_DecryptFinal_ex);
		LOAD_SYM(LIBCRYPTO_SO, EVP_CIPHER_CTX_ctrl);

		LOAD_SYM(LIBCRYPTO_SO, EVP_sha1);
		LOAD_SYM(LIBCRYPTO_SO, EVP_sha256);
		LOAD_SYM(LIBCRYPTO_SO, SHA1);
		LOAD_SYM(LIBCRYPTO_SO, SHA256);
		LOAD_SYM(LIBCRYPTO_SO, HMAC);

		LOAD_SYM(LIBCRYPTO_SO, RAND_bytes);

		except:

		if (!r)
			libcrypto_global_destroy();

		LIBCRYPTO_INIT = r;
	}

	MTY_GlobalUnlock(&LIBCRYPTO_LOCK);

	return LIBCRYPTO_INIT;
}
