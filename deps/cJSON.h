/*
  Copyright (c) 2009-2017 Dave Gamble and cJSON contributors

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

#pragma once

#define CJSON_PUBLIC(type) static type

/* cJSON Types: */
#define cJSON_Invalid (0)
#define cJSON_False   (1 << 0)
#define cJSON_True    (1 << 1)
#define cJSON_NULL    (1 << 2)
#define cJSON_Number  (1 << 3)
#define cJSON_String  (1 << 4)
#define cJSON_Array   (1 << 5)
#define cJSON_Object  (1 << 6)
#define cJSON_Raw     (1 << 7) /* raw json */

#define cJSON_IsReference   256
#define cJSON_StringIsConst 512

/* The cJSON structure: */
typedef struct cJSON {
	/* next/prev allow you to walk array/object chains. Alternatively, use GetArraySize/GetArrayItem/GetObjectItem */
	struct cJSON *next;
	struct cJSON *prev;
	/* An array or object item will have a child pointer pointing to a chain of the items in the array/object. */
	struct cJSON *child;

	/* The type of the item, as above. */
	int type;

	/* The item's string, if type==cJSON_String  and type == cJSON_Raw */
	char *valuestring;
	/* writing to valueint is DEPRECATED, use cJSON_SetNumberValue instead */
	int valueint;
	/* The item's number, if type==cJSON_Number */
	double valuedouble;

	/* The item's name string, if this item is the child of, or is in the list of subitems of an object. */
	char *string;
} cJSON;

typedef int cJSON_bool;

/* Limits how deeply nested arrays/objects can be before cJSON rejects to parse them.
 * This is to prevent stack overflows. */
#define CJSON_NESTING_LIMIT 1000

/* Memory Management: the caller is always responsible to free the results from all variants of cJSON_Parse (with cJSON_Delete) and cJSON_Print (with stdlib free, cJSON_Hooks.free_fn, or cJSON_free as appropriate). The exception is cJSON_PrintPreallocated, where the caller has full responsibility of the buffer. */
/* Supply a block of JSON, and this returns a cJSON object you can interrogate. */
CJSON_PUBLIC(cJSON *) cJSON_Parse(const char *value);
/* ParseWithOpts allows you to require (and check) that the JSON is null terminated, and to retrieve the pointer to the final byte parsed. */
/* If you supply a ptr in return_parse_end and parsing fails, then return_parse_end will contain a pointer to the error so will match cJSON_GetErrorPtr(). */
CJSON_PUBLIC(cJSON *) cJSON_ParseWithOpts(const char *value, const char **return_parse_end, cJSON_bool require_null_terminated);
CJSON_PUBLIC(cJSON *)
cJSON_ParseWithLengthOpts(const char *value, size_t buffer_length, const char **return_parse_end, cJSON_bool require_null_terminated);

/* Render a cJSON entity to text for transfer/storage without any formatting. */
CJSON_PUBLIC(char *) cJSON_PrintUnformatted(const cJSON *item);
/* Delete a cJSON entity and all subentities. */
CJSON_PUBLIC(void) cJSON_Delete(cJSON *item);

/* Returns the number of items in an array (or object). */
CJSON_PUBLIC(int) cJSON_GetArraySize(const cJSON *array);
/* Retrieve item number "index" from array "array". Returns NULL if unsuccessful. */
CJSON_PUBLIC(cJSON *) cJSON_GetArrayItem(const cJSON *array, int index);
/* Get item "string" from object. Case insensitive. */
CJSON_PUBLIC(cJSON *) cJSON_GetObjectItemCaseSensitive(const cJSON *const object, const char *const string);
/* For analysing failed parses. This returns a pointer to the parse error. You'll probably need to look a few chars back to make sense of it. Defined when cJSON_Parse() returns 0. 0 when cJSON_Parse() succeeds. */

/* These functions check the type of an item */
CJSON_PUBLIC(cJSON_bool) cJSON_IsBool(const cJSON *const item);
CJSON_PUBLIC(cJSON_bool) cJSON_IsNull(const cJSON *const item);
CJSON_PUBLIC(cJSON_bool) cJSON_IsNumber(const cJSON *const item);
CJSON_PUBLIC(cJSON_bool) cJSON_IsString(const cJSON *const item);
CJSON_PUBLIC(cJSON_bool) cJSON_IsArray(const cJSON *const item);
CJSON_PUBLIC(cJSON_bool) cJSON_IsObject(const cJSON *const item);

/* These calls create a cJSON item of the appropriate type. */
CJSON_PUBLIC(cJSON *) cJSON_CreateNull(void);
CJSON_PUBLIC(cJSON *) cJSON_CreateBool(cJSON_bool boolean);
CJSON_PUBLIC(cJSON *) cJSON_CreateNumber(double num);
CJSON_PUBLIC(cJSON *) cJSON_CreateString(const char *string);
CJSON_PUBLIC(cJSON *) cJSON_CreateArray(void);
CJSON_PUBLIC(cJSON *) cJSON_CreateObject(void);

/* Append item to the specified array/object. */
CJSON_PUBLIC(cJSON_bool) cJSON_AddItemToArray(cJSON *array, cJSON *item);
CJSON_PUBLIC(cJSON_bool) cJSON_AddItemToObject(cJSON *object, const char *string, cJSON *item);

/* Remove/Detach items from Arrays/Objects. */
CJSON_PUBLIC(cJSON *) cJSON_DetachItemViaPointer(cJSON *parent, cJSON *const item);
CJSON_PUBLIC(cJSON *) cJSON_DetachItemFromArray(cJSON *array, int which);
CJSON_PUBLIC(void) cJSON_DeleteItemFromArray(cJSON *array, int which);
CJSON_PUBLIC(cJSON *) cJSON_DetachItemFromObjectCaseSensitive(cJSON *object, const char *string);
CJSON_PUBLIC(void) cJSON_DeleteItemFromObjectCaseSensitive(cJSON *object, const char *string);

/* Update array items. */
CJSON_PUBLIC(cJSON_bool) cJSON_InsertItemInArray(cJSON *array, int which, cJSON *newitem); /* Shifts pre-existing items to the right. */
CJSON_PUBLIC(cJSON_bool) cJSON_ReplaceItemViaPointer(cJSON *const parent, cJSON *const item, cJSON *replacement);
CJSON_PUBLIC(cJSON_bool) cJSON_ReplaceItemInObjectCaseSensitive(cJSON *object, const char *string, cJSON *newitem);

/* Duplicate a cJSON item */
/* Duplicate will create a new, identical cJSON item to the one you pass, in new memory that will
 * need to be released. With recurse!=0, it will duplicate any children connected to the item.
 * The item->next and ->prev pointers are always zero on return from Duplicate. */
CJSON_PUBLIC(cJSON *) cJSON_Duplicate(const cJSON *item, cJSON_bool recurse);

