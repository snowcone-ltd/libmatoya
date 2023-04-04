// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#pragma once

#include "app-os.h"
#include "jnih.h"

static bool home_get_dir(char *dir, size_t size)
{
	JNIEnv *env = MTY_GetJNIEnv();

	jobject obj = mty_app_get_obj();
	if (!obj)
		return false;

	char *internal = mty_jni_cstrdup(env, mty_jni_obj(env, obj, "getInternalFilesDir", "()Ljava/lang/String;"));

	snprintf(dir, size, "%s", internal);

	MTY_Free(internal);

	return true;
}
