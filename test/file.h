// File Test
// Copyright 2021, Jamie Blanks
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

static char *file_g_address = "\
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
for the people, shall not perish from the earth.";

static bool file_main (void)
{
	const char *origin_file = "test_file.txt";
	const char *origin_file2 = "other_file.txt";

	const char *get_dir = MTY_GetDir(MTY_DIR_HOME);
	test_cmp("MTY_GetDir1", strlen(get_dir));

	get_dir = MTY_GetDir(MTY_DIR_PROGRAMS);
	test_cmp("MTY_GetDir2", strlen(get_dir));

	get_dir = MTY_GetDir(MTY_DIR_GLOBAL_HOME);
	test_cmp("MTY_GetDir3", strlen(get_dir));

	const char *cwd = MTY_GetDir(MTY_DIR_CWD);
	test_cmp("MTY_GetDir4", strlen(cwd));

	const char *full_path = MTY_JoinPath(cwd, origin_file);
	test_cmp("MTY_JoinPath", strlen(full_path) > strlen(origin_file) + strlen(cwd));

	MTY_WriteFile(full_path, file_g_address, strlen(file_g_address));
	test_cmp("MTY_WriteFile", MTY_FileExists(full_path));

	size_t read_bytes;
	char *g_address_2 = (char *) MTY_ReadFile(full_path, &read_bytes);
	test_cmp("MTY_ReadFile", strlen(g_address_2) == strlen(file_g_address));
	MTY_Free(g_address_2);

	MTY_WriteTextFile(full_path, "%s", "a");
	MTY_AppendTextToFile(full_path, "%s", file_g_address);
	g_address_2 = (char *) MTY_ReadFile(full_path, &read_bytes);
	test_cmp("MTY_AppendTextToFile", g_address_2[0] == 'a' && g_address_2[1] == 'F');
	MTY_Free(g_address_2);

	const char *prefix = MTY_GetPathPrefix(full_path);
	test_cmp("MTY_GetPathPrefix", !strcmp(cwd, prefix));

	const char *filename = MTY_GetFileName(full_path, true);
	test_cmp("MTY_GetFileName", !strcmp(origin_file, filename));

	const char *full_path_2 = MTY_JoinPath(prefix, origin_file2);
	MTY_CopyFile(full_path, full_path_2);
	test_cmp("MTY_CopyFile", MTY_FileExists(full_path_2));


	MTY_DeleteFile(full_path);
	test_cmp("MTY_DeleteFile1", !MTY_FileExists(full_path));
	test_cmp("MTY_DeleteFile2", !MTY_DeleteFile(full_path)); // Make sure it handles missing file

	test_cmp("MTY_MoveFile1", MTY_MoveFile(full_path_2, full_path));
	test_cmp("MTY_MoveFile2", MTY_FileExists(full_path));
	test_cmp("MTY_MoveFile3", !MTY_FileExists(full_path_2));

	test_cmp("MTY_MoveFile4", !MTY_MoveFile(full_path_2, full_path));
	test_cmp("MTY_MoveFile5", MTY_FileExists(full_path));
	test_cmp("MTY_MoveFile6", !MTY_FileExists(full_path_2));

	MTY_FileList *list = MTY_GetFileList(prefix, ".txt");
	bool failed = true;
	if (list) {
		for (uint32_t x = 0; x < list->len; x++) {
			// if (strcmp(".txt", list->files[x].name) < 0) // Will show dirs, working as intended?
			// 	break;
			if (!strcmp(origin_file, list->files[x].name)) {
				failed = false;
				break;
			}
		}
	}
	test_cmp("MTY_FileList", !failed);
	MTY_FreeFileList(&list);
	test_cmp("MTY_FreeFileList", !list);

	MTY_DeleteFile(full_path);

	full_path = MTY_JoinPath(cwd, "test_dir");
	MTY_Mkdir(full_path);
	test_cmp("MTY_Mkdir", MTY_FileExists(full_path));

	// FIXME: This will leave an orphaned dir.

	return true;
}
