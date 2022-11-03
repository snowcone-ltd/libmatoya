// Copyright (c) Christopher D. Dickson <cdd@matoya.group>
//
// This Source Code Form is subject to the terms of the MIT License.
// If a copy of the MIT License was not distributed with this file,
// You can obtain one at https://spdx.org/licenses/MIT.html.

#include "matoya.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

struct MTY_JSON {
	MTY_JSONType type;
	MTY_JSON *parent;
	uint8_t stage;

	union {
		bool boolean;
		struct {
			bool isint;
			double value;
		} number;
		char *string;
		struct json_array {
			MTY_JSON **values;
			uint32_t len;
			uint32_t size;
			uint32_t index;
		} array;
		struct json_object {
			MTY_Hash *hash;
			uint64_t iter;
		} object;
	};
};


// Parse

#define JSON_ARRAY_PAD  64
#define JSON_STRING_PAD 64

#define JSON_NONE   0
#define JSON_OPEN   1
#define JSON_KEY    2
#define JSON_COLON  3
#define JSON_CLOSED 4

static const char JSON_UNESCAPE[UINT8_MAX] = {
	['"']  = '"',
	['/']  = '/',
	['\\'] = '\\',
	['b']  = '\b',
	['f']  = '\f',
	['n']  = '\n',
	['r']  = '\r',
	['t']  = '\t',
};

static const uint8_t JSON_CHARS[UINT8_MAX] = {
	['{'] = 1, ['['] = 1, // Opening object/array
	['}'] = 2, [']'] = 2, // Closing object array
	['t'] = 3, ['f'] = 3, // Boolean
	[':'] = 4,            // Object key/value separator
	[','] = 5,            // Object/array element delimiter
	['"'] = 6,            // String
	['n'] = 7,            // Null

	// Number
	['-'] = 8, ['0'] = 8, ['1'] = 8, ['2'] = 8,
	['3'] = 8, ['4'] = 8, ['5'] = 8, ['6'] = 8,
	['7'] = 8, ['8'] = 8, ['9'] = 8, ['.'] = 9,
	['+'] = 9, ['e'] = 9, ['E'] = 9,

	// White space
	['\t'] = 10, ['\r'] = 10, ['\n'] = 10, [' '] = 10,
	['\0'] = 10,
};

static MTY_JSON *json_parse_null(const char *input, size_t len, uint32_t *p)
{
	if (len - *p >= 4 && !memcmp(input + *p, "null", 4)) {
		*p += 3;
		return MTY_JSONNullCreate();
	}

	return NULL;
}

static MTY_JSON *json_parse_bool(const char *input, size_t len, uint32_t *p)
{
	if (len - *p >= 4 && !memcmp(input + *p, "true", 4)) {
		*p += 3;
		return MTY_JSONBoolCreate(true);
	}

	if (len - *p >= 5 && !memcmp(input + *p, "false", 5)) {
		*p += 4;
		return MTY_JSONBoolCreate(false);
	}

	return NULL;
}

static bool json_validate_number(const char *number, uint32_t len)
{
	if (len >= 1 && number[len - 1] == '.')
		return false;

	if (len >= 2 && number[0] == '0' && isdigit(number[1]))
		return false;

	if (len >= 2 && number[0] == '-' && number[1] == '.')
		return false;

	if (len >= 3 && number[0] == '-' && number[1] == '0' && isdigit(number[2]))
		return false;

	if (len >= 3 && isdigit(number[0]) && number[1] == '.' && (number[2] == 'e' || number[2] == 'E'))
		return false;

	return true;
}

