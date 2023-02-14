// Log Test
// Copyright 2021, Jamie Blanks
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

static char *log_g_address = " \
Four score and seven years ago our fathers brought forth on this continent, a new nation, conceived \
in Liberty, and dedicated to the proposition that all men are created equal. \
Now we are engaged in a great civil war, testing whether that nation, or any nation so conceived \
and so dedicated, can long endure. We are met on a great battle-field of that war. We have come to \
dedicate a portion of that field, as a final resting place for those who here gave their lives that \
that nation might live. It is altogether fitting and proper that we should do this. \
But, in a larger sense, we can not dedicate -- we can not consecrate -- we can not hallow -- \
this ground. The brave men, living and dead, who struggled here, have consecrated it, far above \
our poor power to add or detract. The world will little note, nor long remember what we say here, \
but it can never forget what they did here. It is for us the living, rather, to be dedicated here \
to the unfinished work which they who fought here have thus far so nobly advanced. It is rather for \
us to be here dedicated to the great task remaining before us -- that from these honored dead we \
take increased devotion to that cause for which they gave the last full measure of devotion -- \
that we here highly resolve that these dead shall not have died in vain -- that this nation, \
under God, shall have a new birth of freedom -- and that government of the people, by the people, \
for the people, shall not perish from the earth. ";

static void log_main_logfunc(const char *msg, void *opaque)
{
	uint32_t *test_num_p = (uint32_t *) opaque;

	char *test_name = "";

	switch (*test_num_p) {
		case 0: test_name = "MTY_LogFunc(Short)"; break;
		case 1: test_name = "MTY_LogFunc(Long)"; break;
		case 2: test_print_cmp("MTY_DisableLog(On)", 0); break;
		case 3: test_name = "MTY_DisableLog(off)"; break;
		default: test_name = "Unknown"; break;
	}

	test_print_cmp(test_name, msg != NULL && strlen(msg));
}

static bool log_main(void)
{
	uint32_t test_num = 0;

	MTY_SetLogFunc(log_main_logfunc, &test_num);

	MTY_LogParams("FunkyFunc", "Funky func getting %s", "Funky.");

	test_num = 1;
	MTY_LogParams("FunkyFunc", "Lincoln Say %s", log_g_address);

	const char *last_log = (char *) MTY_GetLog();
	test_cmp("MTY_GetLog", last_log != NULL && strlen(last_log));

	// LogFatalParams is untestable.
	test_num = 2;
	MTY_DisableLog(true);
	MTY_LogParams("FunkyFuncOff", "Funky func getting %s", "Funky.");

	test_num = 3;
	MTY_DisableLog(false);
	MTY_LogParams("FunkyFunc", "Funky func getting %s", "Funky.");

	return true;
}

