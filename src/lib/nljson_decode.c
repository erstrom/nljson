/*
 * Copyright (C) 2016  Erik Stromdahl
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "nljson.h"
#include "nljson_internal.h"

#define KEY_MATCH(key, fixed_key) \
((strlen(key) == fixed_key ## _LEN) && (strncmp(key, fixed_key, fixed_key ## _LEN) == 0))

#define STR_MATCH(str, fixed_str) \
(strncmp(str, fixed_str, strlen(fixed_str)) == 0)

struct nlattr_list_item {
	struct nlattr_list_item *next;
	struct nlattr *attr;
};

enum json_type {
	JSON_TYPE_INTEGER,
	JSON_TYPE_STRING,
	JSON_TYPE_ARRAY,
	JSON_TYPE_OBJECT
};

union attr_value {
	const char *str;
	json_t *array;
	json_t *obj;
	nljson_int_t integer;
};

static int attr_type_lengths[NLA_TYPE_MAX + 1] = {
	[NLA_UNSPEC] = 0, /* Any size */
	[NLA_U8] = sizeof(uint8_t),
	[NLA_U16] = sizeof(uint16_t),
	[NLA_U32] = sizeof(uint32_t),
	[NLA_U64] = sizeof(uint64_t),
	[NLA_STRING] = 0, /* Any size */
	[NLA_FLAG] = 4, /* TODO: Verify*/
	[NLA_MSECS] = 4, /* TODO: Verify*/
	[NLA_NESTED] = 0 /* Any size */
};

static struct nlattr_list_item *create_attr_list(json_t *attrs_json,
						 size_t *tot_attr_len);

static void free_attr_list(struct nlattr_list_item *head)
{
	struct nlattr_list_item *iter;

	iter = head;

	while (iter != NULL) {
		struct nlattr_list_item *tmp;

		tmp = iter;
		iter = iter->next;
		if (tmp->attr)
			free(tmp->attr);
		free(tmp);
	}
}

static int get_data_type_from_string(const char *str)
{
	unsigned int i;

	for (i = 0; i < NLA_TYPE_MAX + 1; i++) {
		if (STR_MATCH(str, data_type_strings[i]))
			break;
	}

	if (i == (NLA_TYPE_MAX + 1))
		i = NLA_UNSPEC;

	return i;
}

/* Checks if an attribute is valid.
 * Returns true if the attribute is valid, false otherwise.
 */
static bool attr_data_is_valid(int attr_type, int data_type, int attr_len,
			       enum json_type attr_json_type)
{
	if (attr_type < 0)
		return false;

	if ((data_type > NLA_TYPE_MAX) || (data_type < 0))
		return false;

	if ((attr_type_lengths[data_type] > 0) &&
	    (attr_len != attr_type_lengths[data_type]))
		return false;

	/* Check mismatch between data_type and json_type */
	if ((data_type == NLA_UNSPEC) &&
	    (attr_json_type != JSON_TYPE_ARRAY))
		return false;
	else if ((data_type == NLA_NESTED) &&
		 (attr_json_type != JSON_TYPE_OBJECT))
		return false;
	else if ((data_type == NLA_STRING) &&
		 (attr_json_type != JSON_TYPE_STRING))
		return false;
	else if ((((data_type >= NLA_U8) && (data_type <= NLA_U64)) ||
		 (data_type == NLA_FLAG) || (data_type == NLA_MSECS)) &&
		 (attr_json_type != JSON_TYPE_INTEGER))
		return false;

	return true;
}

static void populate_nla_stream_and_free_attr_list(struct nlattr_list_item *head,
						   void *nla_stream,
						   int (*decode_cb)(const void *buf,
								    size_t size,
								    void *data),
						   void *cb_data)
{
	struct nlattr_list_item *iter;
	void *nla_stream_loc;

	nla_stream_loc = nla_stream;
	iter = head;

	while (iter != NULL) {
		size_t cur_len;
		struct nlattr_list_item *tmp;

		if (iter->attr) {
			cur_len = NLA_ALIGN(nla_len(iter->attr) + NLA_HDR_LEN);
			if (decode_cb) {
				decode_cb(iter->attr, cur_len, cb_data);
			} else {
				memcpy(nla_stream_loc, iter->attr, cur_len);
				nla_stream_loc += cur_len;
			}
		}

		tmp = iter;
		iter = iter->next;
		free(tmp->attr);
		free(tmp);
	}
}

static int populate_attr(void *attr_buf, size_t attr_len, int attr_type,
			 enum json_type attr_json_type,
			 union attr_value attr_value,
			 struct nlattr_list_item *nested_head)
{
	struct nlattr *attr;
	size_t a_index, attr_data_len;
	json_t *a_value;
	uint8_t *attr_buf_u8;

