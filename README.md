# nljson

nljson is a library: libnljson and a set of programs: nljson-encoder and nljson-decoder
that can be used to encode/decode a stream of netlink attributes (obtained from a program
using libnl, such as iw or iwraw) to/from a JSON representation.

It can be used together with iw and iwraw to configure and debug wireless network
hardware.

## How to build

nljson uses cmake.

It is recommended to use a separate build directory in order not to mix
the build artifacts with the source code.

Basic build using default configuration:

```sh
mkdir build
cd build
cmake ..
make
```

If you want to customize the build, you will have to add build options to
cmake.

Below shows how to build libnljson as a static lib.

```sh
mkdir build
cd build
cmake -DNLJSON_BUILD_SHARED_LIB=0 ..
make
```

Packet installation:

```sh
make install
```

The installation prefix can be changed by using the CMAKE_INSTALL_PREFIX
option:

```sh
cmake -DCMAKE_INSTALL_PREFIX=/home/user/ ..
make
make install
```

### Dependencies

libnl-3.0 and jansson (https://github.com/akheron/jansson)

### Cross compilation

The easiest way of cross compiling is to create a toolchain file with all necessary
cmake variables defined and then involve cmake with -DCMAKE_TOOLCHAIN_FILE

Toolchain file:

```cmake
SET(CMAKE_SYSTEM_NAME Linux)

SET(CMAKE_C_COMPILER <path to c-compiler>)
SET(CMAKE_FIND_ROOT_PATH <path to cross-toolchain staging root dir>)
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
```

Build command:

```sh
mkdir build
cd build
cmake -DCMAKE_TOOLCHAIN_FILE=<path to toolchain file> ..
make
```

## Intended usage

As mentioned earlier, the intended usage of nljson is with nl80211 and wireless
network drivers.

It is mainly targeted as a tool to construct and parse nl80211 vendor commands
and events together with another program called iwraw.

Please consult the iwraw documentation for more info on this.

## netlink overview

The netlink sockets where introduced in version 2.2 of linux kernel as a kernel to
user space (and vice versa) communication method.
It is the preferred user to kernel space communication method for all new drivers,
replacing ioctl.

The normal way of using netlink sockets is not to use the socket API directly,
but rather using a wrapper library called libnl.
libnl exports functions for reading and writing nl messages and to add/read
netlink attributes to/from a netlink message.

Most of the nl based protocols use attributes containing all protocol data.
This is the recommended way of writing an nl based protocol since it is easy
to extend it in future versions.
see http://www.infradead.org/~tgr/libnl/doc/core.html\#core\_attr

A netlink attribute consists of a 4 byte header + payload.
The header is defined by struct nlattr like this:

```c
struct nlattr {
        uint16_t           nla_len;
        uint16_t           nla_type;
};
```

nla_len is the length of the entire payload with the header included,
so the actual payload length is 4 bytes less.
nla_type is the attribute identifier. It should not be confused with the
nla policy type which is a data_type definition (more about policies in
a later section)

The payload is added immediately after the header:

~~~
+---------+----------+--------------+---------+
| nla_len | nla_type | payload_data | padding |
+---------+----------+--------------+---------+
<-- struct nlattr -->
~~~

An nla stream (netlink attribute stream) is a stream of netlink attributes
where several attributes have been concatenated.
The start of each attribute must be 4 byte aligned, so there might be
padding bytes added in between attributes.
The pictures found in the libnl doxygen documentation also shows a padding
field between the header and the payload. This is actually incorrect since
struct nlattr is 4 bytes long and no padding is needed between header and payload.

nla stream:
~~~
+---------+----------+--------------+---------+---------+----------+------------
| nla_len | nla_type | payload_data | padding | nla_len | nla_type | payload ...
+---------+----------+--------------+---------+---------+----------+-----------
<-- struct nlattr -->                         <-- struct nlattr -->
~~~

## <a name="json_representation"></a> JSON representation of netlink attributes

Perhaps the best way to explain the nljson JSON representation of an nla stream
is with an example:

```json
{
    "ATTR_TYPE_1": {
        "data_type": "NLA_U16",
        "nla_type": 100,
        "nla_len": 2,
        "value": 56
    },
    "ATTR_TYPE_2": {
        "data_type": "NLA_STRING",
        "nla_type": 101,
        "nla_len": 12,
        "value": "Hello world"
    },
    "ATTR_TYPE_3": {
        "data_type": "NLA_UNSPEC",
        "nla_type": 102,
        "nla_len": 4,
        "value": [
            132,
            0,
            0,
            0
        ]
    }
}
```

The above example shows the JSON representation of a stream of 3 attributes.
Each attribute is identified by a unique key string. Usually the key has the
same name as the enum used to define nla_type, but any string will do.
The purpose of the attribute key is to easily identify the attribute.

The attribute itself contains a couple of keys defining the attribute.
The "nla_type" and "nla_len" keys are perhaps most obvious since they have a
direct mapping to struct nlattr.
"nla_len" is the length of the payload and not the full attribute length
(as in struct nlattr)

"data_type" specifies the data type of the attribute. This value is not part of the
attribute stream. The reason for having it in the attribute definition is to
make it easier to interpret the attribute data.
If it is set to any of the integer types (NLA_U8 to NLA_U64) or NLA_STRING, the
"nla_len" key can be omitted. The attribute length is then calculated from the
data type or the string length.

"value" is the actual value of the attribute. This corresponds to the attribute
payload. Depending on the value of "data_type", "value" is expected to be either
an integer, string, array or object.
If "data_type" is an integer (NLA_U8 to NLA_U64), "value" must be an integer.
The same thing goes with strings. If "data_type" is an NLA_STRING, the value
must also be a string.
NLA_UNSPEC data types requires an array and NLA_NESTED requires another JSON
object (nested attributes is described in the next [section](#json_representation_nested)).
If there is a mismatch between "data_type" and "value" the decoding will fail.

### <a name="json_representation_nested"></a> Nested netlink attributes.

Attributes in an attribute stream might have payloads containing other attributes
(or attribute streams).
Attributes thar are embedded in other attributes payloads are called nested
attributes.

Below is an example of a JSON representation containing nested attributes:

```json
{
    "NL80211_ATTR_VENDOR_ID": {
        "data_type": "NLA_U32",
        "nla_type": 195,
	"value": 4980
    },
    "NL80211_ATTR_VENDOR_SUBCMD": {
        "data_type": "NLA_U32",
        "nla_type": 196,
        "value": 1
    },
    "NL80211_ATTR_VENDOR_DATA": {
        "data_type": "NLA_NESTED",
        "nla_type": 197,
        "value": {
            "QCA_WLAN_VENDOR_ATTR_TEST": {
                "data_type": "NLA_U32",
                "nla_type": 8,
                "value": 5
            }
        }
    }
}
```

"NL80211_ATTR_VENDOR_DATA" is a nested attribute. "value" is another JSON object
describing the nested attribute.

## JSON decoding

Decoding the JSON representation from section [JSON representation](#json_representation)
results in the below nla stream:

```
06 00 64 00 38 00 00 00 10 00 65 00 48 65 6C 6C 6F 20 77 6F 72 6C 64 00 08 00 66 00 84 00 00 00
```

Note the padding between attributes 100 (0x64) and 101 (0x65). The first attribute
(0x64) is an NLA_U16, so the attribute length is only 6 bytes.
Thus, two padding bytes have been added before the next attribute (0x65).

Since the data type and attribute name is not included in the nla stream, there
are several possible JSON representations of the same nla stream.

The below JSON representation will be decoded into the same nla stream as the
one from section [JSON representation](#json_representation).
The difference with this JSON representation is that all
attributes have "data_type" set to NLA_UNSPEC and consequently all values are
arrays.

```json
{
    "UNKNOWN_ATTR_100": {
        "data_type": "NLA_UNSPEC",
        "nla_type": 100,
        "nla_len": 2,
        "value": [
            56,
            0
        ]
    },
    "UNKNOWN_ATTR_101": {
        "data_type": "NLA_UNSPEC",
        "nla_type": 101,
        "nla_len": 12,
        "value": [
            72,
            101,
            108,
            108,
            111,
            32,
            119,
            111,
            114,
            108,
            100,
            0
        ]
    },
    "UNKNOWN_ATTR_102": {
        "data_type": "NLA_UNSPEC",
        "nla_type": 102,
        "nla_len": 4,
        "value": [
            132,
            0,
            0,
            0
        ]
    }
}
```

## JSON encoding

Encoding nla attributes into a JSON representation involves parsing of
an attribute stream.

Since netlink attributes does not contain any information of how they are
interpreted (only length, type and payload) we need a so called nla
policy that defines how to interpret the attributes.

The nla policy is defined by struct nla_policy like this:

```c
struct nla_policy {
        /** Type of attribute or NLA_UNSPEC */
        uint16_t        type;

        /** Minimal length of payload required */
        uint16_t        minlen;

        /** Maximal length of payload allowed */
        uint16_t        maxlen;
};
```

struct nla_policy describes how to interpret a specific netlink attribute.
type is the data type of the attribute, maxlen is the maximum length of
the attribute and minlen is the minimum length of the attribute.

If maxlen is set to zero, it will be ignored (no maximum length validation
will be made by nla_parse and nla_validate).
If minlen is set to zero, the data type's fixed length will be used.
This is only applicable for those data types that has a fixed length like
the integer types (NLA_U8 to NLA_64).
If type is NLA_UNSPEC, minlen must be set, otherwise the validation will fail.
Note that maxlen and minlen refers to the attribute payload length and not
the nla_len field in struct nlattr.

The normal way of using an nla_policy is to create an array of struct nla_policy,
where each index is the attribute type. Like this:

```c
#define ATTR_OFFSET (100)
#define ATTR_TYPE_2_MAX_LEN (28)
#define ATTR_TYPE_3_LEN (4)

enum {
        ATTR_TYPE_1 = ATTR_OFFSET,
        ATTR_TYPE_2,
        ATTR_TYPE_3,
        ATTR_MAX = ATTR_TYPE_3
};

static const struct nla_policy my_policy[ATTR_MAX + 1] = {
        [ATTR_TYPE_1] = { .type = NLA_U16 },
        [ATTR_TYPE_2] = { .type = NLA_STRING, .maxlen = ATTR_TYPE_1_MAX_LEN },
        [ATTR_TYPE_3] = { .type = NLA_UNSPEC. .minlen = ATTR_TYPE_3_LEN, .maxlen = ATTR_TYPE_3_LEN}
};
```

Note that the policy array must have a length equal to the value
of the maximum attribute type. In the above example, my_policy has a
length of 103 elements.

The JSON representation of my_policy used by nljson looks like this:

```json
{
    "ATTR_TYPE_1": {
        "data_type": "NLA_U16",
        "nla_type": 100
    },
    "ATTR_TYPE_2": {
        "data_type": "NLA_STRING",
        "nla_type": 101,
        "maxlen": 28
    },
    "ATTR_TYPE_3": {
        "data_type": "NLA_UNSPEC",
        "nla_type": 102,
        "minlen": 4,
        "maxlen": 4
    }
}
```

"data_type" maps to the type member in struct nla_policy and defines
the data type of the attribute.

"nla_type" maps to the nla_type member in struct nlattr.

"minlen" and "maxlen" maps to the minlen and maxlen  members in struct nla_policy

The libnljson init family of functions will read the JSON policy definition
and create an internal struct nla_policy array used in the parsing.

If the policy is not available all attributes will be set to
UNKNOWN_ATTR_<attr_type>, where <attr_type> is the attribute type value.
If attributes that are not included in the policy array are encountered, 
these attributes will be set to UNKNOWN_ATTR_<attr_type> as well.

Example:
Let's take the attribute stream defined in the [JSON representation](#json_representation) section
and encode it back into JSON:

```
06 00 64 00 38 00 00 00 10 00 65 00 48 65 6C 6C 6F 20 77 6F 72 6C 64 00 08 00 66 00 84 00 00 00
```

When encoding without a policy definition the end result will look something like this:

```json
{
    "UNKNOWN_ATTR_100": {
        "data_type": "NLA_UNSPEC",
        "nla_type": 100,
        "nla_len": 2,
        "value": [
            56,
            0
        ]
    },
    "UNKNOWN_ATTR_101": {
        "data_type": "NLA_UNSPEC",
        "nla_type": 101,
        "nla_len": 12,
        "value": [
            72,
            101,
            108,
            108,
            111,
            32,
            119,
            111,
            114,
            108,
            100,
            0
        ]
    },
    "UNKNOWN_ATTR_102": {
        "data_type": "NLA_UNSPEC",
        "nla_type": 102,
        "nla_len": 4,
        "value": [
            132,
            0,
            0,
            0
        ]
    }
}
```

When encoding with a policy definition the end result will have the data interpreted in the
right way:

```json
{
    "ATTR_TYPE_1": {
        "data_type": "NLA_U16",
        "nla_type": 100,
        "nla_len": 2,
        "value": 56
    },
    "ATTR_TYPE_2": {
        "data_type": "NLA_STRING",
        "nla_type": 101,
        "nla_len": 12,
        "value": "Hello world"
    },
    "ATTR_TYPE_3": {
        "data_type": "NLA_UNSPEC",
        "nla_type": 102,
        "nla_len": 4,
        "value": [
            132,
            0,
            0,
            0
        ]
    }
}
```

This is the same JSON definition that was used in section [JSON representation](#json_representation)

### Nested policies

Policy definitions can be nested in the same way as attributes.
In fact, when encoding streams containing nested attributes we must have
a policy definition with nested policies.

Below is a policy definition for the nested attribute stream defined in section
 [Nested netlink attributes](#json_representation_nested)

```json
{
    "NL80211_ATTR_VENDOR_DATA": {
        "data_type": "NLA_NESTED",
        "nla_type": 197,
        "nested": {
            "SUBCMD_8": {
                "data_type": "NLA_U32",
                "nla_type": 8
            }
        }
    }
}
```

A JSON policy object containing a nested policy is expected to have a "nested"
key with another policy object as its value.
The nested policy must comply with the same rules as all other policy definitions
and can have its own nested policies as well.
There is no limit for how deep the nesting can be.

## nljson library (libnljson)

The library is documented in the API header: include/nljson.h
Doxygen documentation can be generated like this:

```sh
cd docs
doxygen
```

The doxygen output is stored in: docs/doxygen

The nljson tools can also be used as an example of how to use the library.

## nljson tools
The nljson tools consists of two programs that are depending on the nljson library:
nljson-decoder and nljson-encoder.

nljson-decoder reads a JSON encoded nla stream from an input file or
stdin and writes a nla stream to an output file or stdout. The output stream
could be either in ASCII or binary.

Like this:

```sh
# Read JSON from nla_stream.json and write binary output to nla_stream.bin
cat nla_stream.json | nljson-decoder > nla_stream.bin
# Read JSON from nla_stream.json and write ASCII output to stdout
cat nla_stream.json | nljson-decoder -a
```

nljson-encoder works the other way around and converts a binary nla stream
into a JSON encoded representation. It can also read/write from/to a file
or stdin/stdout in the same way as the decoder.

As mentioned in previous sections, the encoder requires an nla policy in order
to perform the encoding in a good manner.
The policy can be provided as an argument to the program.

Examples:

```sh
# Encode the nla stream without using a policy
cat nla_stream.json | nljson-decoder | nljson_decoder --json-flags 4
# Encode the nla stream using a policy
cat nla_stream.json | nljson-decoder | nljson_decoder --json-flags 4 -p policy.json
```

## nljson tools and nl80211

### nl80211 and iw

For wireless drivers a netlink configuration API called nl80211 is used.
In its simplest form, nl80211.h is a defintion of netlink attributes and
commands used by all cfg80211 capable network drivers.
These attributes can be sent to the driver (for configuration) or received from
the driver (either as events or command responses).

iw is a userspace tool that creates and transmits netlink messages (containing
attributes defined in nl80211.h) to the network drivers.

### nljson and iw

Below are some examples of how to use nljson together with iw.
Note that it is recommended to use nljson together with iwraw instead of iw.
Examples of how to use nljson and iwraw can be found in the iwraw documentation.

Send vendor command to driver (in this case mac80211_hwsim) and
print the response in ASCII format to stdout:

```sh
cat vendor-command.json | nljson-decoder | iw dev wlan0 vendor recv 0x1374 0x1 -
```

If vendor-command.json looks like this:

```json
{
    "QCA_WLAN_VENDOR_ATTR_TEST": {
        "data_type": "NLA_U32",
        "nla_type": 8,
        "value": 5
    }
}
```

then, the response will look like this:

```
vendor response: 08 00 08 00 07 00 00 00
```

We can also pipe the received output to nljson-encoder.
In this case we must use the "vendor recvbin" command with iw since
nljson-encoder does not (currently) support ASCII input.

```sh
cat vendor-command.json | nljson-decoder | iw dev wlan0 vendor recvbin 0x1374 0x1 - | nljson-encoder -f4 -p vendor-policy.json
```

If vendor-policy.json looks like this:

```json
{
    "SUBCMD_8": {
        "data_type": "NLA_U32",
        "nla_type": 8
    }
}
```

then, the JSON output will look like this:

```json
{
    "SUBCMD_8": {
        "data_type": "NLA_U32",
        "nla_type": 8,
        "nla_len": 4,
        "value": 7
    }
}
```


