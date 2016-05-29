#include "nljson.h"
#include "nljson_internal.h"

struct local_encode_cb_data {
	char *output;
	size_t output_len;
	size_t bytes_consumed;
};

static json_t *create_unspec_attr_object(const uint8_t *data, size_t data_len)
{
	unsigned int i;
	json_t *obj;

	obj = json_array();
	if (!obj)
		return NULL;

	for (i = 0; i < data_len; i++)
		json_array_append_new(obj, json_integer(data[i]));

	return obj;
}

static json_t *create_attr_object(struct nlattr *attr, int data_type)
{
	json_t *obj;
	union {
		uint8_t u8;
		uint16_t u16;
		uint16_t u32;
		uint16_t u64;
		char *str;
		void *unspec;
	} data;

	obj = json_object();
	if (!obj)
		goto err;

	json_object_set_new(obj, DATA_TYPE_STR, json_string_nocheck(data_type_strings[data_type]));
	json_object_set_new(obj, ATTR_TYPE_STR, json_integer(nla_type(attr)));
	json_object_set_new(obj, LENGTH_STR, json_integer(nla_len(attr)));

	switch (data_type) {
	case NLA_U8:
		data.u8 = nla_get_u8(attr);
		json_object_set_new(obj, VALUE_STR, json_integer(data.u8));
		break;
	case NLA_U16:
		data.u16 = nla_get_u16(attr);
		json_object_set_new(obj, VALUE_STR, json_integer(data.u16));
		break;
	case NLA_U32:
		data.u32 = nla_get_u32(attr);
		json_object_set_new(obj, VALUE_STR, json_integer(data.u32));
		break;
	case NLA_U64:
		data.u64 = nla_get_u64(attr);
		json_object_set_new(obj, VALUE_STR, json_integer(data.u64));
		break;
	case NLA_STRING:
		data.str = nla_get_string(attr);
		json_object_set_new(obj, VALUE_STR, json_string_nocheck(data.str));
		break;
	case NLA_NESTED:
	case NLA_UNSPEC:
	/*Fallthrough*/
	default:
		data.unspec = nla_data(attr);
		json_object_set_new(obj, VALUE_STR, create_unspec_attr_object((uint8_t *)data.unspec, nla_len(attr)));
		break;
	}

	return obj;
err:
	if (obj != NULL)
		json_decref(obj);
	return NULL;
}

/* buf is assumed to point directly at the attribute stream */
static json_t *parse_nl_attrs(uint8_t *buf, size_t buflen, nljson_t *hdl,
			      size_t *bytes_consumed)
{
	struct nlattr *cur_attr;
	json_t *obj = NULL, *cur_attr_obj;
	int remaining, max_attr_type = 0;
	struct nla_policy *policy = NULL;
	char **attr_type_to_str_map = NULL;

	cur_attr = (struct nlattr *) buf;
	remaining = buflen;

	if (hdl) {
		max_attr_type = hdl->max_attr_type;
		policy = hdl->policy;
		attr_type_to_str_map = hdl->id_to_str_map;
	}

	*bytes_consumed = 0;
	obj = json_object();
	if (!obj)
		return NULL;

	while (nla_ok(cur_attr, remaining)) {
		int data_type = NLA_UNSPEC, type = nla_type(cur_attr);

		*bytes_consumed += (nla_len(cur_attr) + NLA_HDR_LEN);
		if (policy && (type <= max_attr_type))
			data_type = policy[type].type;
		cur_attr_obj = create_attr_object(cur_attr, data_type);
		if (cur_attr_obj) {
			if (attr_type_to_str_map && (type <= max_attr_type) &&
			    attr_type_to_str_map[type]) {
				json_object_set(obj, attr_type_to_str_map[type],
						cur_attr_obj);
			} else {
				char tmp[20];

				snprintf(tmp, sizeof(tmp), "UNKNOWN_ATTR_%d", type);
				json_object_set(obj, tmp, cur_attr_obj);
			}
			json_decref(cur_attr_obj);
		}
		cur_attr = nla_next(cur_attr, &remaining);
	}

	return obj;
}

static int local_encode_cb(const char *buf, size_t size, void *data)
{
	struct local_encode_cb_data *cb_data =
		(struct local_encode_cb_data *) data;

	if (size > cb_data->output_len - cb_data->bytes_consumed)
		return -1;

	memcpy(cb_data->output + cb_data->bytes_consumed, buf, size);
	cb_data->bytes_consumed += size;
	return 0;
}

int nljson_encode_nla(nljson_t *hdl,
		      const void *nla_stream,
		      size_t nla_stream_len,
		      char *output,
		      size_t output_len,
		      size_t *bytes_consumed,
		      size_t *bytes_produced,
		      uint32_t json_format_flags)
{
	json_t *obj;
	int rc;
	struct local_encode_cb_data cb_data = {
		.output = output,
		.output_len = output_len,
	};

	/*We add JSON_PRESERVE_ORDER in order to make sure the encoded
	 *attributes are written in the same order as in nla_stream.
	 */
	json_format_flags |= JSON_PRESERVE_ORDER;

	obj = parse_nl_attrs((uint8_t *) nla_stream, nla_stream_len, hdl,
			     bytes_consumed);
	if (!obj)
		return -EINVAL;

	rc = json_dump_callback(obj, local_encode_cb, &cb_data,
				json_format_flags);
	json_decref(obj);
	*bytes_produced = cb_data.bytes_consumed;
	return rc;
}

char *nljson_encode_nla_alloc(nljson_t *hdl,
			      const void *nla_stream,
			      size_t nla_stream_len,
			      size_t *bytes_consumed,
			      size_t *bytes_produced,
			      uint32_t json_format_flags)
{
	json_t *obj;
	char *output;

	/*We add JSON_PRESERVE_ORDER in order to make sure the encoded
	 *attributes are written in the same order as in nla_stream.
	 */
	json_format_flags |= JSON_PRESERVE_ORDER;

	obj = parse_nl_attrs((uint8_t *) nla_stream, nla_stream_len, hdl,
			     bytes_consumed);
	if (!obj)
		return NULL;

	output = json_dumps(obj, json_format_flags);
	json_decref(obj);
	*bytes_produced = strlen(output);
	return output;
}

int nljson_encode_nla_cb(nljson_t *hdl,
			 const void *nla_stream,
			 size_t nla_stream_len,
			 size_t *bytes_consumed,
			 int (*encode_cb)(const char *buf, size_t size, void *data),
			 void *cb_data,
			 uint32_t json_format_flags)
{
	json_t *obj;
	int rc;

	/*We add JSON_PRESERVE_ORDER in order to make sure the encoded
	 *attributes are written in the same order as in nla_stream.
	 */
	json_format_flags |= JSON_PRESERVE_ORDER;

	if (!encode_cb)
		return -EINVAL;

	obj = parse_nl_attrs((uint8_t *) nla_stream, nla_stream_len, hdl,
			     bytes_consumed);
	if (!obj)
		return -EINVAL;

	rc = json_dump_callback(obj, encode_cb, cb_data,
				json_format_flags);
	json_decref(obj);
	return rc;
}