#define CJSON_MALLOC(size)       MTY_Alloc(size, 1)
#define CJSON_REALLOC(ptr, size) MTY_Realloc(ptr, size, 1)
#define CJSON_FREE(ptr)          MTY_Free(ptr)

/* define our own boolean type */
#ifdef true
	#undef true
#endif
#define true ((cJSON_bool) 1)

#ifdef false
	#undef false
#endif
#define false ((cJSON_bool) 0)

typedef struct {
	const unsigned char *json;
	size_t position;
} error;

/* Internal constructor. */
static cJSON *cJSON_New_Item(void)
{
	return CJSON_MALLOC(sizeof(cJSON));
}

/* Delete a cJSON structure. */
CJSON_PUBLIC(void) cJSON_Delete(cJSON *item)
{
	cJSON *next = NULL;
	while (item != NULL) {
		next = item->next;
		if (!(item->type & cJSON_IsReference) && (item->child != NULL)) {
			cJSON_Delete(item->child);
		}
		if (!(item->type & cJSON_IsReference) && (item->valuestring != NULL)) {
			MTY_SecureFree(item->valuestring, strlen(item->valuestring));
		}
		if (!(item->type & cJSON_StringIsConst) && (item->string != NULL)) {
			MTY_SecureFree(item->string, strlen(item->string));
		}
		MTY_SecureFree(item, sizeof(cJSON));
		item = next;
	}
}

typedef struct {
	const unsigned char *content;
	size_t length;
	size_t offset;
	size_t depth; /* How deeply nested (in arrays/objects) is the input at the current offset. */
} parse_buffer;

/* check if the given size is left to read in a given parse buffer (starting with 1) */
#define can_read(buffer, size) ((buffer != NULL) && (((buffer)->offset + size) <= (buffer)->length))
/* check if the buffer can be accessed at the given index (starting with 0) */
#define can_access_at_index(buffer, index)    ((buffer != NULL) && (((buffer)->offset + index) < (buffer)->length))
#define cannot_access_at_index(buffer, index) (!can_access_at_index(buffer, index))
/* get a pointer to the buffer at the position */
#define buffer_at_offset(buffer) ((buffer)->content + (buffer)->offset)

/* Parse the input text to generate a number, and populate the result into item. */
static cJSON_bool parse_number(cJSON *const item, parse_buffer *const input_buffer)
{
	double number = 0;
	unsigned char *after_end = NULL;
	unsigned char number_c_string[64];
	unsigned char decimal_point = '.';
	size_t i = 0;

	if ((input_buffer == NULL) || (input_buffer->content == NULL)) {
		return false;
	}

	/* copy the number into a temporary buffer and replace '.' with the decimal point
     * of the current locale (for strtod)
     * This also takes care of '\0' not necessarily being available for marking the end of the input */
	for (i = 0; (i < (sizeof(number_c_string) - 1)) && can_access_at_index(input_buffer, i); i++) {
		switch (buffer_at_offset(input_buffer)[i]) {
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
			case '+':
			case '-':
			case 'e':
			case 'E':
				number_c_string[i] = buffer_at_offset(input_buffer)[i];
				break;

			case '.':
				number_c_string[i] = decimal_point;
				break;

			default:
				goto loop_end;
		}
	}
loop_end:
	number_c_string[i] = '\0';

	number = strtod((const char *) number_c_string, (char **) &after_end);
	if (number_c_string == after_end) {
		return false; /* parse_error */
	}

	item->valuedouble = number;

	/* use saturation in case of overflow */
	if (number >= INT_MAX) {
		item->valueint = INT_MAX;
	} else if (number <= (double) INT_MIN) {
		item->valueint = INT_MIN;
	} else {
		item->valueint = (int) number;
	}

	item->type = cJSON_Number;

	input_buffer->offset += (size_t)(after_end - number_c_string);
	return true;
}

typedef struct {
	unsigned char *buffer;
	size_t length;
	size_t offset;
	size_t depth; /* current nesting depth (for formatted printing) */
	cJSON_bool noalloc;
	cJSON_bool format; /* is this print a formatted print */
} printbuffer;

/* realloc printbuffer if necessary to have at least "needed" bytes more */
static unsigned char *ensure(printbuffer *const p, size_t needed)
{
	unsigned char *newbuffer = NULL;
	size_t newsize = 0;

	if ((p == NULL) || (p->buffer == NULL)) {
		return NULL;
	}

	if ((p->length > 0) && (p->offset >= p->length)) {
		/* make sure that offset is valid */
		return NULL;
	}

	if (needed > INT_MAX) {
		/* sizes bigger than INT_MAX are currently not supported */
		return NULL;
	}

	needed += p->offset + 1;
	if (needed <= p->length) {
		return p->buffer + p->offset;
	}

	if (p->noalloc) {
		return NULL;
	}

	/* calculate new buffer size */
	if (needed > (INT_MAX / 2)) {
		/* overflow of int, use INT_MAX if possible */
		if (needed <= INT_MAX) {
			newsize = INT_MAX;
		} else {
			return NULL;
		}
	} else {
		newsize = needed * 2;
	}

	/* reallocate with realloc if available */
	newbuffer = CJSON_REALLOC(p->buffer, newsize);
	if (newbuffer == NULL) {
		CJSON_FREE(p->buffer);
		p->length = 0;
		p->buffer = NULL;

		return NULL;
	}

	p->length = newsize;
	p->buffer = newbuffer;

	return newbuffer + p->offset;
}

/* calculate the new length of the string in a printbuffer and update the offset */
static void update_offset(printbuffer *const buffer)
{
	const unsigned char *buffer_pointer = NULL;
	if ((buffer == NULL) || (buffer->buffer == NULL)) {
		return;
	}
	buffer_pointer = buffer->buffer + buffer->offset;

	buffer->offset += strlen((const char *) buffer_pointer);
}

/* securely comparison of floating-point variables */
static cJSON_bool compare_double(double a, double b)
{
	double maxVal = fabs(a) > fabs(b) ? fabs(a) : fabs(b);
	return (fabs(a - b) <= maxVal * DBL_EPSILON);
}

