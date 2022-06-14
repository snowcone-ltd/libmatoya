// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#define TEST_JSON_NUM 512

static MTY_List *json_rands(void)
{
	// Create random keys
	MTY_List *l = MTY_ListCreate();

	for (uint32_t x = 0; x < TEST_JSON_NUM; x++) {
		size_t s = x + 1;
		void *b = MTY_Alloc(s, 1);
		char *hex = MTY_Alloc(s * 2 + 1, 1);

		MTY_GetRandomBytes(b, s);
		MTY_BytesToHex(b, s, hex, s * 2 + 1);
		MTY_Free(b);

		MTY_ListAppend(l, hex);
	}

	return l;
}

static bool json_torture(void)
{
	MTY_List *l = json_rands();

	// Create a JSON object with strange values
	MTY_JSON *root = MTY_JSONObjCreate();

	for (MTY_ListNode *n = MTY_ListGetFirst(l); n; n = n->next) {
		int32_t pn = (intptr_t) n & 0xFFFFFF;

		if (pn % 4 == 0) {
			MTY_JSONObjSetNull(root, n->value);

		} else if (pn % 4 == 1) {
			char *val = MTY_SprintfD("%u", MTY_DJB2(n->value));
			MTY_JSONObjSetString(root, n->value, val);
			MTY_Free(val);

		} else if (pn % 4 == 2) {
			MTY_JSONObjSetFloat(root, n->value, (float) pn + (float) M_PI);

		} else {
			MTY_JSONObjSetInt(root, n->value, pn);
		}
	}

	// Serialize the JSON object, then parse it back
	char *str = MTY_JSONSerialize(root);
	MTY_JSONDestroy(&root);
	root = MTY_JSONParse(str);
	MTY_Free(str);

	// Check to make sure it's the same as it was when it was created
	for (MTY_ListNode *n = MTY_ListGetFirst(l); n; n = n->next) {
		int32_t pn = (intptr_t) n & 0xFFFFFF;

		if (pn % 4 == 0) {
			if (MTY_JSONObjGetValType(root, n->value) != MTY_JSON_NULL)
				test_failed("JSON Value: null");

		} else if (pn % 4 == 1) {
			char gstr[64] = {0};
			if (!MTY_JSONObjGetString(root, n->value, gstr, 64))
				test_failed("JSON Key: string");

			char *val = MTY_SprintfD("%u", MTY_DJB2(n->value));
			if (strcmp(gstr, val))
				test_failed("JSON Value: string");

			MTY_Free(val);

		} else if (pn % 4 == 2) {
			float f = 0.0f;
			if (!MTY_JSONObjGetFloat(root, n->value, &f))
				test_failed("JSON Key: float")

			if (fabs(f - ((float) pn + M_PI)) > 0.1)
				test_failed("JSON Value: float");

		} else {
			int32_t val = 0;
			if (!MTY_JSONObjGetInt(root, n->value, &val))
				test_failed("JSON Key: int");

			if (val != pn)
				test_failed("JSON Value: int");
		}
	}

	// Cleanup
	MTY_JSONDestroy(&root);
	MTY_ListDestroy(&l, MTY_Free);

	return true;
}

static bool json_main(void)
{
	for (uint8_t x = 0; x < 5; x++)
		if (!json_torture())
			return false;

	test_passed("JSON");

	return true;
}
