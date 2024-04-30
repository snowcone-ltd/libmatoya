// Copyright (c) Ronald Huveneers <ronald@huuf.info>
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

static bool validate_unknown_aesgcm(void)
{
	const char *password1 = "1234567890123456789012345678901212345678901234567890123456789012";

	for (uint32_t i = 0; i < 64; i++) {
		if (i != 16 && i != 32) {
			MTY_AESGCM *aes1 = MTY_AESGCMCreate(password1, i);
			if (aes1 != NULL) {
				MTY_AESGCMDestroy(&aes1);
				test_cmp("MTY_AESGCMCreate %u", aes1 == NULL);
			}
		}
	}

	return true;
}

static bool validate_aesgcm_128(void)
{
	const uint8_t pre_encrypted[] = {
		0xE5, 0x19, 0x0B, 0x88, 0x89, 0xE4, 0x3D, 0x6B, 0xCC, 0xD1, 0x59, 0xC7, 0x39, 0x46, 0x34, 0x64,
		0xB0, 0x11, 0x00, 0x24, 0x35, 0xB6, 0xC1, 0xF8, 0x0C, 0xCE, 0xB5, 0x82, 0x4E, 0x5B, 0xFD, 0xFD,
		0x42, 0x5C, 0x16, 0x67, 0x0D, 0x6B, 0x5D, 0x57, 0x91, 0x2A, 0x84, 0x65, 0xF3, 0x4E, 0x4E, 0x75,
		0x8C, 0x00, 0x3A, 0xF4, 0xEF, 0x5F, 0x42, 0xA2, 0x79, 0x69, 0x48, 0xED, 0xE5, 0x85, 0x49, 0xAE,
		0x24, 0x48, 0xDA
	};
	const char *password1 = "1234567890123456";
	const char *password2 = "secret password!";
	char nonce1[] = "123456789012";
	const char *nonce2 = "s3cr3t3stpas";
	const char *plain     = "The Quick Brown Fox Jumped Over The Super Lazy Dog!";
	const size_t len = strlen(plain);
	char *encrypted1 = calloc(1, len);
	char *encrypted2 = calloc(1, len);
	char *encrypted3 = calloc(1, len);
	char *decrypted = calloc(1, len + 1);
	char *tag1 = calloc(1, 16);

	MTY_AESGCM *aes1 = MTY_AESGCMCreate(password1, 16);
	test_cmp("MTY_AESGCMCreate 128", aes1 != NULL);
	MTY_AESGCMEncrypt(aes1, nonce1, plain, len, tag1, encrypted1);
	bool same = 0 == memcmp(pre_encrypted, encrypted1, len);
	if (!same) {
		char buffer[256] = {0};
		MTY_BytesToHex(pre_encrypted, len, buffer, 256);
		printf("Pre: %s\n", buffer);
		MTY_BytesToHex(encrypted1, len, buffer, 256);
		printf("Our: %s\n", buffer);
	}
	test_cmp("MTY_AESGCMEncrypt 128", same);

	// Succeed
	bool ok = MTY_AESGCMDecrypt(aes1, nonce1, encrypted1, len, tag1, decrypted);
	test_cmp("MTY_AESGCMDecrypt 128", ok);

	// Encrypted data modified -- fail
	encrypted1[3] = ~encrypted1[3];
	ok = MTY_AESGCMDecrypt(aes1, nonce1, encrypted1, len, tag1, decrypted);
	test_cmp("MTY_AESGCMDecrypt 128", !ok);

	// Tag modified -- fail
	encrypted1[3] = ~encrypted1[3];
	tag1[3] = ~tag1[3];
	ok = MTY_AESGCMDecrypt(aes1, nonce1, encrypted1, len, tag1, decrypted);
	test_cmp("MTY_AESGCMDecrypt 128", !ok);

	// Nonce modified -- fail
	tag1[3] = ~tag1[3];
	nonce1[3] = ~nonce1[3];
	ok = MTY_AESGCMDecrypt(aes1, nonce1, encrypted1, len, tag1, decrypted);
	test_cmp("MTY_AESGCMDecrypt 128", !ok);

	// Back to normal -- succeed
	nonce1[3] = ~nonce1[3];
	ok = MTY_AESGCMDecrypt(aes1, nonce1, encrypted1, len, tag1, decrypted);
	test_cmp("MTY_AESGCMDecrypt 128", ok);

	MTY_AESGCMDestroy(&aes1);

	aes1 = MTY_AESGCMCreate(password2, 16);
	MTY_AESGCMEncrypt(aes1, nonce1, plain, len, tag1, encrypted2);
	MTY_AESGCMDestroy(&aes1);
	int32_t cmp_encr_1 = memcmp(encrypted1, encrypted2, len);


	aes1 = MTY_AESGCMCreate(password1, 16);
	test_cmp("MTY_AESGCMCreate 128", aes1 != NULL);
	MTY_AESGCMEncrypt(aes1, nonce2, plain, len, tag1, encrypted3);
	MTY_AESGCMDestroy(&aes1);
	int32_t cmp_encr_2 = memcmp(encrypted1, encrypted3, len);
	int32_t cmp_encr_3 = memcmp(encrypted2, encrypted3, len);

	aes1 = MTY_AESGCMCreate(password1, 16);
	MTY_AESGCMDecrypt(aes1, nonce1, encrypted1, len, tag1, decrypted);
	MTY_AESGCMDestroy(&aes1);
	int32_t cmp_decr = strcmp(decrypted, plain);
	free(encrypted3);
	free(encrypted2);
	free(encrypted1);
	free(decrypted);
	free(tag1);

	test_cmp("MTY_AESGCMDestroy 128", aes1 == NULL);

	test_cmp("MTY_AESGCMDecrypt 128", cmp_decr == 0);
	test_cmp("MTY_AESGCMEncrypt 128", cmp_encr_1 != 0);
	test_cmp("MTY_AESGCMEncrypt 128", cmp_encr_2 != 0);
	test_cmp("MTY_AESGCMEncrypt 128", cmp_encr_3 != 0);

	return true;
}