/* Render the number nicely from the given item into a string. */
static cJSON_bool print_number(const cJSON *const item, printbuffer *const output_buffer)
{
	unsigned char *output_pointer = NULL;
	double d = item->valuedouble;
	int length = 0;
	size_t i = 0;
	unsigned char number_buffer[26] = {0}; /* temporary buffer to print the number into */
	unsigned char decimal_point = '.';
	double test = 0.0;

	if (output_buffer == NULL) {
		return false;
	}

	/* This checks for NaN and Infinity */
	if (isnan(d) || isinf(d)) {
		length = snprintf((char *) number_buffer, 26, "null");
	} else {
		/* Try 15 decimal places of precision to avoid nonsignificant nonzero digits */
		length = snprintf((char *) number_buffer, 26, "%1.15g", d);

		/* Check whether the original double can be recovered */
		char *eptr = NULL;
		test = strtod((char *) number_buffer, &eptr);

		if (eptr == (char *) number_buffer || !compare_double((double) test, d)) {
			/* If not, print with 17 decimal places of precision */
			length = snprintf((char *) number_buffer, 26, "%1.17g", d);
		}
	}

	/* sprintf failed or buffer overrun occurred */
	if ((length < 0) || (length > (int) (sizeof(number_buffer) - 1))) {
		return false;
	}

	/* reserve appropriate space in the output */
	output_pointer = ensure(output_buffer, (size_t) length + sizeof(""));
	if (output_pointer == NULL) {
		return false;
	}

	/* copy the printed number to the output and replace locale
     * dependent decimal point with '.' */
	for (i = 0; i < ((size_t) length); i++) {
		if (number_buffer[i] == decimal_point) {
			output_pointer[i] = '.';
			continue;
		}

		output_pointer[i] = number_buffer[i];
	}
	output_pointer[i] = '\0';

	output_buffer->offset += (size_t) length;

	return true;
}

/* parse 4 digit hexadecimal number */
static unsigned parse_hex4(const unsigned char *const input)
{
	unsigned int h = 0;
	size_t i = 0;

	for (i = 0; i < 4; i++) {
		/* parse digit */
		if ((input[i] >= '0') && (input[i] <= '9')) {
			h += (unsigned int) input[i] - '0';
		} else if ((input[i] >= 'A') && (input[i] <= 'F')) {
			h += (unsigned int) 10 + input[i] - 'A';
		} else if ((input[i] >= 'a') && (input[i] <= 'f')) {
			h += (unsigned int) 10 + input[i] - 'a';
		} else /* invalid */
		{
			return 0;
		}

		if (i < 3) {
			/* shift left to make place for the next nibble */
			h = h << 4;
		}
	}

	return h;
}

/* converts a UTF-16 literal to UTF-8
 * A literal can be one or two sequences of the form \uXXXX */
static unsigned char utf16_literal_to_utf8(const unsigned char *const input_pointer, const unsigned char *const input_end, unsigned char **output_pointer)
{
	long unsigned int codepoint = 0;
	unsigned int first_code = 0;
	const unsigned char *first_sequence = input_pointer;
	unsigned char utf8_length = 0;
	unsigned char utf8_position = 0;
	unsigned char sequence_length = 0;
	unsigned char first_byte_mark = 0;

	if ((input_end - first_sequence) < 6) {
		/* input ends unexpectedly */
		goto fail;
	}

	/* get the first utf16 sequence */
	first_code = parse_hex4(first_sequence + 2);

	/* check that the code is valid */
	if (((first_code >= 0xDC00) && (first_code <= 0xDFFF))) {
		goto fail;
	}

	/* UTF16 surrogate pair */
	if ((first_code >= 0xD800) && (first_code <= 0xDBFF)) {
		const unsigned char *second_sequence = first_sequence + 6;
		unsigned int second_code = 0;
		sequence_length = 12; /* \uXXXX\uXXXX */

		if ((input_end - second_sequence) < 6) {
			/* input ends unexpectedly */
			goto fail;
		}

		if ((second_sequence[0] != '\\') || (second_sequence[1] != 'u')) {
			/* missing second half of the surrogate pair */
			goto fail;
		}

		/* get the second utf16 sequence */
		second_code = parse_hex4(second_sequence + 2);
		/* check that the code is valid */
		if ((second_code < 0xDC00) || (second_code > 0xDFFF)) {
			/* invalid second half of the surrogate pair */
			goto fail;
		}


		/* calculate the unicode codepoint from the surrogate pair */
		codepoint = 0x10000 + (((first_code & 0x3FF) << 10) | (second_code & 0x3FF));
	} else {
		sequence_length = 6; /* \uXXXX */
		codepoint = first_code;
	}

	/* encode as UTF-8
     * takes at maximum 4 bytes to encode:
     * 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
	if (codepoint < 0x80) {
		/* normal ascii, encoding 0xxxxxxx */
		utf8_length = 1;
	} else if (codepoint < 0x800) {
		/* two bytes, encoding 110xxxxx 10xxxxxx */
		utf8_length = 2;
		first_byte_mark = 0xC0; /* 11000000 */
	} else if (codepoint < 0x10000) {
		/* three bytes, encoding 1110xxxx 10xxxxxx 10xxxxxx */
		utf8_length = 3;
		first_byte_mark = 0xE0; /* 11100000 */
	} else if (codepoint <= 0x10FFFF) {
		/* four bytes, encoding 1110xxxx 10xxxxxx 10xxxxxx 10xxxxxx */
		utf8_length = 4;
		first_byte_mark = 0xF0; /* 11110000 */
	} else {
		/* invalid unicode codepoint */
		goto fail;
	}

	/* encode as utf8 */
	for (utf8_position = (unsigned char) (utf8_length - 1); utf8_position > 0; utf8_position--) {
		/* 10xxxxxx */
		(*output_pointer)[utf8_position] = (unsigned char) ((codepoint | 0x80) & 0xBF);
		codepoint >>= 6;
	}
	/* encode first byte */
	if (utf8_length > 1) {
		(*output_pointer)[0] = (unsigned char) ((codepoint | first_byte_mark) & 0xFF);
	} else {
		(*output_pointer)[0] = (unsigned char) (codepoint & 0x7F);
	}

	*output_pointer += utf8_length;

	return sequence_length;

fail:
	return 0;
}

