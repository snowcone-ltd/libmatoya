// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#define JSON_ARRAY_MAX   32
#define JSON_OBJECT_MAX  32
#define JSON_STRING_MIN  0
#define JSON_STRING_MAX  64

#define JSON_ITEM_MAX    256
#define JSON_ITER        2000

static const uint8_t JSON_UTF8[] = {
	0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x09, 0x0A, 0xD3,
	0x81, 0xD3, 0x82, 0xD3, 0x83, 0xD3, 0x84, 0xD3, 0x85, 0xD3, 0x86, 0xD3, 0x87,
	0xD3, 0x88, 0xD3, 0x89, 0xD3, 0x8A, 0xD3, 0x8B, 0xD3, 0x8C, 0xD3, 0x8D, 0xD3,
	0x8E, 0x09, 0x0A, 0xE0, 0xA2, 0xA0, 0xE0, 0xA2, 0xA1, 0xE0, 0xA2, 0xA2, 0xE0,
	0xA2, 0xA3, 0xE0, 0xA2, 0xA4, 0xE0, 0xA2, 0xA5, 0xE0, 0xA2, 0xA6, 0xE0, 0xA2,
	0xA7, 0xE0, 0xA2, 0xA8, 0x09, 0x0A, 0xF0, 0x9F, 0x86, 0x80, 0xF0, 0x9F, 0x86,
	0x81, 0xF0, 0x9F, 0x86, 0x82, 0xF0, 0x9F, 0x86, 0x83, 0xF0, 0x9F, 0x86, 0x84,
	0xF0, 0x9F, 0x86, 0x85, 0xF0, 0x9F, 0x86, 0x86, 0xF0, 0x9F, 0x86, 0x87, 0xF0,
	0x9F, 0x86, 0x88, 0xF0, 0x9F, 0x86, 0x89, 0x00
};

static const char *JSON_UTF16 = "\""
	"\\u0071\\u0072\\u0073\\u0074\\u0075\\u0076\\u0077\\u0078\\u0079\\u007a"
	"\\u0009\\u000a\\u04c1\\u04c2\\u04c3\\u04c4\\u04c5\\u04c6\\u04c7\\u04c8"
	"\\u04c9\\u04ca\\u04cb\\u04cc\\u04cd\\u04ce\\u0009\\u000a\\u08a0\\u08a1"
	"\\u08a2\\u08a3\\u08a4\\u08a5\\u08a6\\u08a7\\u08a8\\u0009\\u000a\\ud83c"
	"\\udd80\\ud83c\\udd81\\ud83c\\udd82\\ud83c\\udd83\\ud83c\\udd84\\ud83c"
	"\\udd85\\ud83c\\udd86\\ud83c\\udd87\\ud83c\\udd88\\ud83c\\udd89"
"\"";

static char *json_random_string(void)
{
	uint32_t len = MTY_GetRandomUInt(JSON_STRING_MIN, JSON_STRING_MAX);
	char *str = MTY_Alloc(len + 1, 1);

	for (uint32_t x = 0; x < len; x++)
		str[x] = (char) MTY_GetRandomUInt(1, 128);

	return str;
}

static MTY_JSON *json_random(uint32_t *n)
{
	MTY_JSON *j = NULL;

	uint32_t action = MTY_GetRandomUInt(0, 6);

	switch (action) {
		case 0: {
			uint32_t size = MTY_GetRandomUInt(0, JSON_ARRAY_MAX);
			j = MTY_JSONArrayCreate(size);

			for (uint32_t x = 0; x < size && *n < JSON_ITEM_MAX; x++, (*n)++)
				MTY_JSONArraySetItem(j, x, json_random(n));
			break;
		}
		case 1: {
			uint32_t size = MTY_GetRandomUInt(0, JSON_OBJECT_MAX);
			j = MTY_JSONObjCreate();

			for (uint32_t x = 0; x < size && *n < JSON_ITEM_MAX; x++, (*n)++) {
				char *key = json_random_string();
				MTY_JSONObjSetItem(j, key, json_random(n));

				MTY_Free(key);
			}
			break;
		}
		case 2: {
			double num = 0;
			MTY_GetRandomBytes(&num, sizeof(double));

			j = MTY_JSONNumberCreate(num);
			break;
		}
		case 3:
			j = MTY_JSONBoolCreate(MTY_GetRandomUInt(0, 2) == 1);
			break;
		case 4:
			j = MTY_JSONNullCreate();
			break;
		case 5: {
			char *str = json_random_string();
			j = MTY_JSONStringCreate(str);

			MTY_Free(str);
			break;
		}
	}

	return j;
}

