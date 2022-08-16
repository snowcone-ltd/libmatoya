// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <float.h>
#include <string.h>
#include <limits.h>
#include <math.h>

#include "cJSON.h"

MTY_JSON *MTY_JSONParse(const char *input)
{
	return (MTY_JSON *) cJSON_Parse(input);
}

MTY_JSON *MTY_JSONReadFile(const char *path)
{
	MTY_JSON *j = NULL;
	void *jstr = MTY_ReadFile(path, NULL);

	if (jstr)
		j = MTY_JSONParse(jstr);

	MTY_Free(jstr);

	return j;
}

MTY_JSON *MTY_JSONDuplicate(const MTY_JSON *json)
{
	return (MTY_JSON *) (!json ? cJSON_CreateNull() : cJSON_Duplicate((cJSON *) json, true));
}

void MTY_JSONDestroy(MTY_JSON **json)
{
	if (!json || !*json)
		return;

	cJSON_Delete((cJSON *) *json);

	*json = NULL;
}

char *MTY_JSONSerialize(const MTY_JSON *json)
{
	cJSON *cj = (cJSON *) json;

	if (!cj)
		return MTY_Strdup("null");

	return cJSON_PrintUnformatted(cj);
}

bool MTY_JSONWriteFile(const char *path, const MTY_JSON *json)
{
	char *jstr = (char *) print((const cJSON *) json, true);

	bool r = MTY_WriteFile(path, jstr, strlen(jstr));
	MTY_Free(jstr);

	return r;
}

uint32_t MTY_JSONArrayGetLength(const MTY_JSON *json)
{
	cJSON *cj = (cJSON *) json;

	if (!cj || !cJSON_IsArray(cj))
		return 0;

	return cJSON_GetArraySize(cj);
}

MTY_JSON *MTY_JSONObjCreate(void)
{
	return (MTY_JSON *) cJSON_CreateObject();
}

MTY_JSON *MTY_JSONArrayCreate(uint32_t len)
{
	return (MTY_JSON *) cJSON_CreateArray();
}

bool MTY_JSONObjKeyExists(const MTY_JSON *json, const char *key)
{
	if (!json)
		return false;

	return cJSON_GetObjectItemCaseSensitive((cJSON *) json, key) != NULL;
}

bool MTY_JSONObjGetNextKey(const MTY_JSON *json, uint64_t *iter, const char **key)
{
	cJSON *cj = (cJSON *) json;

	if (!cj || !cJSON_IsObject(cj))
		return false;

	cJSON *item = cJSON_GetArrayItem(cj, (int) *iter);
	if (!item)
		return false;

	(*iter)++;
	*key = item->string;

	return true;
}

void MTY_JSONObjDeleteItem(MTY_JSON *json, const char *key)
{
	if (!json)
		return;

	cJSON_DeleteItemFromObjectCaseSensitive((cJSON *) json, key);
}

const MTY_JSON *MTY_JSONObjGetItem(const MTY_JSON *json, const char *key)
{
	const cJSON *cj = (const cJSON *) json;

	if (!cj || !cJSON_IsObject(cj))
		return NULL;

	return (const MTY_JSON *) cJSON_GetObjectItemCaseSensitive(cj, key);
}

void MTY_JSONObjSetItem(MTY_JSON *json, const char *key, MTY_JSON *value)
{
	cJSON *cj = (cJSON *) json;

	if (!cj || !cJSON_IsObject(cj) || !value)
		return;

	if (cJSON_GetObjectItemCaseSensitive(cj, key)) {
		cJSON_ReplaceItemInObjectCaseSensitive(cj, key, (cJSON *) value);

	} else {
		cJSON_AddItemToObject(cj, key, (cJSON *) value);
	}
}

const MTY_JSON *MTY_JSONArrayGetItem(const MTY_JSON *json, uint32_t index)
{
	const cJSON *cj = (const cJSON *) json;

	// cJSON allows you to iterate through and index objects
	if (!cj || (!cJSON_IsArray(cj) && !cJSON_IsObject(cj)))
		return NULL;

	return (const MTY_JSON *) cJSON_GetArrayItem(cj, index);
}

void MTY_JSONArraySetItem(MTY_JSON *json, uint32_t index, MTY_JSON *value)
{
	cJSON *cj = (cJSON *) json;

	if (!cj || !cJSON_IsArray(cj) || !value)
		return;

	cJSON_InsertItemInArray(cj, index, (cJSON *) value);
}


// Typed getters and setters

static const char *json_to_fullstring(const MTY_JSON *json)
{
	cJSON *cj = (cJSON *) json;

	if (!cj || !cJSON_IsString(cj))
		return NULL;

	return cj->valuestring;
}

static bool json_to_string(const MTY_JSON *json, char *value, size_t size)
{
	cJSON *cj = (cJSON *) json;

	if (!cj || !cJSON_IsString(cj))
		return false;

	snprintf(value, size, "%s", cj->valuestring);

	return true;
}

static bool json_to_int(const MTY_JSON *json, int32_t *value)
{
	cJSON *cj = (cJSON *) json;

	if (!cj || !cJSON_IsNumber(cj))
		return false;

	*value = cj->valueint;

	return true;
}

static bool json_to_int16(const MTY_JSON *json, int16_t *value)
{
	int32_t v = 0;
	bool r = json_to_int(json, &v);

	*value = (int16_t) v;

	return r;
}

static bool json_to_int8(const MTY_JSON *json, int8_t *value)
{
	int32_t v = 0;
	bool r = json_to_int(json, &v);

	*value = (int8_t) v;

	return r;
}

static bool json_to_float(const MTY_JSON *json, float *value)
{
	cJSON *cj = (cJSON *) json;

	if (!cj || !cJSON_IsNumber(cj))
		return false;

	*value = (float) cj->valuedouble;

	return true;
}