/* Parse the input text into an unescaped cinput, and populate item. */
static cJSON_bool parse_string(cJSON *const item, parse_buffer *const input_buffer)
{
	const unsigned char *input_pointer = buffer_at_offset(input_buffer) + 1;
	const unsigned char *input_end = buffer_at_offset(input_buffer) + 1;
	unsigned char *output_pointer = NULL;
	unsigned char *output = NULL;

	/* not a string */
	if (buffer_at_offset(input_buffer)[0] != '\"') {
		goto fail;
	}

	{
		/* calculate approximate size of the output (overestimate) */
		size_t allocation_length = 0;
		size_t skipped_bytes = 0;
		while (((size_t)(input_end - input_buffer->content) < input_buffer->length) && (*input_end != '\"')) {
			/* is escape sequence */
			if (input_end[0] == '\\') {
				if ((size_t)(input_end + 1 - input_buffer->content) >= input_buffer->length) {
					/* prevent buffer overflow when last input character is a backslash */
					goto fail;
				}
				skipped_bytes++;
				input_end++;
			}
			input_end++;
		}
		if (((size_t)(input_end - input_buffer->content) >= input_buffer->length) || (*input_end != '\"')) {
			goto fail; /* string ended unexpectedly */
		}

		/* This is at most how much we need for the output */
		allocation_length = (size_t)(input_end - buffer_at_offset(input_buffer)) - skipped_bytes;
		output = CJSON_MALLOC(allocation_length + sizeof(""));
		if (output == NULL) {
			goto fail; /* allocation failure */
		}
	}

	output_pointer = output;
	/* loop through the string literal */
	while (input_pointer < input_end) {
		if (*input_pointer != '\\') {
			*output_pointer++ = *input_pointer++;
		}
		/* escape sequence */
		else {
			unsigned char sequence_length = 2;
			if ((input_end - input_pointer) < 1) {
				goto fail;
			}

			switch (input_pointer[1]) {
				case 'b':
					*output_pointer++ = '\b';
					break;
				case 'f':
					*output_pointer++ = '\f';
					break;
				case 'n':
					*output_pointer++ = '\n';
					break;
				case 'r':
					*output_pointer++ = '\r';
					break;
				case 't':
					*output_pointer++ = '\t';
					break;
				case '\"':
				case '\\':
				case '/':
					*output_pointer++ = input_pointer[1];
					break;

				/* UTF-16 literal */
				case 'u':
					sequence_length = utf16_literal_to_utf8(input_pointer, input_end, &output_pointer);
					if (sequence_length == 0) {
						/* failed to convert UTF16-literal to UTF-8 */
						goto fail;
					}
					break;

				default:
					goto fail;
			}
			input_pointer += sequence_length;
		}
	}

	/* zero terminate the output */
	*output_pointer = '\0';

	item->type = cJSON_String;
	item->valuestring = (char *) output;

	input_buffer->offset = (size_t)(input_end - input_buffer->content);
	input_buffer->offset++;

	return true;

fail:
	if (output != NULL) {
		CJSON_FREE(output);
	}

	if (input_pointer != NULL) {
		input_buffer->offset = (size_t)(input_pointer - input_buffer->content);
	}

	return false;
}

/* Render the cstring provided to an escaped version that can be printed. */
static cJSON_bool print_string_ptr(const unsigned char *const input, printbuffer *const output_buffer)
{
	const unsigned char *input_pointer = NULL;
	unsigned char *output = NULL;
	unsigned char *output_pointer = NULL;
	size_t output_length = 0;
	/* numbers of additional characters needed for escaping */
	size_t escape_characters = 0;

	if (output_buffer == NULL) {
		return false;
	}

	/* empty string */
	if (input == NULL) {
		output = ensure(output_buffer, sizeof("\"\""));
		if (output == NULL) {
			return false;
		}
		snprintf((char *) output, sizeof("\"\""), "\"\"");

		return true;
	}

	/* set "flag" to 1 if something needs to be escaped */
	for (input_pointer = input; *input_pointer; input_pointer++) {
		switch (*input_pointer) {
			case '\"':
			case '\\':
			case '\b':
			case '\f':
			case '\n':
			case '\r':
			case '\t':
				/* one character escape sequence */
				escape_characters++;
				break;
			default:
				if (*input_pointer < 32) {
					/* UTF-16 escape sequence uXXXX */
					escape_characters += 5;
				}
				break;
		}
	}
	output_length = (size_t)(input_pointer - input) + escape_characters;

	output = ensure(output_buffer, output_length + sizeof("\"\""));
	if (output == NULL) {
		return false;
	}

	/* no characters have to be escaped */
	if (escape_characters == 0) {
		output[0] = '\"';
		memcpy(output + 1, input, output_length);
		output[output_length + 1] = '\"';
		output[output_length + 2] = '\0';

		return true;
	}

	output[0] = '\"';
	output_pointer = output + 1;
	/* copy the string */
	for (input_pointer = input; *input_pointer != '\0'; (void) input_pointer++, output_pointer++) {
		if ((*input_pointer > 31) && (*input_pointer != '\"') && (*input_pointer != '\\')) {
			/* normal character, copy */
			*output_pointer = *input_pointer;
		} else {
			/* character needs to be escaped */
			*output_pointer++ = '\\';
			switch (*input_pointer) {
				case '\\':
					*output_pointer = '\\';
					break;
				case '\"':
					*output_pointer = '\"';
					break;
				case '\b':
					*output_pointer = 'b';
					break;
				case '\f':
					*output_pointer = 'f';
					break;
				case '\n':
					*output_pointer = 'n';
					break;
				case '\r':
					*output_pointer = 'r';
					break;
				case '\t':
					*output_pointer = 't';
					break;
				default:
					/* escape and print as unicode codepoint */
					snprintf((char *) output_pointer, 6, "u%04x", *input_pointer);
					output_pointer += 4;
					break;
			}
		}
	}
	output[output_length + 1] = '\"';
	output[output_length + 2] = '\0';

	return true;
}

/* Invoke print_string_ptr (which is useful) on an item. */
static cJSON_bool print_string(const cJSON *const item, printbuffer *const p)
{
	return print_string_ptr((unsigned char *) item->valuestring, p);
}

/* Predeclare these prototypes. */
static cJSON_bool parse_value(cJSON *const item, parse_buffer *const input_buffer);
static cJSON_bool print_value(const cJSON *const item, printbuffer *const output_buffer);
static cJSON_bool parse_array(cJSON *const item, parse_buffer *const input_buffer);
static cJSON_bool print_array(const cJSON *const item, printbuffer *const output_buffer);
static cJSON_bool parse_object(cJSON *const item, parse_buffer *const input_buffer);
static cJSON_bool print_object(const cJSON *const item, printbuffer *const output_buffer);

/* Utility to jump whitespace and cr/lf */
static parse_buffer *buffer_skip_whitespace(parse_buffer *const buffer)
{
	if ((buffer == NULL) || (buffer->content == NULL)) {
		return NULL;
	}

	if (cannot_access_at_index(buffer, 0)) {
		return buffer;
	}

	while (can_access_at_index(buffer, 0) && (buffer_at_offset(buffer)[0] <= 32)) {
		buffer->offset++;
	}

	if (buffer->offset == buffer->length) {
		buffer->offset--;
	}

	return buffer;
}

/* skip the UTF-8 BOM (byte order mark) if it is at the beginning of a buffer */
static parse_buffer *skip_utf8_bom(parse_buffer *const buffer)
{
	if ((buffer == NULL) || (buffer->content == NULL) || (buffer->offset != 0)) {
		return NULL;
	}

	if (can_access_at_index(buffer, 4) && (strncmp((const char *) buffer_at_offset(buffer), "\xEF\xBB\xBF", 3) == 0)) {
		buffer->offset += 3;
	}

	return buffer;
}