static void json_test_suite(void)
{
	MTY_FileList *fl = MTY_GetFileList("json", ".json");

	if (fl) {
		for (uint32_t x = 0; x < fl->len; x++) {
			MTY_FileDesc *fd = &fl->files[x];

			if (!fd->dir) {
				char type = fd->name[0];

				MTY_DisableLog(true);
				MTY_JSON *j = MTY_JSONReadFile(MTY_JoinPath("json", fd->name));
				MTY_DisableLog(false);

				if ((!j && type == 'y') || (j && type == 'n'))
					test_print_cmp_(fd->name, "", false, "", "%s");

				MTY_JSONDestroy(&j);
			}
		}

		MTY_FreeFileList(&fl);
	}
}

static bool json_torture(void)
{
	for (uint32_t x = 0; x < JSON_ITER; x++) {
		uint32_t n = 0;

		// Random JSON
		MTY_JSON *j = json_random(&n);
		char *str = MTY_JSONSerialize(j);

		// Pass 1
		MTY_JSONDestroy(&j);
		j = MTY_JSONParse(str);
		if (!j)
			test_failed("Bad parse");

		char *str2 = MTY_JSONSerialize(j);
		if (strcmp(str, str2))
			test_failed("Mismatching parse/serialize");

		// Pass 2
		MTY_JSON *j2 = MTY_JSONDuplicate(j);
		MTY_JSONDestroy(&j);

		MTY_Free(str);
		str = MTY_JSONSerialize(j2);
		if (strcmp(str, str2))
			test_failed("Mismatching parse/serialize");

		MTY_JSONDestroy(&j2);
		MTY_Free(str);
		MTY_Free(str2);
	}

	test_passed("JSON Torture Test");

	return true;
}

static bool json_duplication(void)
{
	size_t narrays = 2000;
	char *buf = MTY_Alloc(narrays * 2 + 1, 1);
	MTY_JSON *j = MTY_JSONArrayCreate(1);
	MTY_JSON *root = j;

	for (uint32_t x = 0; x < narrays - 1; x++) {
		MTY_JSON *child = MTY_JSONArrayCreate(1);
		MTY_JSONArraySetItem(j, 0, child);
		j = child;
	}

	size_t n = narrays;

	for (j = root; j;) {
		for (size_t x = 0; x < n * 2; x++)
			buf[x] = x < n ? '[' : ']';

		buf[n * 2] = '\0';
		n--;

		MTY_JSON *j2 = MTY_JSONDuplicate(j);
		char *str = MTY_JSONSerialize(j2);

		MTY_JSONDestroy(&j2);

		if (strcmp(str, buf))
			test_failed("Duplication of child failed");

		MTY_Free(str);

		j = (MTY_JSON *) MTY_JSONArrayGetItem(j, 0);
	}

	MTY_Free(buf);
	MTY_JSONDestroy(&root);

	test_passed("JSON duplication/serialization");

	return true;
}

static bool json_utf16(void)
{
	MTY_JSON *j = MTY_JSONParse(JSON_UTF16);
	if (!j)
		test_failed("Could not parse UTF-16 string");

	char utf8[sizeof(JSON_UTF8)];
	MTY_JSONString(j, utf8, sizeof(JSON_UTF8));

	if (strcmp(utf8, (const char *) JSON_UTF8))
		test_failed("Bad UTF-16 parse");

	test_passed("JSON UTF-16");

	return true;
}

static bool json_main(void)
{
	json_test_suite();

	if (!json_torture())
		return false;

	if (!json_duplication())
		return false;

	if (!json_utf16())
		return false;

	return true;
}