static bool validate_aesgcm_256(void)
{
	const uint8_t pre_encrypted[] = {
		0xD4, 0x9A, 0x98, 0xCE, 0x53, 0x34, 0xA8, 0x82, 0x8E, 0x32, 0x7D, 0xF6, 0xBE, 0x62, 0x06, 0x7C,
		0x58, 0xD1, 0x6E, 0xD4, 0x35, 0x87, 0xD0, 0x04, 0x6C, 0x87, 0x68, 0xD2, 0x97, 0x8B, 0xD6, 0x91,
		0x28, 0xDB, 0xC5, 0x00, 0x79, 0xC1, 0x43, 0x47, 0x0D, 0x24, 0x60, 0x0C, 0x34, 0xE1, 0x30, 0xFD,
		0xE2, 0x5F, 0xBB, 0x68, 0x63, 0xFA, 0x5D, 0x72, 0x81, 0xFD, 0x2F, 0x73, 0x3A, 0x34, 0x0B, 0x05,
		0x20, 0x8A, 0x4D
	};
	const char *password1 = "12345678901234567890123456789012";
	const char *password2 = "secret password!secret password!";
	char nonce1[] = "123456789012";
	const char *nonce2 = "s3cr3t3stpas";
	const char *plain     = "The Quick Brown Fox Jumped Over The Super Lazy Dog!";
	const size_t len = strlen(plain);
	char *encrypted1 = calloc(1, len);
	char *encrypted2 = calloc(1, len);
	char *encrypted3 = calloc(1, len);
	char *decrypted = calloc(1, len + 1);
	char *tag1 = calloc(1, 32);

	MTY_AESGCM *aes1 = MTY_AESGCMCreate(password1, 32);
	test_cmp("MTY_AESGCMCreate 256", aes1 != NULL);
	MTY_AESGCMEncrypt(aes1, nonce1, plain, len, tag1, encrypted1);
	bool same = 0 == memcmp(pre_encrypted, encrypted1, len);
	if (!same) {
		char buffer[256] = {0};
		MTY_BytesToHex(pre_encrypted, len, buffer, 256);
		printf("Pre: %s\n", buffer);
		MTY_BytesToHex(encrypted1, len, buffer, 256);
		printf("Our: %s\n", buffer);
	}
	test_cmp("MTY_AESGCMEncrypt 256", same);

	// Succeed
	bool ok = MTY_AESGCMDecrypt(aes1, nonce1, encrypted1, len, tag1, decrypted);
	test_cmp("MTY_AESGCMDecrypt 256", ok);

	// Encrypted data modified -- fail
	encrypted1[3] = ~encrypted1[3];
	ok = MTY_AESGCMDecrypt(aes1, nonce1, encrypted1, len, tag1, decrypted);
	test_cmp("MTY_AESGCMDecrypt 256", !ok);

	// Tag modified -- fail
	encrypted1[3] = ~encrypted1[3];
	tag1[3] = ~tag1[3];
	ok = MTY_AESGCMDecrypt(aes1, nonce1, encrypted1, len, tag1, decrypted);
	test_cmp("MTY_AESGCMDecrypt 256", !ok);

	// Nonce modified -- fail
	tag1[3] = ~tag1[3];
	nonce1[3] = ~nonce1[3];
	ok = MTY_AESGCMDecrypt(aes1, nonce1, encrypted1, len, tag1, decrypted);
	test_cmp("MTY_AESGCMDecrypt 256", !ok);

	// Back to normal -- succeed
	nonce1[3] = ~nonce1[3];
	ok = MTY_AESGCMDecrypt(aes1, nonce1, encrypted1, len, tag1, decrypted);
	test_cmp("MTY_AESGCMDecrypt 256", ok);

	MTY_AESGCMDestroy(&aes1);

	aes1 = MTY_AESGCMCreate(password2, 32);
	MTY_AESGCMEncrypt(aes1, nonce1, plain, len, tag1, encrypted2);
	MTY_AESGCMDestroy(&aes1);
	int32_t cmp_encr_1 = memcmp(encrypted1, encrypted2, len);


	aes1 = MTY_AESGCMCreate(password1, 32);
	test_cmp("MTY_AESGCMCreate 256", aes1 != NULL);
	MTY_AESGCMEncrypt(aes1, nonce2, plain, len, tag1, encrypted3);
	MTY_AESGCMDestroy(&aes1);
	int32_t cmp_encr_2 = memcmp(encrypted1, encrypted3, len);
	int32_t cmp_encr_3 = memcmp(encrypted2, encrypted3, len);

	aes1 = MTY_AESGCMCreate(password1, 32);
	MTY_AESGCMDecrypt(aes1, nonce1, encrypted1, len, tag1, decrypted);
	MTY_AESGCMDestroy(&aes1);
	int32_t cmp_decr = strcmp(decrypted, plain);
	free(encrypted3);
	free(encrypted2);
	free(encrypted1);
	free(decrypted);
	free(tag1);

	test_cmp("MTY_AESGCMDestroy 256", aes1 == NULL);

	test_cmp("MTY_AESGCMDecrypt 256", cmp_decr == 0);
	test_cmp("MTY_AESGCMEncrypt 256", cmp_encr_1 != 0);
	test_cmp("MTY_AESGCMEncrypt 256", cmp_encr_2 != 0);
	test_cmp("MTY_AESGCMEncrypt 256", cmp_encr_3 != 0);

	return true;
}