CJSON_PUBLIC(cJSON *) cJSON_ParseWithOpts(const char *value, const char **return_parse_end, cJSON_bool require_null_terminated)
{
	size_t buffer_length;

	if (NULL == value) {
		return NULL;
	}

	/* Adding null character size due to require_null_terminated. */
	buffer_length = strlen(value) + sizeof("");

	return cJSON_ParseWithLengthOpts(value, buffer_length, return_parse_end, require_null_terminated);
}

/* Parse an object - create a new root, and populate. */
CJSON_PUBLIC(cJSON *)
cJSON_ParseWithLengthOpts(const char *value, size_t buffer_length, const char **return_parse_end, cJSON_bool require_null_terminated)
{
	parse_buffer buffer = {0};
	cJSON *item = NULL;

	if (value == NULL || 0 == buffer_length) {
		goto fail;
	}

	buffer.content = (const unsigned char *) value;
	buffer.length = buffer_length;
	buffer.offset = 0;

	item = cJSON_New_Item();
	if (item == NULL) /* memory fail */
	{
		goto fail;
	}

	if (!parse_value(item, buffer_skip_whitespace(skip_utf8_bom(&buffer)))) {
		/* parse failure. ep is set. */
		goto fail;
	}

	/* if we require null-terminated JSON without appended garbage, skip and then check for a null terminator */
	if (require_null_terminated) {
		buffer_skip_whitespace(&buffer);
		if ((buffer.offset >= buffer.length) || buffer_at_offset(&buffer)[0] != '\0') {
			goto fail;
		}
	}
	if (return_parse_end) {
		*return_parse_end = (const char *) buffer_at_offset(&buffer);
	}

	return item;

fail:
	if (item != NULL) {
		cJSON_Delete(item);
	}

	if (value != NULL) {
		error local_error;
		local_error.json = (const unsigned char *) value;
		local_error.position = 0;

		if (buffer.offset < buffer.length) {
			local_error.position = buffer.offset;
		} else if (buffer.length > 0) {
			local_error.position = buffer.length - 1;
		}

		if (return_parse_end != NULL) {
			*return_parse_end = (const char *) local_error.json + local_error.position;
		}
	}

	return NULL;
}

/* Default options for cJSON_Parse */
CJSON_PUBLIC(cJSON *) cJSON_Parse(const char *value)
{
	return cJSON_ParseWithOpts(value, 0, 0);
}

static unsigned char *print(const cJSON *const item, cJSON_bool format)
{
	static const size_t default_buffer_size = 256;
	printbuffer buffer[1];
	unsigned char *printed = NULL;

	memset(buffer, 0, sizeof(buffer));

	/* create buffer */
	buffer->buffer = CJSON_MALLOC(default_buffer_size);
	buffer->length = default_buffer_size;
	buffer->format = format;
	if (buffer->buffer == NULL) {
		goto fail;
	}

	/* print the value */
	if (!print_value(item, buffer)) {
		goto fail;
	}
	update_offset(buffer);

	/* check if reallocate is available */
	printed = CJSON_REALLOC(buffer->buffer, buffer->offset + 1);
	if (printed == NULL) {
		goto fail;
	}
	buffer->buffer = NULL;

	return printed;

fail:
	if (buffer->buffer != NULL) {
		CJSON_FREE(buffer->buffer);
	}

	if (printed != NULL) {
		CJSON_FREE(printed);
	}

	return NULL;
}

CJSON_PUBLIC(char *) cJSON_PrintUnformatted(const cJSON *item)
{
	return (char *) print(item, false);
}

/* Parser core - when encountering text, process appropriately. */
static cJSON_bool parse_value(cJSON *const item, parse_buffer *const input_buffer)
{
	if ((input_buffer == NULL) || (input_buffer->content == NULL)) {
		return false; /* no input */
	}

	/* parse the different types of values */
	/* null */
	if (can_read(input_buffer, 4) && (strncmp((const char *) buffer_at_offset(input_buffer), "null", 4) == 0)) {
		item->type = cJSON_NULL;
		input_buffer->offset += 4;
		return true;
	}
	/* false */
	if (can_read(input_buffer, 5) && (strncmp((const char *) buffer_at_offset(input_buffer), "false", 5) == 0)) {
		item->type = cJSON_False;
		input_buffer->offset += 5;
		return true;
	}
	/* true */
	if (can_read(input_buffer, 4) && (strncmp((const char *) buffer_at_offset(input_buffer), "true", 4) == 0)) {
		item->type = cJSON_True;
		item->valueint = 1;
		input_buffer->offset += 4;
		return true;
	}
	/* string */
	if (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == '\"')) {
		return parse_string(item, input_buffer);
	}
	/* number */
	if (can_access_at_index(input_buffer, 0) && ((buffer_at_offset(input_buffer)[0] == '-') || ((buffer_at_offset(input_buffer)[0] >= '0') && (buffer_at_offset(input_buffer)[0] <= '9')))) {
		return parse_number(item, input_buffer);
	}
	/* array */
	if (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == '[')) {
		return parse_array(item, input_buffer);
	}
	/* object */
	if (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == '{')) {
		return parse_object(item, input_buffer);
	}

	return false;
}

/* Render a value to text. */
static cJSON_bool print_value(const cJSON *const item, printbuffer *const output_buffer)
{
	unsigned char *output = NULL;

	if ((item == NULL) || (output_buffer == NULL)) {
		return false;
	}

	switch ((item->type) & 0xFF) {
		case cJSON_NULL:
			output = ensure(output_buffer, 5);
			if (output == NULL) {
				return false;
			}
			snprintf((char *) output, 5, "null");
			return true;

		case cJSON_False:
			output = ensure(output_buffer, 6);
			if (output == NULL) {
				return false;
			}
			snprintf((char *) output, 6, "false");
			return true;

		case cJSON_True:
			output = ensure(output_buffer, 5);
			if (output == NULL) {
				return false;
			}
			snprintf((char *) output, 5, "true");
			return true;

		case cJSON_Number:
			return print_number(item, output_buffer);

		case cJSON_Raw: {
			size_t raw_length = 0;
			if (item->valuestring == NULL) {
				return false;
			}

			raw_length = strlen(item->valuestring) + sizeof("");
			output = ensure(output_buffer, raw_length);
			if (output == NULL) {
				return false;
			}
			memcpy(output, item->valuestring, raw_length);
			return true;
		}

		case cJSON_String:
			return print_string(item, output_buffer);

		case cJSON_Array:
			return print_array(item, output_buffer);

		case cJSON_Object:
			return print_object(item, output_buffer);

		default:
			return false;
	}
}

