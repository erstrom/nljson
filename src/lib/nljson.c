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

#define NL_ID_TO_STR_ELEMENT(id) \
[id] = #id

const char *data_type_strings[NLA_TYPE_MAX + 1] = {
	NL_ID_TO_STR_ELEMENT(NLA_UNSPEC),
	NL_ID_TO_STR_ELEMENT(NLA_U8),
	NL_ID_TO_STR_ELEMENT(NLA_U16),
	NL_ID_TO_STR_ELEMENT(NLA_U32),
	NL_ID_TO_STR_ELEMENT(NLA_U64),
	NL_ID_TO_STR_ELEMENT(NLA_STRING),
	NL_ID_TO_STR_ELEMENT(NLA_FLAG),
	NL_ID_TO_STR_ELEMENT(NLA_MSECS),
	NL_ID_TO_STR_ELEMENT(NLA_NESTED)
};

struct policy_list_item {
	struct policy_list_item *next;
	nljson_int_t data_type;
	nljson_int_t attr_type;
	nljson_int_t maxlen;
	nljson_int_t minlen;
	char *key;
	json_t *nested_policy;
};

static int parse_policy_json(json_t *policy_json, struct nljson_nla_policy **policy);
static void free_policy(struct nljson_nla_policy *policy);

static int get_nl_data_type_from_string(const char *type_str)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(data_type_strings); i++) {
		if (data_type_strings[i] && !strcmp(data_type_strings[i], type_str))
			return i;
	}

	return NLA_UNSPEC; /*Default to NLA_UNSPEC*/
}

static void free_attr_list(struct policy_list_item *head)
{
	struct policy_list_item *iter;

	iter = head;

	while (iter != NULL) {
		struct policy_list_item *tmp;

		tmp = iter;
		iter = iter->next;
		if (tmp->key)
			free(tmp->key);
		free(tmp);
	}
}

/* Creates a linked list of policy attributes from a JSON object. */
static struct policy_list_item *create_attr_list(json_t *policy_json,
						 nljson_int_t *max_attr_type,
						 nljson_int_t *max_nested_attr_type)
{
	const char *key;
	json_t *value;
	struct policy_list_item *prev_item, head = {.next = NULL};

	*max_attr_type = 0;
	*max_nested_attr_type = 0;
	prev_item = &head;

	json_object_foreach(policy_json, key, value) {
		json_t *data_type_json, *attr_type_json, *maxlen_json,
		       *minlen_json, *nested_policy_json;
		nljson_int_t data_type, attr_type, maxlen, minlen;
		const char *data_type_str;

		struct policy_list_item *cur_item;

		if (!json_is_object(value))
			goto err;

		attr_type_json = json_object_get(value, POLICY_ATTR_TYPE_STR);
		if (!attr_type_json || !json_is_integer(attr_type_json))
			goto err;
		attr_type = json_integer_value(attr_type_json);

		data_type_json = json_object_get(value, DATA_TYPE_STR);
		if (!data_type_json || !json_is_string(data_type_json))
			goto err;
		data_type_str = json_string_value(data_type_json);
		data_type = get_nl_data_type_from_string(data_type_str);

		maxlen_json = json_object_get(value, POLICY_MAX_LENGTH_STR);
		if (maxlen_json) {
			/* maxlen is not mandatory */
			if (!json_is_integer(maxlen_json))
				goto err;

			maxlen = json_integer_value(maxlen_json);
		} else {
			maxlen = 0;
		}

		minlen_json = json_object_get(value, POLICY_MIN_LENGTH_STR);
		if (minlen_json) {
			/* minlen is not mandatory */
			if (!json_is_integer(minlen_json))
				goto err;

			minlen = json_integer_value(minlen_json);
		} else {
			minlen = 0;
		}

		if (attr_type > *max_attr_type)
			*max_attr_type = attr_type;

		if (data_type == NLA_NESTED) {
			if (attr_type > *max_nested_attr_type)
				*max_nested_attr_type = attr_type;

			/* In case of a nested attribute,
			 * there must be a "nested" key
			 */
			nested_policy_json = json_object_get(value, POLICY_STR);
			if (!nested_policy_json)
				goto err;
		} else {
			nested_policy_json = NULL;
		}

		cur_item = calloc(sizeof(struct policy_list_item), 1);
		if (!cur_item)
			goto err;
		cur_item->attr_type = attr_type;
		cur_item->data_type = data_type;
		cur_item->maxlen = maxlen;
		cur_item->minlen = minlen;
		cur_item->nested_policy = nested_policy_json;
		cur_item->key = calloc(1, strlen(key));
		if (!cur_item->key)
			goto err;
		strcpy(cur_item->key, key);
		prev_item->next = cur_item;
		prev_item = cur_item;
	}

	return head.next;
err:
	free_attr_list(head.next);
	return NULL;
}

