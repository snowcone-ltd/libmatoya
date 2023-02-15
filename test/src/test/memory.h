// Memory Test
// Copyright 2021, Jamie Blanks
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

static bool test_specific_printf(const char *function, char *buffer, const char *compare)
{
	int32_t cmp_val = strcmp(buffer, compare);
	free(buffer);
	test_cmp_(function, cmp_val == 0, compare, ": %s");
	return true;
}

static bool memory_printf(void)
{
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%d", 42), "42");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%d", -42), "-42");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%5d", 42), "   42");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%5d", -42), "  -42");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%15d", 42), "             42");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%15d", -42), "            -42");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%15d", -42), "            -42");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%15.3f", -42.987), "        -42.987");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%15.3f", 42.987), "         42.987");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%s", "Hello testing"), "Hello testing");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%d", 1024), "1024");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%d", -1024), "-1024");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%i", 1024), "1024");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%i", -1024), "-1024");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%u", 1024), "1024");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%u", 4294966272U), "4294966272");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%o", 511), "777");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%o", 4294966785U), "37777777001");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%x", 305441741), "1234abcd");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%x", 3989525555U), "edcb5433");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%X", 305441741), "1234ABCD");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%X", 3989525555U), "EDCB5433");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%c", 'x'), "x");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%+d", 42), "+42");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%+d", -42), "-42");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%+5d", 42), "  +42");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%+5d", -42), "  -42");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%+15d", 42), "            +42");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%+15d", -42), "            -42");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%s", "Hello testing"), "Hello testing");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%+d", 1024), "+1024");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%+d", -1024), "-1024");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%+i", 1024), "+1024");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%+i", -1024), "-1024");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%u", 1024), "1024");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%u", 4294966272U), "4294966272");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%o", 511), "777");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%o", 4294966785U), "37777777001");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%x", 305441741), "1234abcd");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%x", 3989525555U), "edcb5433");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%X", 305441741), "1234ABCD");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%X", 3989525555U), "EDCB5433");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%c", 'x'), "x");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%+.0d", 0), "+");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%0d", 42), "42");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%0ld", 42L), "42");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%0d", -42), "-42");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%05d", 42), "00042");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%05d", -42), "-0042");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%015d", 42), "000000000000042");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%015d", -42), "-00000000000042");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%015.2f", 42.1234), "000000000042.12");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%015.3f", 42.9876), "00000000042.988");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%015.5f", -42.9876), "-00000042.98760");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%-d", 42), "42");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%-d", -42), "-42");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%-5d", 42), "42   ");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%-5d", -42), "-42  ");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%-15d", 42), "42             ");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%-15d", -42), "-42            ");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%-d", 42), "42");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%-d", -42), "-42");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%-5d", 42), "42   ");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%-5d", -42), "-42  ");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%-15d", 42), "42             ");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%-15d", -42), "-42            ");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%-d", 42), "42");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%-d", -42), "-42");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%-5d", 42), "42   ");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%-5d", -42), "-42  ");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%-15d", 42), "42             ");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%-15d", -42), "-42            ");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%020d", 1024), "00000000000000001024");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%020d", -1024), "-0000000000000001024");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%020i", 1024), "00000000000000001024");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%020i", -1024), "-0000000000000001024");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%020u", 1024), "00000000000000001024");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%020u", 4294966272U), "00000000004294966272");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%#020o", 511), "00000000000000000777");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%#020o", 4294966785U), "00000000037777777001");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%#020x", 305441741), "0x00000000001234abcd");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%#020x", 3989525555U), "0x0000000000edcb5433");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%#020X", 305441741), "0X00000000001234ABCD");
	test_specific_printf("MTY_SprintfD", MTY_SprintfD("%#020X", 3989525555U), "0X0000000000EDCB5433");
	return true;
}