/* Build an array from input text. */
static cJSON_bool parse_array(cJSON *const item, parse_buffer *const input_buffer)
{
	cJSON *head = NULL; /* head of the linked list */
	cJSON *current_item = NULL;

	if (input_buffer->depth >= CJSON_NESTING_LIMIT) {
		return false; /* to deeply nested */
	}
	input_buffer->depth++;

	if (buffer_at_offset(input_buffer)[0] != '[') {
		/* not an array */
		goto fail;
	}

	input_buffer->offset++;
	buffer_skip_whitespace(input_buffer);
	if (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == ']')) {
		/* empty array */
		goto success;
	}

	/* check if we skipped to the end of the buffer */
	if (cannot_access_at_index(input_buffer, 0)) {
		input_buffer->offset--;
		goto fail;
	}

	/* step back to character in front of the first element */
	input_buffer->offset--;
	/* loop through the comma separated array elements */
	do {
		/* allocate next item */
		cJSON *new_item = cJSON_New_Item();
		if (new_item == NULL) {
			goto fail; /* allocation failure */
		}

		/* attach next item to list */
		if (head == NULL) {
			/* start the linked list */
			current_item = head = new_item;
		} else {
			/* add to the end and advance */
			current_item->next = new_item;
			new_item->prev = current_item;
			current_item = new_item;
		}

		/* parse next value */
		input_buffer->offset++;
		buffer_skip_whitespace(input_buffer);
		if (!parse_value(current_item, input_buffer)) {
			goto fail; /* failed to parse value */
		}
		buffer_skip_whitespace(input_buffer);
	} while (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == ','));

	if (cannot_access_at_index(input_buffer, 0) || buffer_at_offset(input_buffer)[0] != ']') {
		goto fail; /* expected end of array */
	}

success:
	input_buffer->depth--;

	item->type = cJSON_Array;
	item->child = head;

	input_buffer->offset++;

	return true;

fail:
	if (head != NULL) {
		cJSON_Delete(head);
	}

	return false;
}

/* Render an array to text */
static cJSON_bool print_array(const cJSON *const item, printbuffer *const output_buffer)
{
	unsigned char *output_pointer = NULL;
	size_t length = 0;
	cJSON *current_element = item->child;

	if (output_buffer == NULL) {
		return false;
	}

	/* Compose the output array. */
	/* opening square bracket */
	output_pointer = ensure(output_buffer, 1);
	if (output_pointer == NULL) {
		return false;
	}

	*output_pointer = '[';
	output_buffer->offset++;
	output_buffer->depth++;

	while (current_element != NULL) {
		if (!print_value(current_element, output_buffer)) {
			return false;
		}
		update_offset(output_buffer);
		if (current_element->next) {
			length = (size_t)(output_buffer->format ? 2 : 1);
			output_pointer = ensure(output_buffer, length + 1);
			if (output_pointer == NULL) {
				return false;
			}
			*output_pointer++ = ',';
			if (output_buffer->format) {
				*output_pointer++ = ' ';
			}
			*output_pointer = '\0';
			output_buffer->offset += length;
		}
		current_element = current_element->next;
	}

	output_pointer = ensure(output_buffer, 2);
	if (output_pointer == NULL) {
		return false;
	}
	*output_pointer++ = ']';
	*output_pointer = '\0';
	output_buffer->depth--;

	return true;
}

/* Build an object from the text. */
static cJSON_bool parse_object(cJSON *const item, parse_buffer *const input_buffer)
{
	cJSON *head = NULL; /* linked list head */
	cJSON *current_item = NULL;

	if (input_buffer->depth >= CJSON_NESTING_LIMIT) {
		return false; /* to deeply nested */
	}
	input_buffer->depth++;

	if (cannot_access_at_index(input_buffer, 0) || (buffer_at_offset(input_buffer)[0] != '{')) {
		goto fail; /* not an object */
	}

	input_buffer->offset++;
	buffer_skip_whitespace(input_buffer);
	if (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == '}')) {
		goto success; /* empty object */
	}

	/* check if we skipped to the end of the buffer */
	if (cannot_access_at_index(input_buffer, 0)) {
		input_buffer->offset--;
		goto fail;
	}

	/* step back to character in front of the first element */
	input_buffer->offset--;
	/* loop through the comma separated array elements */
	do {
		/* allocate next item */
		cJSON *new_item = cJSON_New_Item();
		if (new_item == NULL) {
			goto fail; /* allocation failure */
		}

		/* attach next item to list */
		if (head == NULL) {
			/* start the linked list */
			current_item = head = new_item;
		} else {
			/* add to the end and advance */
			current_item->next = new_item;
			new_item->prev = current_item;
			current_item = new_item;
		}

		/* parse the name of the child */
		input_buffer->offset++;
		buffer_skip_whitespace(input_buffer);
		if (!parse_string(current_item, input_buffer)) {
			goto fail; /* failed to parse name */
		}
		buffer_skip_whitespace(input_buffer);

		/* swap valuestring and string, because we parsed the name */
		current_item->string = current_item->valuestring;
		current_item->valuestring = NULL;

		if (cannot_access_at_index(input_buffer, 0) || (buffer_at_offset(input_buffer)[0] != ':')) {
			goto fail; /* invalid object */
		}

		/* parse the value */
		input_buffer->offset++;
		buffer_skip_whitespace(input_buffer);
		if (!parse_value(current_item, input_buffer)) {
			goto fail; /* failed to parse value */
		}
		buffer_skip_whitespace(input_buffer);
	} while (can_access_at_index(input_buffer, 0) && (buffer_at_offset(input_buffer)[0] == ','));

	if (cannot_access_at_index(input_buffer, 0) || (buffer_at_offset(input_buffer)[0] != '}')) {
		goto fail; /* expected end of object */
	}

success:
	input_buffer->depth--;

	item->type = cJSON_Object;
	item->child = head;

	input_buffer->offset++;
	return true;

fail:
	if (head != NULL) {
		cJSON_Delete(head);
	}

	return false;
}