/* Populates a struct nljson_nla_policy (must be allocated before calling
 * this function) with the values in a policy_list.
 * The list is freed continuously during iteration.
 */
static int populate_policy_and_free_list(struct policy_list_item *head,
					 struct nljson_nla_policy *policy)
{
	struct policy_list_item *iter;

	iter = head;

	while (iter != NULL) {
		struct policy_list_item *tmp;

		policy->policy[iter->attr_type].type = iter->data_type;
		policy->policy[iter->attr_type].maxlen = iter->maxlen;
		policy->policy[iter->attr_type].minlen = iter->minlen;
		policy->id_to_str_map[iter->attr_type] = iter->key;

		if (iter->nested_policy) {
			int rc = 0;

			rc = parse_policy_json(iter->nested_policy,
					       &policy->nested[iter->attr_type]);
			json_decref(iter->nested_policy);
			if (rc)
				goto err;
		}

		tmp = iter;
		iter = iter->next;
		free(tmp);
	}

	return 0;
err:
	/* Free remaining list in case of error */
	free_attr_list(iter);
	return -1;
}

/* Allocates a struct nljson_nla_policy */
static struct nljson_nla_policy *alloc_nljson_nla_policy(nljson_int_t max_attr_type,
							 nljson_int_t max_nested_attr_type)
{
	struct nljson_nla_policy *policy;

	policy = calloc(sizeof(*policy), 1);
	if (!policy)
		goto err;
	policy->policy = calloc(sizeof(struct nla_policy), max_attr_type + 1);
	if (!policy->policy)
		goto err;

	policy->id_to_str_map = calloc(sizeof(char *), max_attr_type + 1);
	if (!policy->id_to_str_map)
		goto err;

	policy->max_attr_type = max_attr_type;

	if (max_nested_attr_type > 0) {
		policy->nested = calloc(sizeof(struct nljson_nla_policy *),
					max_nested_attr_type + 1);
		if (!policy->nested)
			goto err;

		policy->max_nested_attr_type = max_nested_attr_type;
	}

	return policy;
err:
	return NULL;
}

/* Create a struct nljson_nla_policy from the JSON object policy_json.
 * The created policy might contain nested policies, so this function might
 * be called recursively.
 * In case any memory allocation goes wrong, no cleanup of memory will be
 * attempted. This must be done by the caller.
 */
static int parse_policy_json(json_t *policy_json, struct nljson_nla_policy **policy)
{
	struct policy_list_item *head;
	nljson_int_t max_attr_type, max_nested_attr_type;

	/* First, create a temporary linked list of policy attributes  */
	head = create_attr_list(policy_json, &max_attr_type,
				&max_nested_attr_type);
	if (!head)
		goto err;

	/* Next, allocate an nljson nla policy structure. */
	*policy = alloc_nljson_nla_policy(max_attr_type, max_nested_attr_type);
	if (!*policy)
		goto err;

	/* Last, populate the structure. Note that this step might result in
	 * a recursive call back to this function (if there are any nested
	 * policý definitions).
	 */
	if (populate_policy_and_free_list(head, *policy))
		goto err;

	return 0;
err:
	return -1;
}