	attr = (struct nlattr *) attr_buf;
	attr_buf_u8 = attr_buf + NLA_HDR_LEN;
	attr->nla_type = (uint16_t) attr_type;
	attr_data_len = attr_len - NLA_HDR_LEN;
	attr->nla_len = (uint16_t) attr_len;

	switch (attr_json_type) {
	case JSON_TYPE_INTEGER:
		memcpy(attr_buf_u8, &attr_value.integer, attr_data_len);
		break;
	case JSON_TYPE_STRING:
		memcpy(attr_buf_u8, attr_value.str, attr_data_len);
		break;
	case JSON_TYPE_ARRAY:
		json_array_foreach(attr_value.array, a_index, a_value) {
			int a_val_int;

			if (a_index >= attr_data_len)
				return -1;

			/* The array is expected to only contain integers */
			if (!json_is_integer(a_value))
				return -1;

			a_val_int = json_integer_value(a_value);
			/* The elements in the array is expected to be in the
			 * range 0 - 255, i.e. must fit in a uint8_t */
			if ((a_val_int < 0) || (a_val_int > 255))
				return -1;
			attr_buf_u8[a_index] = (uint8_t) a_val_int;
		}
		break;
	case JSON_TYPE_OBJECT:
		if (!nested_head)
			return -1;

		populate_nla_stream_and_free_attr_list(nested_head,
						       attr_buf_u8,
						       NULL, NULL);
		break;
	}
	return 0;
}

static struct nlattr *parse_json_attr(json_t *attr_json,
				      size_t *attr_len)
{
	const char *key;
	json_t *value;
	void *attr_buf;
	int attr_type = -1, data_type = NLA_UNSPEC, attr_data_len = 0, rc;
	union attr_value attr_value;
	enum json_type attr_json_type;
	bool length_set = false;
	struct nlattr_list_item *nested_head = NULL;

	/* Read all members of each attribute */
	json_object_foreach(attr_json, key, value) {
		if (KEY_MATCH(key, DATA_TYPE_STR)) {
			const char *tmp;

			if (!json_is_string(value))
				return NULL;
			tmp = json_string_value(value);
			data_type = get_data_type_from_string(tmp);
		} else if (KEY_MATCH(key, ATTR_TYPE_STR)) {
			if (!json_is_integer(value))
				return NULL;
			attr_type = json_integer_value(value);
		} else if (KEY_MATCH(key, LENGTH_STR)) {
			if (!json_is_integer(value))
				return NULL;
			attr_data_len = json_integer_value(value);
			length_set = true;
		} else if (KEY_MATCH(key, VALUE_STR)) {
			if (json_is_integer(value)) {
				attr_json_type = JSON_TYPE_INTEGER;
				attr_value.integer = json_integer_value(value);
			} else if (json_is_string(value)) {
				attr_json_type = JSON_TYPE_STRING;
				attr_value.str = json_string_value(value);
			} else if (json_is_array(value)) {
				attr_json_type = JSON_TYPE_ARRAY;
				attr_value.array = value;
			} else {
				attr_json_type = JSON_TYPE_OBJECT;
				attr_value.obj = value;
			}
		}
	}

	if (!length_set) {
		/* Special case for integers. If attribute length was not set we
		 * will use the length of the attribute type.
		 */
		if ((data_type >= NLA_U8) && (data_type <= NLA_U64))
			attr_data_len = attr_type_lengths[data_type];
		/* Special case for strings. If attribute length was not set we
		 * will use the length of the string.
		 */
		else if ((data_type == NLA_STRING) &&
			 (attr_json_type = JSON_TYPE_STRING))
			attr_data_len = strlen(attr_value.str);
	}

	if (!attr_data_is_valid(attr_type, data_type, attr_data_len,
	    attr_json_type))
		return NULL;

	if (data_type == NLA_NESTED) {
		size_t nested_len;

		nested_head = create_attr_list(attr_value.obj, &nested_len);
		if (!nested_head)
			return NULL;

		attr_data_len = nested_len;
	}

	*attr_len = attr_data_len + NLA_HDR_LEN;
	attr_buf = calloc(1, NLA_ALIGN(*attr_len));
	if (!attr_buf)
		return NULL;

	rc = populate_attr(attr_buf, *attr_len, attr_type, attr_json_type,
			   attr_value, nested_head);
	if (rc)
		goto err;

	return (struct nlattr *) attr_buf;
err:
	if (attr_buf)
		free(attr_buf);
	return NULL;
}

static struct nlattr_list_item *create_attr_list(json_t *attrs_json,
						 size_t *tot_attr_len)
{
	const char *key;
	json_t *value;
	struct nlattr_list_item *prev_item, head = {.next = NULL};

	*tot_attr_len = 0;
	prev_item = &head;

	json_object_foreach(attrs_json, key, value) {
		struct nlattr_list_item *cur_item;
		struct nlattr *cur_attr;
		size_t cur_attr_len;

		if (!json_is_object(value))
			goto err;

		cur_attr = parse_json_attr(value, &cur_attr_len);
		if (!cur_attr)
			goto err;

		*tot_attr_len += NLA_ALIGN(cur_attr_len);
		cur_item = calloc(sizeof(struct nlattr_list_item), 1);
		cur_item->attr = cur_attr;
		prev_item->next = cur_item;
		prev_item = cur_item;
	}

	return head.next;
err:
	free_attr_list(head.next);
	return NULL;
}