/* Render an object to text. */
static cJSON_bool print_object(const cJSON *const item, printbuffer *const output_buffer)
{
	unsigned char *output_pointer = NULL;
	size_t length = 0;
	cJSON *current_item = item->child;

	if (output_buffer == NULL) {
		return false;
	}

	/* Compose the output: */
	length = (size_t)(output_buffer->format ? 2 : 1); /* fmt: {\n */
	output_pointer = ensure(output_buffer, length + 1);
	if (output_pointer == NULL) {
		return false;
	}

	*output_pointer++ = '{';
	output_buffer->depth++;
	if (output_buffer->format) {
		*output_pointer++ = '\n';
	}
	output_buffer->offset += length;

	while (current_item) {
		if (output_buffer->format) {
			size_t i;
			output_pointer = ensure(output_buffer, output_buffer->depth);
			if (output_pointer == NULL) {
				return false;
			}
			for (i = 0; i < output_buffer->depth; i++) {
				*output_pointer++ = '\t';
			}
			output_buffer->offset += output_buffer->depth;
		}

		/* print key */
		if (!print_string_ptr((unsigned char *) current_item->string, output_buffer)) {
			return false;
		}
		update_offset(output_buffer);

		length = (size_t)(output_buffer->format ? 2 : 1);
		output_pointer = ensure(output_buffer, length);
		if (output_pointer == NULL) {
			return false;
		}
		*output_pointer++ = ':';
		if (output_buffer->format) {
			*output_pointer++ = ' ';
		}
		output_buffer->offset += length;

		/* print value */
		if (!print_value(current_item, output_buffer)) {
			return false;
		}
		update_offset(output_buffer);

		/* print comma if not last */
		length = ((size_t)(output_buffer->format ? 1 : 0) + (size_t)(current_item->next ? 1 : 0));
		output_pointer = ensure(output_buffer, length + 1);
		if (output_pointer == NULL) {
			return false;
		}
		if (current_item->next) {
			*output_pointer++ = ',';
		}

		if (output_buffer->format) {
			*output_pointer++ = '\n';
		}
		*output_pointer = '\0';
		output_buffer->offset += length;

		current_item = current_item->next;
	}

	output_pointer = ensure(output_buffer, output_buffer->format ? (output_buffer->depth + 1) : 2);
	if (output_pointer == NULL) {
		return false;
	}
	if (output_buffer->format) {
		size_t i;
		for (i = 0; i < (output_buffer->depth - 1); i++) {
			*output_pointer++ = '\t';
		}
	}
	*output_pointer++ = '}';
	*output_pointer = '\0';
	output_buffer->depth--;

	return true;
}

/* Get Array size/item / object item. */
CJSON_PUBLIC(int) cJSON_GetArraySize(const cJSON *array)
{
	cJSON *child = NULL;
	size_t size = 0;

	if (array == NULL) {
		return 0;
	}

	child = array->child;

	while (child != NULL) {
		size++;
		child = child->next;
	}

	/* FIXME: Can overflow here. Cannot be fixed without breaking the API */

	return (int) size;
}

static cJSON *get_array_item(const cJSON *array, size_t index)
{
	cJSON *current_child = NULL;

	if (array == NULL) {
		return NULL;
	}

	current_child = array->child;
	while ((current_child != NULL) && (index > 0)) {
		index--;
		current_child = current_child->next;
	}

	return current_child;
}

CJSON_PUBLIC(cJSON *) cJSON_GetArrayItem(const cJSON *array, int index)
{
	if (index < 0) {
		return NULL;
	}

	return get_array_item(array, (size_t) index);
}

static cJSON *get_object_item(const cJSON *const object, const char *const name)
{
	cJSON *current_element = NULL;

	if ((object == NULL) || (name == NULL)) {
		return NULL;
	}

	current_element = object->child;
	while ((current_element != NULL) && (current_element->string != NULL) && (strcmp(name, current_element->string) != 0)) {
		current_element = current_element->next;
	}

	if ((current_element == NULL) || (current_element->string == NULL)) {
		return NULL;
	}

	return current_element;
}

CJSON_PUBLIC(cJSON *) cJSON_GetObjectItemCaseSensitive(const cJSON *const object, const char *const string)
{
	return get_object_item(object, string);
}

/* Utility for array list handling. */
static void suffix_object(cJSON *prev, cJSON *item)
{
	prev->next = item;
	item->prev = prev;
}

static cJSON_bool add_item_to_array(cJSON *array, cJSON *item)
{
	cJSON *child = NULL;

	if ((item == NULL) || (array == NULL) || (array == item)) {
		return false;
	}

	child = array->child;
	/*
     * To find the last item in array quickly, we use prev in array
     */
	if (child == NULL) {
		/* list is empty, start new one */
		array->child = item;
		item->prev = item;
		item->next = NULL;
	} else {
		/* append to the end */
		if (child->prev) {
			suffix_object(child->prev, item);
			array->child->prev = item;
		} else {
			while (child->next) {
				child = child->next;
			}
			suffix_object(child, item);
			array->child->prev = item;
		}
	}

	return true;
}

/* Add item to array/object. */
CJSON_PUBLIC(cJSON_bool) cJSON_AddItemToArray(cJSON *array, cJSON *item)
{
	return add_item_to_array(array, item);
}

static cJSON_bool add_item_to_object(cJSON *const object, const char *const string, cJSON *const item, const cJSON_bool constant_key)
{
	char *new_key = NULL;
	int new_type = cJSON_Invalid;

	if ((object == NULL) || (string == NULL) || (item == NULL) || (object == item)) {
		return false;
	}

	if (constant_key) {
		new_key = (char *) string;
		new_type = item->type | cJSON_StringIsConst;
	} else {
		new_key = (char *) MTY_Strdup(string);
		if (new_key == NULL) {
			return false;
		}

		new_type = item->type & ~cJSON_StringIsConst;
	}

	if (!(item->type & cJSON_StringIsConst) && (item->string != NULL)) {
		CJSON_FREE(item->string);
	}

	item->string = new_key;
	item->type = new_type;

	return add_item_to_array(object, item);
}

CJSON_PUBLIC(cJSON_bool) cJSON_AddItemToObject(cJSON *object, const char *string, cJSON *item)
{
	return add_item_to_object(object, string, item, false);
}

CJSON_PUBLIC(cJSON *) cJSON_DetachItemViaPointer(cJSON *parent, cJSON *const item)
{
	if ((parent == NULL) || (item == NULL)) {
		return NULL;
	}

	if (item != parent->child) {
		/* not the first element */
		item->prev->next = item->next;
	}

	if (item->next != NULL) {
		/* not the last element */
		item->next->prev = item->prev;
	}

	if (item == parent->child) {
		/* first element */
		parent->child = item->next;

	} else if (item->next == NULL) {
		/* last element */
		parent->child->prev = item->prev;
	}

	/* make sure the detached item doesn't point anywhere anymore */
	item->prev = NULL;
	item->next = NULL;

	return item;
}

CJSON_PUBLIC(cJSON *) cJSON_DetachItemFromArray(cJSON *array, int which)
{
	if (which < 0) {
		return NULL;
	}

	return cJSON_DetachItemViaPointer(array, get_array_item(array, (size_t) which));
}

CJSON_PUBLIC(void) cJSON_DeleteItemFromArray(cJSON *array, int which)
{
	cJSON_Delete(cJSON_DetachItemFromArray(array, which));
}

CJSON_PUBLIC(cJSON *) cJSON_DetachItemFromObjectCaseSensitive(cJSON *object, const char *string)
{
	cJSON *to_detach = cJSON_GetObjectItemCaseSensitive(object, string);

	return cJSON_DetachItemViaPointer(object, to_detach);
}

CJSON_PUBLIC(void) cJSON_DeleteItemFromObjectCaseSensitive(cJSON *object, const char *string)
{
	cJSON_Delete(cJSON_DetachItemFromObjectCaseSensitive(object, string));
}