static bool memory_main(void)
{
	bool failed = false;

	MTY_Free(NULL);
	test_cmp("MTY_Free", !failed);

	MTY_FreeAligned(NULL);
	test_cmp("MTY_FreeAligned", !failed);

	size_t fail_value = 0;
	for (size_t x = 0; x < 128 * 1024 * 1024; x += UINT16_MAX) {
		char *buf = (char *) MTY_Alloc(1, x);

		if (buf == NULL) {
			failed = 1;
			fail_value = x;
			break;
		}
		MTY_Free(buf);
	}

	if (failed) {
		test_cmpi64("MTY_Alloc", failed, fail_value);
	} else {
		test_cmp("MTY_Alloc", !failed);
	}

	size_t opts[4] = {1, 2, 4, 8};
	for (uint8_t x = 0; x < 4; x++) {
		size_t align = opts[x] * sizeof(void *);
		char *buf = (char *) MTY_AllocAligned(0xFFFF, align);
		if (buf == NULL || (uintptr_t)buf % (uint64_t)  align) {
			failed = 1;
			fail_value = (uint64_t) align;
			break;
		}
		MTY_FreeAligned(buf);
	}

	// Limitation must be power of two
	if (failed) {
		test_cmpi64("MTY_AllocAligned", failed, fail_value);
	} else {
		test_cmp("MTY_AllocAligned", !failed);
	}

	uint8_t *test_buf = (uint8_t *) MTY_Alloc(1,128);
	for (size_t x = 0; x < 128; x++) {
		test_buf[x] = (uint8_t) x;
	}

	test_buf = (uint8_t *) MTY_Realloc(test_buf, 256, 1);
	for (size_t x = 0; x < 128; x++) {
		if (test_buf[x] != x) {
			test_cmpi64("MTY_Realloc1", (test_buf[x] != x), x);
		};
	}
	for (size_t x = 128; x < 256; x++) {
		test_buf[x] = (uint8_t) x;
	}

	test_buf = (uint8_t *) MTY_Realloc(test_buf, 128, 1);
	for (size_t x = 0; x < 128; x++) {
		if (test_buf[x] != x) {
			test_cmpi64("MTY_Realloc2", (test_buf[x] != x), x);
		};
	}
	test_cmp("MTY_Realloc", !failed);

	uint8_t *dup_buf = (uint8_t *) MTY_Dup(test_buf, 128);
	for (size_t x = 0; x < 128; x++) {
		if (dup_buf[x] != test_buf[x]) {
			test_cmpi64("MTY_Dup", failed, x);
		}
	}
	MTY_Free(dup_buf);
	test_cmp("MTY_Dup", !failed);

	for (size_t x = 0; x < 127; x++) {
		test_buf[x] = 'A';
	}
	test_buf[127] = '\0';

	char *dup_str = MTY_Strdup((char *) test_buf);
	for (size_t x = 0; x < 128; x++) {
		if (dup_str[x] != test_buf[x]) {
			test_cmpi64("MTY_Strdup", failed, x);
		}
	}
	test_cmp("MTY_Strdup", dup_str[127] == '\0');

	MTY_Strcat(dup_str, 128, (char *) test_buf);
	test_cmp("MTY_Strcat", dup_str[127] == '\0');

	MTY_Free(dup_str);

	int32_t cmp = MTY_Strcasecmp("abc#&&(!1qwerty", "ABC#&&(!1QWeRTY");
	test_cmp("MTY_Strcasecmp", cmp == 0);

	uint16_t u16_a = 0xBEEF;
	uint32_t u32_a = 0xC0CAC01A;
	uint64_t u64_a = 0xDEADBEEFFACECAFE;

	uint16_t u16_b = MTY_SwapToBE16(u16_a);
	uint32_t u32_b = MTY_SwapToBE32(u32_a);
	uint64_t u64_b = MTY_SwapToBE64(u64_a);

	test_cmp("MTY_SwapToBE16", u16_a != u16_b);
	test_cmp("MTY_SwapToBE32", u32_a != u32_b);
	test_cmp("MTY_SwapToBE64", u32_a != u32_b);

	u16_b = MTY_SwapFromBE16(u16_b);
	u32_b = MTY_SwapFromBE32(u32_b);
	u64_b = MTY_SwapFromBE64(u64_b);

	test_cmp("MTY_SwapFromBE16", u16_a == u16_b);
	test_cmp("MTY_SwapFromBE32", u32_a == u32_b);
	test_cmp("MTY_SwapFromBE64", u32_a == u32_b);

	u16_b = MTY_Swap16(u16_a);
	u32_b = MTY_Swap32(u32_a);
	u64_b = MTY_Swap64(u64_a);

	test_cmp("MTY_Swap16", u16_a != u16_b);
	test_cmp("MTY_Swap32", u32_a != u32_b);
	test_cmp("MTY_Swap64", u32_a != u32_b);


	wchar_t *wide_str = L"11月の雨が私の心に影を落としました。";

	char *utf8_str = (char *) MTY_Alloc(2048, sizeof(char));
	wchar_t *wide_str2 = (wchar_t *) MTY_Alloc(2048, sizeof(wchar_t));

	MTY_WideToMulti(wide_str, utf8_str, 2048 * sizeof(char));
	MTY_MultiToWide(utf8_str, wide_str2, 2048 * sizeof(wchar_t));
	for (size_t x = 0; x < wcslen(wide_str); x++) {
		if (wide_str[x] != wide_str2[x]) {
			test_cmpi64("MTY_MultiToWide", wide_str[x] == wide_str2[x], x);
		}
	}
	test_cmp("MTY_MultiToWide", !failed);
	MTY_Free(utf8_str);
	MTY_Free(wide_str2);

	utf8_str = MTY_WideToMultiD(wide_str);
	wide_str2 = MTY_MultiToWideD(utf8_str);
	for (size_t x = 0; x < wcslen(wide_str); x++) {
		if (wide_str[x] != wide_str2[x]) {
			test_cmpi64("MTY_MultiToWideD", wide_str[x] == wide_str2[x], x);
		}
	}
	test_cmp("MTY_MultiToWideD", !failed);

	MTY_Free(utf8_str);
	MTY_Free(wide_str2);

	MTY_Free(test_buf);

	failed = !memory_printf();

	return !failed;
}
