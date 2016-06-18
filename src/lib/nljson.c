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

static int get_nl_data_type_from_string(const char *type_str)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(data_type_strings); i++) {
		if (data_type_strings[i] && !strcmp(data_type_strings[i], type_str))
			return i;
	}

	return NLA_UNSPEC; /*Default to NLA_UNSPEC*/
}

/* Create a struct nla_policy from the JSON object policy_json.*/
static int parse_policy_json(json_t *policy_json, struct nla_policy **policy,
			     int *max_attr_type, char ***attr_type_to_str_map)
{
	const char *key;
	json_t *value;

	*attr_type_to_str_map = NULL;
	*policy = NULL;

	/* First, find the maximum attribute type so we can allocate
	 * big enough struct nla_policy and attr_type_to_str_map arrays
	 */
	json_object_foreach(policy_json, key, value) {
		json_t *attr_type_json;
		int attr_type;

		if (!json_is_object(value))
			return -1;

		attr_type_json = json_object_get(value, "attr_type");
		if (!attr_type_json || !json_is_integer(attr_type_json))
			return -1;

		attr_type = json_integer_value(attr_type_json);
		if (attr_type > *max_attr_type)
			*max_attr_type = attr_type;
	}

	*policy = calloc(sizeof(struct nla_policy), *max_attr_type + 1);
	if (!*policy)
		return -1;

	*attr_type_to_str_map = calloc(sizeof(char *), *max_attr_type + 1);
	if (!*attr_type_to_str_map)
		return -1;

	/* Next, populate the nla_policy structure */
	json_object_foreach(policy_json, key, value) {
		json_t *data_type_json, *attr_type_json;
		int data_type, attr_type;
		const char *data_type_str;

		if (!json_is_object(value))
			return -1;

		data_type_json = json_object_get(value, "data_type");
		if (!data_type_json || !json_is_string(data_type_json))
			return -1;

		data_type_str = json_string_value(data_type_json);
		data_type = get_nl_data_type_from_string(data_type_str);

		attr_type_json = json_object_get(value, "attr_type");
		if (!attr_type_json || !json_is_integer(attr_type_json))
			return -1;

		attr_type = json_integer_value(attr_type_json);

		(*policy)[attr_type].type = data_type;
		(*attr_type_to_str_map)[attr_type] = calloc(1, strlen(key));
		if (!(*attr_type_to_str_map)[attr_type])
			return -1;
		strcpy((*attr_type_to_str_map)[attr_type], key);
	}

	return 0;
}

static void free_id_to_str_map(char **id_to_str_map, int len)
{
	int i;

	for (i = 0; i < len; i++) {
		if (id_to_str_map[i])
			free(id_to_str_map[i]);
	}
	free(*id_to_str_map);
}

static void free_handle(nljson_t **hdl)
{
	if ((*hdl)->policy)
		free((*hdl)->policy);

	if ((*hdl)->id_to_str_map)
		free_id_to_str_map((*hdl)->id_to_str_map, (*hdl)->max_attr_type + 1);

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

		rc = parse_policy_json(policy_json_obj, &(*hdl)->policy,
				       &(*hdl)->max_attr_type,
				       &(*hdl)->id_to_str_map);
	}

err:
	if (rc)
		free_handle(hdl);
	return rc;
}

int nljson_init_file(nljson_t **hdl,
		     uint32_t json_format_flags,
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

		rc = parse_policy_json(policy_json_obj, &(*hdl)->policy,
				       &(*hdl)->max_attr_type,
				       &(*hdl)->id_to_str_map);
	}

err:
	if (rc)
		free_handle(hdl);
	return rc;
}

int nljson_init_cb(nljson_t **hdl,
		   uint32_t json_format_flags,
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

		rc = parse_policy_json(policy_json_obj, &(*hdl)->policy,
				       &(*hdl)->max_attr_type,
				       &(*hdl)->id_to_str_map);
	}

err:
	if (rc)
		free_handle(hdl);
	return rc;
}