/* Replace array/object items with new ones. */
CJSON_PUBLIC(cJSON_bool) cJSON_InsertItemInArray(cJSON *array, int which, cJSON *newitem)
{
	cJSON *after_inserted = NULL;

	if (which < 0) {
		return false;
	}

	after_inserted = get_array_item(array, (size_t) which);
	if (after_inserted == NULL) {
		return add_item_to_array(array, newitem);
	}

	newitem->next = after_inserted;
	newitem->prev = after_inserted->prev;
	after_inserted->prev = newitem;
	if (after_inserted == array->child) {
		array->child = newitem;
	} else {
		newitem->prev->next = newitem;
	}
	return true;
}

CJSON_PUBLIC(cJSON_bool) cJSON_ReplaceItemViaPointer(cJSON *const parent, cJSON *const item, cJSON *replacement)
{
	if ((parent == NULL) || (replacement == NULL) || (item == NULL)) {
		return false;
	}

	if (replacement == item) {
		return true;
	}

	replacement->next = item->next;
	replacement->prev = item->prev;

	if (replacement->next != NULL) {
		replacement->next->prev = replacement;
	}
	if (parent->child == item) {
		parent->child = replacement;
	} else { /*
         * To find the last item in array quickly, we use prev in array.
         * We can't modify the last item's next pointer where this item was the parent's child
         */
		if (replacement->prev != NULL) {
			replacement->prev->next = replacement;
		}
	}

	item->next = NULL;
	item->prev = NULL;
	cJSON_Delete(item);

	return true;
}

static cJSON_bool replace_item_in_object(cJSON *object, const char *string, cJSON *replacement)
{
	if ((replacement == NULL) || (string == NULL)) {
		return false;
	}

	/* replace the name in the replacement */
	if (!(replacement->type & cJSON_StringIsConst) && (replacement->string != NULL)) {
		CJSON_FREE(replacement->string);
	}
	replacement->string = (char *) MTY_Strdup(string);
	replacement->type &= ~cJSON_StringIsConst;

	return cJSON_ReplaceItemViaPointer(object, get_object_item(object, string), replacement);
}

CJSON_PUBLIC(cJSON_bool) cJSON_ReplaceItemInObjectCaseSensitive(cJSON *object, const char *string, cJSON *newitem)
{
	return replace_item_in_object(object, string, newitem);
}

/* Create basic types: */
CJSON_PUBLIC(cJSON *) cJSON_CreateNull(void)
{
	cJSON *item = cJSON_New_Item();
	if (item) {
		item->type = cJSON_NULL;
	}

	return item;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateBool(cJSON_bool boolean)
{
	cJSON *item = cJSON_New_Item();
	if (item) {
		item->type = boolean ? cJSON_True : cJSON_False;
	}

	return item;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateNumber(double num)
{
	cJSON *item = cJSON_New_Item();
	if (item) {
		item->type = cJSON_Number;
		item->valuedouble = num;

		/* use saturation in case of overflow */
		if (num >= INT_MAX) {
			item->valueint = INT_MAX;
		} else if (num <= (double) INT_MIN) {
			item->valueint = INT_MIN;
		} else {
			item->valueint = (int) num;
		}
	}

	return item;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateString(const char *string)
{
	cJSON *item = cJSON_New_Item();
	if (item) {
		item->type = cJSON_String;
		item->valuestring = (char *) MTY_Strdup(string);
		if (!item->valuestring) {
			cJSON_Delete(item);
			return NULL;
		}
	}

	return item;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateArray(void)
{
	cJSON *item = cJSON_New_Item();
	if (item) {
		item->type = cJSON_Array;
	}

	return item;
}

CJSON_PUBLIC(cJSON *) cJSON_CreateObject(void)
{
	cJSON *item = cJSON_New_Item();
	if (item) {
		item->type = cJSON_Object;
	}

	return item;
}

/* Duplication */
CJSON_PUBLIC(cJSON *) cJSON_Duplicate(const cJSON *item, cJSON_bool recurse)
{
	cJSON *newitem = NULL;
	cJSON *child = NULL;
	cJSON *next = NULL;
	cJSON *newchild = NULL;

	/* Bail on bad ptr */
	if (!item) {
		goto fail;
	}
	/* Create new item */
	newitem = cJSON_New_Item();
	if (!newitem) {
		goto fail;
	}
	/* Copy over all vars */
	newitem->type = item->type & (~cJSON_IsReference);
	newitem->valueint = item->valueint;
	newitem->valuedouble = item->valuedouble;
	if (item->valuestring) {
		newitem->valuestring = (char *) MTY_Strdup(item->valuestring);
		if (!newitem->valuestring) {
			goto fail;
		}
	}
	if (item->string) {
		newitem->string = (item->type & cJSON_StringIsConst) ? item->string : (char *) MTY_Strdup(item->string);
		if (!newitem->string) {
			goto fail;
		}
	}
	/* If non-recursive, then we're done! */
	if (!recurse) {
		return newitem;
	}
	/* Walk the ->next chain for the child. */
	child = item->child;
	while (child != NULL) {
		newchild = cJSON_Duplicate(child, true); /* Duplicate (with recurse) each item in the ->next chain */
		if (!newchild) {
			goto fail;
		}
		if (next != NULL) {
			/* If newitem->child already set, then crosswire ->prev and ->next and move on */
			next->next = newchild;
			newchild->prev = next;
			next = newchild;
		} else {
			/* Set newitem->child and move to it */
			newitem->child = newchild;
			next = newchild;
		}
		child = child->next;
	}

	return newitem;

fail:
	if (newitem != NULL) {
		cJSON_Delete(newitem);
	}

	return NULL;
}

CJSON_PUBLIC(cJSON_bool) cJSON_IsBool(const cJSON *const item)
{
	if (item == NULL) {
		return false;
	}

	return (item->type & (cJSON_True | cJSON_False)) != 0;
}
CJSON_PUBLIC(cJSON_bool) cJSON_IsNull(const cJSON *const item)
{
	if (item == NULL) {
		return false;
	}

	return (item->type & 0xFF) == cJSON_NULL;
}

CJSON_PUBLIC(cJSON_bool) cJSON_IsNumber(const cJSON *const item)
{
	if (item == NULL) {
		return false;
	}

	return (item->type & 0xFF) == cJSON_Number;
}

CJSON_PUBLIC(cJSON_bool) cJSON_IsString(const cJSON *const item)
{
	if (item == NULL) {
		return false;
	}

	return (item->type & 0xFF) == cJSON_String;
}

CJSON_PUBLIC(cJSON_bool) cJSON_IsArray(const cJSON *const item)
{
	if (item == NULL) {
		return false;
	}

	return (item->type & 0xFF) == cJSON_Array;
}

CJSON_PUBLIC(cJSON_bool) cJSON_IsObject(const cJSON *const item)
{
	if (item == NULL) {
		return false;
	}

	return (item->type & 0xFF) == cJSON_Object;
}