static MTY_JSON *json_parse_number(const char *input, size_t len, uint32_t *p)
{
	char number[96];

	// We allow this to scan up to len + 1 for the '\0' character
	for (uint32_t x = 0; *p < len + 1 && x < 96; (*p)++, x++) {
		char c = number[x] = input[*p];

		switch (JSON_CHARS[(uint8_t) c]) {
			case 2:
			case 5:
			case 10:
				(*p)--;
				number[x] = '\0';

				if (!json_validate_number(number, x))
					return NULL;

				char *end = NULL;
				int64_t ival = strtoll(number, &end, 10);

				if (!*end && ival >= INT32_MIN && ival <= INT32_MAX)
					return MTY_JSONIntCreate((int32_t) ival);

				double val = strtod(number, &end);

				return *end ? NULL : MTY_JSONNumberCreate(val);
			case 8:
			case 9:
				break;
			default:
				return NULL;
		}
	}

	return NULL;
}

static size_t json_utf16_to_utf8(uint32_t code, char *str)
{
	size_t x = 0;

	if (code <= 0x7F) {
		str[x++] = code & 0x7F;

	} else {
		if (code > 0xFFFF) {
			str[x++] = (code >> 18 & 0x07) | 0xF0;
			str[x++] = (code >> 12 & 0x3F) | 0x80;

		} else if (code > 0x07FF) {
			str[x++] = (code >> 12 & 0x0F) | 0xE0;
		}

		if (code > 0x07FF) {
			str[x++] = (code >> 6 & 0x3F) | 0x80;

		} else {
			str[x++] = (code >> 6 & 0x1F) | 0xC0;
		}

		str[x++] = (code >> 0 & 0x3F) | 0x80;
	}

	return x;
}

static uint32_t json_parse_hex(const char *input)
{
	char hex[5] = {0};
	memcpy(hex, input, 4);

	char *end = NULL;
	uint32_t code = strtoul(hex, &end, 16);

	return *end ? 0x10000 : code;
}

static bool json_utf16(const char *input, size_t len, uint32_t *p, char *str, size_t *out)
{
	if (*p + 5 >= len)
		return false;

	uint32_t code = json_parse_hex(input + *p + 1);
	if (code > 0xFFFF || (code >= 0xDC00 && code <= 0xDFFF))
		return false;

	*p += 4;

	// Surrogate pair, we expect another code to follow
	if (code >= 0xD800 && code <= 0xDBFF) {
		if (*p + 7 >= len || input[*p + 1] != '\\' || input[*p + 2] != 'u')
			return false;

		uint32_t code2 = json_parse_hex(input + *p + 3);
		if (code2 < 0xDC00 || code2 > 0xDFFF)
			return false;

		*p += 6;

		code = 0x10000 | (code & 0x3FF) << 10 | (code2 & 0x3FF);
	}

	*out += json_utf16_to_utf8(code, str + *out);

	return true;
}

static char *json_parse_string(const char *input, size_t len, uint32_t *p)
{
	size_t out = 0;
	size_t slen = 0;
	char *str = NULL;

	for ((*p)++; *p < len; (*p)++) {
		char c = input[*p];

		if (c > 0 && c < 0x20)
			break;

		if (out + 4 >= slen) {
			slen += JSON_STRING_PAD;
			str = MTY_Realloc(str, slen, 1);
		}

		if (c == '"') {
			str[out] = '\0';
			return str;

		} else if (c == '\\') {
			if (++(*p) >= len)
				break;

			c = input[*p];

			if (c == 'u') {
				if (!json_utf16(input, len, p, str, &out))
					break;

				continue;

			} else {
				c = JSON_UNESCAPE[(uint8_t) c];
				if (c == 0)
					break;
			}
		}

		str[out++] = c;
	}

	MTY_Free(str);

	return NULL;
}

static bool json_attach_to_array(MTY_JSON *parent, MTY_JSON *j)
{
	struct json_array *a = &parent->array;

	if (parent->stage > JSON_OPEN)
		return false;

	if (a->len == a->size) {
		a->size += JSON_ARRAY_PAD;
		a->values = MTY_Realloc(a->values, a->size, sizeof(MTY_JSON *));
	}

	j->parent = parent;
	a->values[a->len++] = j;

	return true;
}