static bool validate_random(void)
{
	int32_t random_size = 1 * 1024 * 1024;
	uint32_t distribution[256] = {0};
	uint8_t *buffer = calloc(1, random_size);
	MTY_GetRandomBytes(buffer, random_size);

	for (int32_t i = 0; i < random_size; i++) {
		distribution[buffer[i]]++;
	}

	uint32_t sm_occ = distribution[0];
	uint32_t lgs_occ = distribution[0];
	for (int32_t i = 1; i < 256; i++) {
		if (distribution[i] > lgs_occ)
			lgs_occ = distribution[i];
		else if (distribution[i] < sm_occ)
			sm_occ = distribution[i];
	}

	double ds = (1.0 / (double)random_size) * (double)sm_occ;
	double dl = (1.0 / (double)random_size) * (double)lgs_occ;

	test_cmp_("Dist Small", ds >= 0.0035 && ds <= 0.0045, ds, ": %.4f");
	test_cmp_("Dist Large", dl >= 0.0035 && dl <= 0.0045, dl, ": %.4f");

	memset(distribution, 0, 256 * sizeof(uint32_t));
	for (int32_t i = 0; i < random_size; i++) {
		uint32_t rng = MTY_GetRandomUInt(0x00, 0xFF + 1);
		if (rng > 0xFF)
			test_cmp_("MTY_GetRandomUInt", rng <= 0xFF, rng, ": %u");
		distribution[rng]++;
	}

	sm_occ = distribution[0];
	lgs_occ = distribution[0];
	for (int32_t i = 1; i < 256; i++) {
		if (distribution[i] > lgs_occ)
			lgs_occ = distribution[i];
		else if (distribution[i] < sm_occ)
			sm_occ = distribution[i];
	}

	ds = (1.0 / (double)random_size) * (double)sm_occ;
	dl = (1.0 / (double)random_size) * (double)lgs_occ;

	test_cmp_("Dist Small", ds >= 0.0035 && ds <= 0.0045, ds, ": %.4f");
	test_cmp_("Dist Large", dl >= 0.0035 && dl <= 0.0045, dl, ": %.4f");

	return true;
}