static int parse_json_attrs_cb(json_t *attrs_json,
			       int (*decode_cb)(const void *buf,
						size_t size,
						void *data),
			       void *cb_data)
{
	size_t tot_attr_len;
	struct nlattr_list_item *head;

	head = create_attr_list(attrs_json, &tot_attr_len);
	if (!head)
		return -ENOMEM;

	populate_nla_stream_and_free_attr_list(head, NULL, decode_cb, cb_data);

	return 0;
}

static int parse_json_attrs_alloc(json_t *attrs_json, void **nla_stream,
				  size_t *nla_stream_len)
{
	size_t tot_attr_len;
	struct nlattr_list_item *head;

	head = create_attr_list(attrs_json, &tot_attr_len);
	if (!head)
		return -ENOMEM;

	*nla_stream = calloc(1, tot_attr_len);
	if (!*nla_stream)
		return -ENOMEM;

	*nla_stream_len = tot_attr_len;

	populate_nla_stream_and_free_attr_list(head, *nla_stream, NULL, NULL);

	return 0;
}

static int parse_json_attrs(json_t *attrs_json, void *nla_stream,
			    size_t nla_stream_buf_len, size_t *nla_stream_len)
{
	size_t tot_attr_len;
	struct nlattr_list_item *head;

	head = create_attr_list(attrs_json, &tot_attr_len);
	if (!head)
		return -ENOMEM;

	if (tot_attr_len > nla_stream_buf_len)
		goto err;

	populate_nla_stream_and_free_attr_list(head, nla_stream, NULL, NULL);
	*nla_stream_len = tot_attr_len;

	return 0;
err:
	free_attr_list(head);
	return -ENOMEM;
}

int nljson_decode_nla(const char *input,
		      void *nla_stream,
		      size_t nla_stream_buf_len,
		      size_t *bytes_consumed,
		      size_t *bytes_produced,
		      uint32_t json_decode_flags)
{
	int rc;
	json_t *obj = NULL;
	json_error_t error;

	/*We must have JSON_DISABLE_EOF_CHECK set in order to handle
	 *the case where not all bytes in input are consumed
	 */
	json_decode_flags |= JSON_DISABLE_EOF_CHECK;
	obj = json_loads(input, json_decode_flags, &error);
	if (!obj)
		goto err;

	rc = parse_json_attrs(obj, nla_stream, nla_stream_buf_len,
			      bytes_produced);
	if (rc)
		goto err;

	json_decref(obj);
	*bytes_consumed = error.position;
	return rc;
err:
	*bytes_consumed = 0;
	*bytes_produced = 0;
	if (obj)
		json_decref(obj);
	return -EINVAL;

}

void *nljson_decode_nla_alloc(const char *input,
			      size_t *bytes_consumed,
			      size_t *bytes_produced,
			      uint32_t json_decode_flags)
{
	int rc;
	json_t *obj = NULL;
	json_error_t error;
	void *nla_stream;

	/*We must have JSON_DISABLE_EOF_CHECK set in order to handle
	 *the case where not all bytes in input are consumed
	 */
	json_decode_flags |= JSON_DISABLE_EOF_CHECK;
	obj = json_loads(input, json_decode_flags, &error);
	if (!obj)
		goto err;

	rc = parse_json_attrs_alloc(obj, &nla_stream, bytes_produced);
	if (rc)
		goto err;

	json_decref(obj);
	*bytes_consumed = error.position;
	return nla_stream;
err:
	*bytes_consumed = 0;
	*bytes_produced = 0;
	if (obj)
		json_decref(obj);
	return NULL;
}


int nljson_decode_nla_cb(const char *input,
			 size_t *bytes_consumed,
			 int (*decode_cb)(const void *buf,
					  size_t size,
					  void *data),
			 void *cb_data,
			 uint32_t json_decode_flags)
{
	int rc;
	json_t *obj = NULL;
	json_error_t error;

	/*We must have JSON_DISABLE_EOF_CHECK set in order to handle
	 *the case where not all bytes in input are consumed
	 */
	json_decode_flags |= JSON_DISABLE_EOF_CHECK;
	obj = json_loads(input, json_decode_flags, &error);
	if (!obj)
		goto err;

	rc = parse_json_attrs_cb(obj, decode_cb, cb_data);

	json_decref(obj);
	*bytes_consumed = error.position;
	return rc;
err:
	*bytes_consumed = 0;
	if (obj)
		json_decref(obj);
	return -EINVAL;
}