static bool json_attach_to_object(MTY_JSON *parent, char **key, MTY_JSON *j)
{
	if (!*key || parent->stage != JSON_COLON)
		return false;

	MTY_JSONObjSetItem(parent, *key, j);

	MTY_Free(*key);
	*key = NULL;

	return true;
}

static bool json_attach_item(MTY_JSON **root, MTY_JSON *parent, char **key, MTY_JSON *j)
{
	if (!j)
		return false;

	if (!parent) {
		if (*root) {
			MTY_JSONDestroy(&j);
			return false;
		}

		*root = j;
		return true;
	}

	bool r = parent->type == MTY_JSON_ARRAY ?
		json_attach_to_array(parent, j) :
		json_attach_to_object(parent, key, j);

	parent->stage = JSON_CLOSED;

	if (!r)
		MTY_JSONDestroy(&j);

	return r;
}

MTY_JSON *MTY_JSONParse(const char *input)
{
	size_t len = strlen(input);

	MTY_JSON *root = NULL;
	MTY_JSON *parent = NULL;
	int32_t nest = 0;
	char *key = NULL;

	uint32_t p = 0;

	for (; p < len; p++) {
		char c = input[p];

		switch (JSON_CHARS[(uint8_t) c]) {
			case 1: {
				MTY_JSON *j = c == '{' ? MTY_JSONObjCreate() : MTY_JSONArrayCreate(0);

				if (!json_attach_item(&root, parent, &key, j))
					goto except;

				parent = j;
				nest++;
				break;
			}
			case 2: {
				MTY_JSONType type = c == '}' ? MTY_JSON_OBJECT : MTY_JSON_ARRAY;
				nest--;

				if (!parent || nest < 0 || parent->type != type)
					goto except;

				if (parent->stage != JSON_NONE && parent->stage != JSON_CLOSED)
					goto except;

				parent = parent->parent;
				break;
			}
			case 4:
				if (!parent || parent->stage != JSON_KEY)
					goto except;

				parent->stage = JSON_COLON;
				break;
			case 5:
				if (!parent || parent->stage != JSON_CLOSED)
					goto except;

				parent->stage = JSON_OPEN;
				break;
			case 6: {
				char *str = json_parse_string(input, len, &p);
				if (!str)
					goto except;

				if (parent && parent->type == MTY_JSON_OBJECT && parent->stage <= JSON_OPEN) {
					key = str;
					parent->stage = JSON_KEY;

				} else {
					MTY_JSON *j = MTY_Alloc(1, sizeof(MTY_JSON));
					j->type = MTY_JSON_STRING;
					j->string = str;

					if (!json_attach_item(&root, parent, &key, j))
						goto except;
				}
				break;
			}
			case 3:
				if (!json_attach_item(&root, parent, &key, json_parse_bool(input, len, &p)))
					goto except;
				break;
			case 7:
				if (!json_attach_item(&root, parent, &key, json_parse_null(input, len, &p)))
					goto except;
				break;
			case 8:
				if (!json_attach_item(&root, parent, &key, json_parse_number(input, len, &p)))
					goto except;
				break;
			case 10:
				break;
			default:
				goto except;
		}
	}

	except:

	if (key || nest != 0 || p != len) {
		MTY_Log("Parse error at position %u", p);
		MTY_JSONDestroy(&root);
	}

	MTY_Free(key);

	return root;
}

MTY_JSON *MTY_JSONReadFile(const char *path)
{
	MTY_JSON *j = NULL;
	char *jstr = MTY_ReadFile(path, NULL);

	if (jstr)
		j = MTY_JSONParse(jstr);

	MTY_Free(jstr);

	return j;
}

MTY_JSON *MTY_JSONDuplicate(const MTY_JSON *json)
{
	if (!json)
		return MTY_JSONNullCreate();

	char *tmp = MTY_JSONSerialize(json);
	MTY_JSON *dup = MTY_JSONParse(tmp);

	MTY_Free(tmp);

	return dup;
}