static bool validate_sha256_hmac(const char *hmac, const char *data, const char *result)
{
	size_t len_hmac = strlen(hmac);
	char *hmac_bytes = calloc(1, len_hmac / 2);
	MTY_HexToBytes(hmac, hmac_bytes, len_hmac / 2);

	size_t len_data = strlen(data);
	char *data_bytes = calloc(1, len_data / 2);
	MTY_HexToBytes(data, data_bytes, len_data / 2);

	char sha256_shash[MTY_SHA256_HEX_MAX] = {0};
	MTY_CryptoHash(MTY_ALGORITHM_SHA256_HEX, data_bytes, len_data / 2, hmac_bytes, len_hmac / 2, sha256_shash, MTY_SHA256_HEX_MAX);

	char tmp_hmac[20] = {0};
	snprintf(tmp_hmac, 20, "SHA256(\"%4s", data);
	int32_t strcmp_v = strcmp(result, sha256_shash);

	free(hmac_bytes);
	free(data_bytes);

	test_cmp_(tmp_hmac, strcmp_v == 0, sha256_shash, ": \"%s\"")
	return true;
}

static bool validate_cryptohash(void)
{
	/*
	MTY_ALGORITHM_SHA1
	MTY_ALGORITHM_SHA1_HEX
	MTY_ALGORITHM_SHA256
	MTY_ALGORITHM_SHA256_HEX
	*/
	char *tmp_a = calloc(1, 1000000);
	memset(tmp_a, 'a', 1000000);
	int32_t strcmp_v = 0;

	char sha_shash[MTY_SHA1_HEX_MAX] = {0};
	MTY_CryptoHash(MTY_ALGORITHM_SHA1_HEX, "abc", 3, NULL, 0, sha_shash, MTY_SHA1_HEX_MAX);
	strcmp_v = strcmp("a9993e364706816aba3e25717850c26c9cd0d89d", sha_shash);
	test_cmp_("SHA1(\"abc\")", strcmp_v == 0, sha_shash, ": \"%s\"")

	MTY_CryptoHash(MTY_ALGORITHM_SHA1_HEX, "", 0, NULL, 0, sha_shash, MTY_SHA1_HEX_MAX);
	strcmp_v = strcmp("da39a3ee5e6b4b0d3255bfef95601890afd80709", sha_shash);
	test_cmp_("SHA1(\"\")", strcmp_v == 0, sha_shash, ": \"%s\"")

	MTY_CryptoHash(MTY_ALGORITHM_SHA1_HEX, "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 448 / 8, NULL, 0, sha_shash, MTY_SHA1_HEX_MAX);
	strcmp_v = strcmp("84983e441c3bd26ebaae4aa1f95129e5e54670f1", sha_shash);
	test_cmp_("SHA1(\"abcdbcde...\")", strcmp_v == 0, sha_shash, ": \"%s\"")

	MTY_CryptoHash(MTY_ALGORITHM_SHA1_HEX, "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu", 896 / 8, NULL, 0, sha_shash, MTY_SHA1_HEX_MAX);
	strcmp_v = strcmp("a49b2446a02c645bf419f995b67091253a04a259", sha_shash);
	test_cmp_("SHA1(\"abcdefgh...\")", strcmp_v == 0, sha_shash, ": \"%s\"")

	MTY_CryptoHash(MTY_ALGORITHM_SHA1_HEX, tmp_a, 1000000, NULL, 0, sha_shash, MTY_SHA1_HEX_MAX);
	strcmp_v = strcmp("34aa973cd4c4daa4f61eeb2bdbad27316534016f", sha_shash);
	test_cmp_("SHA1(\"aaaaaaaa...\")", strcmp_v == 0, sha_shash, ": \"%s\"")

	char sha256_shash[MTY_SHA256_HEX_MAX] = {0};
	MTY_CryptoHash(MTY_ALGORITHM_SHA256_HEX, "abc", 3, NULL, 0, sha256_shash, MTY_SHA256_HEX_MAX);
	strcmp_v = strcmp("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", sha256_shash);
	test_cmp_("SHA256(\"abc\")", strcmp_v == 0, sha256_shash, ": \"%s\"")

	MTY_CryptoHash(MTY_ALGORITHM_SHA256_HEX, "", 0, NULL, 0, sha256_shash, MTY_SHA256_HEX_MAX);
	strcmp_v = strcmp("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", sha256_shash);
	test_cmp_("SHA256(\"\")", strcmp_v == 0, sha256_shash, ": \"%s\"")

	MTY_CryptoHash(MTY_ALGORITHM_SHA256_HEX, "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 448 / 8, NULL, 0, sha256_shash, MTY_SHA256_HEX_MAX);
	strcmp_v = strcmp("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1", sha256_shash);
	test_cmp_("SHA256(\"abcdbc...\")", strcmp_v == 0, sha256_shash, ": \"%s\"")

	MTY_CryptoHash(MTY_ALGORITHM_SHA256_HEX, "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu", 896 / 8, NULL, 0, sha256_shash, MTY_SHA256_HEX_MAX);
	strcmp_v = strcmp("cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1", sha256_shash);
	test_cmp_("SHA256(\"abcdef...\")", strcmp_v == 0, sha256_shash, ": \"%s\"")

	MTY_CryptoHash(MTY_ALGORITHM_SHA256_HEX, tmp_a, 1000000, NULL, 0, sha256_shash, MTY_SHA256_HEX_MAX);
	strcmp_v = strcmp("cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0", sha256_shash);
	test_cmp_("SHA256(\"aaaaaa...\")", strcmp_v == 0, sha256_shash, ": \"%s\"")

	free(tmp_a);

	if (!validate_sha256_hmac("0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b",
							"4869205468657265",
							"b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7"))
		return false;

	if (!validate_sha256_hmac("4a656665",
							"7768617420646f2079612077616e7420666f72206e6f7468696e673f",
							"5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843"))
		return false;

	if (!validate_sha256_hmac("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
							"dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd",
							"773ea91e36800e46854db8ebd09181a72959098b3ef8c122d9635514ced565fe"))
		return false;

	if (!validate_sha256_hmac("0102030405060708090a0b0c0d0e0f10111213141516171819",
							"cdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcd",
							"82558a389a443c0ea4cc819899f2083a85f0faa3e578f8077a2e3ff46729665b"))
		return false;

	if (!validate_sha256_hmac("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
							"54657374205573696e67204c6172676572205468616e20426c6f636b2d53697a65204b6579202d2048617368204b6579204669727374",
							"60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54"))
		return false;

	if (!validate_sha256_hmac("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
							"5468697320697320612074657374207573696e672061206c6172676572207468616e20626c6f636b2d73697a65206b657920616e642061206c6172676572207468616e20626c6f636b2d73697a6520646174612e20546865206b6579206e6565647320746f20626520686173686564206265666f7265206265696e6720757365642062792074686520484d414320616c676f726974686d2e",
							"9b09ffa71b942fcb27635fbcd5b0e944bfdc63644f0713938a7f51535c3a35e2"))
		return false;

	return true;
}

static bool validate_djb2(void)
{
	uint32_t crc = MTY_DJB2("123456789");
	test_cmp_("CRC32 1", crc == 0x35cdbb82, "123456789", ": \"%s\"");
	crc = MTY_DJB2("The quick brown fox jumps over the lazy dog");
	test_cmp_("CRC32 2", crc == 0x34cc38de, "The quick brown fox jumps over the lazy dog", ": \"%s\"");
	crc = MTY_DJB2("");
	test_cmp_("CRC32 3", crc == 0x00001505, "", ": \"%s\"");

	return true;

}

static bool validate_crc32(void)
{
	uint32_t crc = MTY_CRC32(0, "123456789", 9);
	test_cmp_("CRC32 1", crc == 0xcbf43926, "123456789", ": \"%s\"");
	crc = MTY_CRC32(0, "The quick brown fox jumps over the lazy dog", 43);
	test_cmp_("CRC32 2", crc == 0x414FA339, "The quick brown fox jumps over the lazy dog", ": \"%s\"");
	crc = MTY_CRC32(0, "", 0);
	test_cmp_("CRC32 3", crc == 0x00000000, "", ": \"%s\"");

	return true;
}

static bool crypto_main(void)
{
	if (!validate_crc32())
		return false;

	if (!validate_djb2())
		return false;

	if (!validate_cryptohash())
		return false;

	if (!validate_random())
		return false;

	if (!validate_unknown_aesgcm())
		return false;

	if (!validate_aesgcm_128())
		return false;

	if (!validate_aesgcm_256())
		return false;

	return true;
}