static void free_id_to_str_map(char **id_to_str_map, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++) {
		if (id_to_str_map[i])
			free(id_to_str_map[i]);
	}
	free(*id_to_str_map);
}

static void free_nested_policy(struct nljson_nla_policy **policy, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++) {
		if (policy[i])
			free_policy(policy[i]);
	}
	free(policy);
}

static void free_policy(struct nljson_nla_policy *policy)
{
	if (policy->policy)
		free(policy->policy);

	if (policy->id_to_str_map)
		free_id_to_str_map(policy->id_to_str_map,
				   policy->max_attr_type + 1);

	if (policy->nested)
		free_nested_policy(policy->nested,
				   policy->max_nested_attr_type + 1);

	free(policy);
}

static void free_handle(nljson_t **hdl)
{
	if ((*hdl)->policy)
		free_policy((*hdl)->policy);

	free(*hdl);
	*hdl = NULL;
}

void nljson_deinit(nljson_t **hdl)
{
	if (hdl && *hdl)
		free_handle(hdl);
}

int nljson_init(nljson_t **hdl,
		uint32_t json_format_flags,
		uint32_t nljson_flags,
		const char *policy_json)
{
	int rc;
	json_t *policy_json_obj;
	json_error_t error;

	*hdl = calloc(sizeof(struct _nljson), 1);
	if (!*hdl)
		return -ENOMEM;

	if (policy_json) {
		policy_json_obj = json_loads(policy_json, json_format_flags,
					     &error);
		if (!policy_json_obj) {
			rc = -EINVAL;
			goto err;
		}

		rc = parse_policy_json(policy_json_obj, &(*hdl)->policy);
	}

	if (nljson_flags & NLJSON_FLAG_SKIP_UNKNOWN_ATTRS)
		(*hdl)->skip_unknown_attrs = true;

err:
	if (rc)
		free_handle(hdl);
	return rc;
}

int nljson_init_file(nljson_t **hdl,
		     uint32_t json_format_flags,
		     uint32_t nljson_flags,
		     const char *policy_file)
{
	int rc;
	json_t *policy_json_obj;
	json_error_t error;

	*hdl = calloc(sizeof(struct _nljson), 1);
	if (!*hdl)
		return -ENOMEM;

	if (policy_file) {
		policy_json_obj = json_load_file(policy_file,
						 json_format_flags,
						 &error);
		if (!policy_json_obj) {
			rc = -EINVAL;
			goto err;
		}

		rc = parse_policy_json(policy_json_obj, &(*hdl)->policy);
	}

	if (nljson_flags & NLJSON_FLAG_SKIP_UNKNOWN_ATTRS)
		(*hdl)->skip_unknown_attrs = true;

err:
	if (rc)
		free_handle(hdl);
	return rc;
}

int nljson_init_cb(nljson_t **hdl,
		   uint32_t json_format_flags,
		   uint32_t nljson_flags,
		   size_t (*read_policy_cb)(void *, size_t, void *),
		   void *cb_data)
{
	int rc;
	json_t *policy_json_obj;
	json_error_t error;

	*hdl = calloc(sizeof(struct _nljson), 1);
	if (!*hdl)
		return -ENOMEM;

	if (read_policy_cb) {
		policy_json_obj = json_load_callback(read_policy_cb,
						     cb_data,
						     json_format_flags,
						     &error);
		if (!policy_json_obj) {
			rc = -EINVAL;
			goto err;
		}

		rc = parse_policy_json(policy_json_obj, &(*hdl)->policy);
	}

	if (nljson_flags & NLJSON_FLAG_SKIP_UNKNOWN_ATTRS)
		(*hdl)->skip_unknown_attrs = true;

err:
	if (rc)
		free_handle(hdl);
	return rc;
}