MTY_JSONType MTY_JSONGetType(const MTY_JSON *json)
{
	if (!json)
		return MTY_JSON_NULL;

	return json->type;
}


// Destroy

static void json_delete_item(MTY_JSON *j)
{
	for (MTY_JSON *root = j; j;) {
		MTY_JSON *parent = j != root ? j->parent : NULL;

		switch (j->type) {
			case MTY_JSON_NULL:
			case MTY_JSON_BOOL:
			case MTY_JSON_NUMBER:
				break;
			case MTY_JSON_STRING:
				MTY_Free(j->string);
				break;
			case MTY_JSON_ARRAY: {
				struct json_array *a = &j->array;

				for (j = NULL; !j && a->index < a->len; a->index++)
					j = a->values[a->index];

				if (j)
					continue;

				MTY_Free(a->values);
				break;
			}
			case MTY_JSON_OBJECT: {
				struct json_object *o = &j->object;

				const char *key = NULL;

				if (MTY_HashGetNextKey(o->hash, &o->iter, &key)) {
					j = MTY_HashGet(o->hash, key);
					continue;
				}

				MTY_HashDestroy(&o->hash, NULL);
				break;
			}
		}

		MTY_Free(j);
		j = parent;
	}
}

void MTY_JSONDestroy(MTY_JSON **json)
{
	if (!json || !*json)
		return;

	if ((*json)->parent) {
		MTY_Log("Attempted to destroy child item");
		return;
	}

	json_delete_item(*json);
	*json = NULL;
}


// Serialize

#define JSON_SERIAL_PAD 512

struct json_serial {
	char *str;
	size_t size;
	size_t cur;
	bool pretty;
	uint32_t indent;
};

static const char JSON_ESCAPE[UINT8_MAX] = {
	['"']  = '"',
	['\\'] = '\\',
	['\b'] = 'b',
	['\f'] = 'f',
	['\n'] = 'n',
	['\r'] = 'r',
	['\t'] = 't',
};

static void json_append_char(struct json_serial *s, char c)
{
	if (s->cur == s->size) {
		s->size += JSON_SERIAL_PAD;
		s->str = MTY_Realloc(s->str, s->size + 1, 1);
	}

	s->str[s->cur++] = c;
}

static void json_append_string(struct json_serial *s, const char *add)
{
	size_t len = strlen(add);

	for (size_t x = 0; x < len; x++) {
		char c = add[x];
		char ec = JSON_ESCAPE[(uint8_t) c];

		if (ec == 0 && c > 0 && c < 0x20) {
			char hex[3] = {0};
			snprintf(hex, 3, "%02x", c);

			json_append_char(s, '\\');
			json_append_char(s, 'u');
			json_append_char(s, '0');
			json_append_char(s, '0');
			json_append_char(s, hex[0]);
			json_append_char(s, hex[1]);

		} else {
			if (ec != 0) {
				json_append_char(s, '\\');
				c = ec;
			}

			json_append_char(s, c);
		}
	}
}

static void json_append_pretty(struct json_serial *s)
{
	if (s->pretty) {
		json_append_char(s, '\n');

		for (uint32_t x = 0; x < s->indent; x++)
			json_append_char(s, '\t');
	}
}