static bool json_to_bool(const MTY_JSON *json, bool *value)
{
	cJSON *cj = (cJSON *) json;

	if (!cj || !cJSON_IsBool(cj))
		return false;

	// cJSON stores bool information as part of the type
	// XXX DO NOT be tempted to use item->valueint!
	*value = cj->type & cJSON_True;

	return true;
}

static MTY_JSONType json_type(const MTY_JSON *json)
{
	if (!json)
		return MTY_JSON_NULL;

	cJSON *cj = (cJSON *) json;

	return cJSON_IsNull(cj) ? MTY_JSON_NULL :
		cJSON_IsNumber(cj) ? MTY_JSON_NUMBER :
		cJSON_IsString(cj) ? MTY_JSON_STRING :
		cJSON_IsArray(cj) ? MTY_JSON_ARRAY :
		cJSON_IsObject(cj) ? MTY_JSON_OBJECT :
		cJSON_IsBool(cj) ? MTY_JSON_BOOL :
		MTY_JSON_NULL;
}

const char *MTY_JSONObjGetFullString(const MTY_JSON *json, const char *key)
{
	return json_to_fullstring(MTY_JSONObjGetItem(json, key));
}

bool MTY_JSONObjGetString(const MTY_JSON *json, const char *key, char *val, size_t size)
{
	return json_to_string(MTY_JSONObjGetItem(json, key), val, size);
}

bool MTY_JSONObjGetInt(const MTY_JSON *json, const char *key, int32_t *val)
{
	return json_to_int(MTY_JSONObjGetItem(json, key), val);
}

bool MTY_JSONObjGetUInt(const MTY_JSON *json, const char *key, uint32_t *val)
{
	return json_to_int(MTY_JSONObjGetItem(json, key), (int32_t *) val);
}

bool MTY_JSONObjGetInt8(const MTY_JSON *json, const char *key, int8_t *val)
{
	return json_to_int8(MTY_JSONObjGetItem(json, key), val);
}

bool MTY_JSONObjGetUInt8(const MTY_JSON *json, const char *key, uint8_t *val)
{
	return json_to_int8(MTY_JSONObjGetItem(json, key), (int8_t *) val);
}

bool MTY_JSONObjGetInt16(const MTY_JSON *json, const char *key, int16_t *val)
{
	return json_to_int16(MTY_JSONObjGetItem(json, key), val);
}

bool MTY_JSONObjGetUInt16(const MTY_JSON *json, const char *key, uint16_t *val)
{
	return json_to_int16(MTY_JSONObjGetItem(json, key), (int16_t *) val);
}

bool MTY_JSONObjGetFloat(const MTY_JSON *json, const char *key, float *val)
{
	return json_to_float(MTY_JSONObjGetItem(json, key), val);
}

bool MTY_JSONObjGetBool(const MTY_JSON *json, const char *key, bool *val)
{
	return json_to_bool(MTY_JSONObjGetItem(json, key), val);
}

MTY_JSONType MTY_JSONObjGetValType(const MTY_JSON *json, const char *key)
{
	return json_type(MTY_JSONObjGetItem(json, key));
}

void MTY_JSONObjSetString(MTY_JSON *json, const char *key, const char *val)
{
	MTY_JSONObjSetItem(json, key, (MTY_JSON *) cJSON_CreateString(val));
}

void MTY_JSONObjSetInt(MTY_JSON *json, const char *key, int32_t val)
{
	MTY_JSONObjSetItem(json, key, (MTY_JSON *) cJSON_CreateNumber(val));
}

void MTY_JSONObjSetUInt(MTY_JSON *json, const char *key, uint32_t val)
{
	MTY_JSONObjSetItem(json, key, (MTY_JSON *) cJSON_CreateNumber(val));
}

void MTY_JSONObjSetFloat(MTY_JSON *json, const char *key, float val)
{
	MTY_JSONObjSetItem(json, key, (MTY_JSON *) cJSON_CreateNumber(val));
}

void MTY_JSONObjSetBool(MTY_JSON *json, const char *key, bool val)
{
	MTY_JSONObjSetItem(json, key, (MTY_JSON *) cJSON_CreateBool(val));
}

void MTY_JSONObjSetNull(MTY_JSON *json, const char *key)
{
	MTY_JSONObjSetItem(json, key, (MTY_JSON *) cJSON_CreateNull());
}

bool MTY_JSONArrayGetString(const MTY_JSON *json, uint32_t index, char *val, size_t size)
{
	return json_to_string(MTY_JSONArrayGetItem(json, index), val, size);
}

bool MTY_JSONArrayGetInt(const MTY_JSON *json, uint32_t index, int32_t *val)
{
	return json_to_int(MTY_JSONArrayGetItem(json, index), val);
}

bool MTY_JSONArrayGetUInt(const MTY_JSON *json, uint32_t index, uint32_t *val)
{
	return json_to_int(MTY_JSONArrayGetItem(json, index), (int32_t *) val);
}

bool MTY_JSONArrayGetFloat(const MTY_JSON *json, uint32_t index, float *val)
{
	return json_to_float(MTY_JSONArrayGetItem(json, index), val);
}

bool MTY_JSONArrayGetBool(const MTY_JSON *json, uint32_t index, bool *val)
{
	return json_to_bool(MTY_JSONArrayGetItem(json, index), val);
}

MTY_JSONType MTY_JSONArrayGetValType(const MTY_JSON *json, uint32_t index)
{
	return json_type(MTY_JSONArrayGetItem(json, index));
}

void MTY_JSONArraySetString(MTY_JSON *json, uint32_t index, const char *val)
{
	MTY_JSONArraySetItem(json, index, (MTY_JSON *) cJSON_CreateString(val));
}
