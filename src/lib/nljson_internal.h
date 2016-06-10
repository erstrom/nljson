#ifndef _NLJSON_INTERNAL_H_
#define _NLJSON_INTERNAL_H_

#include <nljson_config.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>
#include <netlink/msg.h>
#include <netlink/attr.h>

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

#define DATA_TYPE_STR "data_type"
#define DATA_TYPE_STR_LEN (sizeof(DATA_TYPE_STR) - 1)
#define ATTR_TYPE_STR "type"
#define ATTR_TYPE_STR_LEN (sizeof(ATTR_TYPE_STR) - 1)
#define LENGTH_STR "length"
#define LENGTH_STR_LEN (sizeof(LENGTH_STR) - 1)
#define VALUE_STR "value"
#define VALUE_STR_LEN (sizeof(VALUE_STR) - 1)

#define NLA_HDR_LEN 4

struct nljson_nla_policy {
	struct nla_policy *policy;
	char **id_to_str_map;
	struct nljson_nla_policy **nested;
	nljson_int_t max_attr_type;
	nljson_int_t max_nested_attr_type;
};

struct _nljson {
	struct nljson_nla_policy *policy;
};

extern const char *data_type_strings[NLA_TYPE_MAX + 1];

#endif /*_NLJSON_INTERNAL_H_*/