static char *json_serialize(MTY_JSON *j, bool pretty)
{
	struct json_serial s = {
		.str = MTY_Alloc(JSON_SERIAL_PAD + 1, 1),
		.size = JSON_SERIAL_PAD,
		.pretty = pretty,
	};

	if (!j)
		json_append_string(&s, "null");

	for (MTY_JSON *root = j; j;) {
		MTY_JSON *parent = j != root ? j->parent : NULL;

		switch (j->type) {
			case MTY_JSON_NULL:
				json_append_string(&s, "null");
				break;
			case MTY_JSON_BOOL:
				json_append_string(&s, j->boolean ? "true" : "false");
				break;
			case MTY_JSON_NUMBER: {
				char number[32] = {0};

				if (j->number.isint) {
					snprintf(number, 32, "%ld", lrint(j->number.value));

				} else {
					snprintf(number, 32, "%.15g", j->number.value);
				}

				json_append_string(&s, number);
				break;
			}
			case MTY_JSON_STRING:
				json_append_char(&s, '"');
				json_append_string(&s, j->string);
				json_append_char(&s, '"');
				break;
			case MTY_JSON_ARRAY: {
				struct json_array *a = &j->array;
				uint32_t index = a->index;

				if (index == 0) {
					json_append_char(&s, '[');
					s.indent++;
				}

				for (j = NULL; !j && a->index < a->len; a->index++)
					j = a->values[a->index];

				if (j) {
					if (index > 0)
						json_append_char(&s, ',');

					json_append_pretty(&s);
					continue;
				}

				s.indent--;
				json_append_pretty(&s);
				json_append_char(&s, ']');
				a->index = 0;
				break;
			}
			case MTY_JSON_OBJECT: {
				struct json_object *o = &j->object;
				uint64_t iter = o->iter;

				if (iter == 0) {
					json_append_char(&s, '{');
					s.indent++;
				}

				const char *key = NULL;

				if (MTY_HashGetNextKey(o->hash, &o->iter, &key)) {
					if (iter > 0)
						json_append_char(&s, ',');

					json_append_pretty(&s);
					json_append_char(&s, '"');
					json_append_string(&s, key);
					json_append_char(&s, '"');
					json_append_char(&s, ':');
					if (s.pretty)
						json_append_char(&s, ' ');

					j = MTY_HashGet(o->hash, key);
					continue;
				}

				s.indent--;
				json_append_pretty(&s);
				json_append_char(&s, '}');
				o->iter = 0;
				break;
			}
		}

		j = parent;
	}

	s.str[s.cur] = '\0';

	return s.str;
}

char *MTY_JSONSerialize(const MTY_JSON *json)
{
	return json_serialize((MTY_JSON *) json, false);
}

bool MTY_JSONWriteFile(const char *path, const MTY_JSON *json)
{
	char *jstr = json_serialize((MTY_JSON *) json, true);

	bool r = MTY_WriteFile(path, jstr, strlen(jstr));
	MTY_Free(jstr);

	return r;
}


// Null

MTY_JSON *MTY_JSONNullCreate(void)
{
	MTY_JSON *j = MTY_Alloc(1, sizeof(MTY_JSON));
	j->type = MTY_JSON_NULL;

	return j;
}


// Bool

MTY_JSON *MTY_JSONBoolCreate(bool value)
{
	MTY_JSON *j = MTY_Alloc(1, sizeof(MTY_JSON));
	j->type = MTY_JSON_BOOL;
	j->boolean = value;

	return j;
}

bool MTY_JSONBool(const MTY_JSON *json, bool *value)
{
	if (!json || json->type != MTY_JSON_BOOL)
		return false;

	*value = json->boolean;

	return true;
}


// Number

MTY_JSON *MTY_JSONNumberCreate(double value)
{
	MTY_JSON *j = MTY_Alloc(1, sizeof(MTY_JSON));
	j->type = MTY_JSON_NUMBER;

	if (!isnan(value) && !isinf(value))
		j->number.value = value;

	return j;
}

MTY_JSON *MTY_JSONIntCreate(int32_t value)
{
	MTY_JSON *j = MTY_JSONNumberCreate(value);
	j->number.isint = true;

	return j;
}

bool MTY_JSONNumber(const MTY_JSON *json, double *value)
{
	if (!json || json->type != MTY_JSON_NUMBER)
		return false;

	*value = json->number.value;

	return true;
}

bool MTY_JSONInt8(const MTY_JSON *json, int8_t *value)
{
	double v = 0;
	bool r = MTY_JSONNumber(json, &v);

	*value = (int8_t) lrint(v);

	return r;
}

bool MTY_JSONInt16(const MTY_JSON *json, int16_t *value)
{
	double v = 0;
	bool r = MTY_JSONNumber(json, &v);

	*value = (int16_t) lrint(v);

	return r;
}

bool MTY_JSONInt32(const MTY_JSON *json, int32_t *value)
{
	double v = 0;
	bool r = MTY_JSONNumber(json, &v);

	*value = lrint(v);

	return r;
}

bool MTY_JSONFloat(const MTY_JSON *json, float *value)
{
	double v = 0;
	bool r = MTY_JSONNumber(json, &v);

	*value = (float) v;

	return r;
}


// String

MTY_JSON *MTY_JSONStringCreate(const char *value)
{
	MTY_JSON *j = MTY_Alloc(1, sizeof(MTY_JSON));
	j->type = MTY_JSON_STRING;
	j->string = MTY_Strdup(value);

	return j;
}

bool MTY_JSONString(const MTY_JSON *json, char *value, size_t size)
{
	if (!json || json->type != MTY_JSON_STRING)
		return false;

	int32_t n = snprintf(value, size, "%s", json->string);

	return n >= 0 && (uint32_t) n < size;
}

const char *MTY_JSONStringPtr(const MTY_JSON *json)
{
	if (!json || json->type != MTY_JSON_STRING)
		return NULL;

	return json->string;
}


// Array

MTY_JSON *MTY_JSONArrayCreate(uint32_t len)
{
	MTY_JSON *j = MTY_Alloc(1, sizeof(MTY_JSON));
	j->type = MTY_JSON_ARRAY;
	j->array.values = MTY_Alloc(len, sizeof(MTY_JSON *));
	j->array.len = j->array.size = len;

	return j;
}

uint32_t MTY_JSONArrayGetLength(const MTY_JSON *json)
{
	if (!json || json->type != MTY_JSON_ARRAY)
		return 0;

	return json->array.len;
}

const MTY_JSON *MTY_JSONArrayGetItem(const MTY_JSON *json, uint32_t index)
{
	if (!json || json->type != MTY_JSON_ARRAY)
		return NULL;

	return index < json->array.len ? json->array.values[index] : NULL;
}

bool MTY_JSONArraySetItem(MTY_JSON *json, uint32_t index, MTY_JSON *value)
{
	if (!json || json->type != MTY_JSON_ARRAY)
		return false;

	struct json_array *a = &json->array;

	if (index >= a->len)
		return false;

	if (value) {
		if (value->parent)
			return false;

		value->parent = json;
	}

	json_delete_item(a->values[index]);
	a->values[index] = value;

	return true;
}


// Object

MTY_JSON *MTY_JSONObjCreate(void)
{
	MTY_JSON *j = MTY_Alloc(1, sizeof(MTY_JSON));
	j->type = MTY_JSON_OBJECT;
	j->object.hash = MTY_HashCreate(64);

	return j;
}

bool MTY_JSONObjGetNextKey(const MTY_JSON *json, uint64_t *iter, const char **key)
{
	if (!json || json->type != MTY_JSON_OBJECT)
		return false;

	return MTY_HashGetNextKey(json->object.hash, iter, key);
}

const MTY_JSON *MTY_JSONObjGetItem(const MTY_JSON *json, const char *key)
{
	if (!json || json->type != MTY_JSON_OBJECT)
		return NULL;

	return MTY_HashGet(json->object.hash, key);
}

bool MTY_JSONObjSetItem(MTY_JSON *json, const char *key, MTY_JSON *value)
{
	if (!json || json->type != MTY_JSON_OBJECT)
		return false;

	if (value) {
		if (value->parent)
			return false;

		value->parent = json;
		json_delete_item(MTY_HashSet(json->object.hash, key, value));

	} else {
		json_delete_item(MTY_HashPop(json->object.hash, key));
	}

	return true;
}
