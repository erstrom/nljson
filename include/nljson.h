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

#ifndef _NLJSON_H_
#define _NLJSON_H_

#include <stddef.h>
#include <stdint.h>

#define NLJSON_ERR_STR_LEN (256)

/**
 * When this flag is set, the encoder will skip all unknown attributes,
 * i.e. attributes not present in the policy.
 */
#define NLJSON_FLAG_SKIP_UNKNOWN_ATTRS (1)

/**
 * When this flag is set, the encoder will add a time stamp to each
 * encoded message. */
#define NLJSON_FLAG_ADD_TIMESTAMP (2)

/**
 * nljson handle.
 */
typedef struct _nljson nljson_t;

/**
 * Structure used to describe an error that has occurred during
 * any operation (encoding, decoding or initialization).
 */
struct nljson_error {
	/**
	 * Error message describing the error
	 */
	char err_msg[NLJSON_ERR_STR_LEN];
	/**
	 * Error code (if applicable)
	 * The error code will be one of the errors defined in errno.h
	 * Some errors might not have an error code (none of the errno.h
	 * codes are applicable). In this case, err_code will be set to 0
	 */
	int err_code;
};

/**
 * Init family of functions.
 *
 * Initializes an nljson handle.
 * These functions will allocate the handle and create the nl attribute
 * policy from the JSON input. The different init functions will read
 * the JSON policy in different ways.
 *
 * The allocated handle can be used with the encode family of functions
 * in order to control the behavior of the encoding.
 */

/**
 * Init function reading the nla JSON policy from a buffer.
 *
 * @arg hdl               Handle that will be allocated
 *
 * @arg json_format_flags Flags for the JSON decoding of the nla policy.
 *                        Passed directly to the jansson library.
 *                        See jansson documentation for more info.
 *
 * @arg nljson_flags      Flags for the JSON encoding of the nla stream.
 *                        See description of each flag for more info.
 *                        These flags will be used by the encode functions
 *                        when called with the handle obtained from this
 *                        function.
 *
 * @arg policy_json       The nla policy definition string.
 *                        Must be a valid JSON string.
 *                        If NULL, the handle will not contain any policy.
 *                        The policy will be used by the encode functions
 *                        when called with the handle obtained from this
 *                        function.
 *                        See description of encode functions for more
 *                        details.
 *
 * @arg error             Error output. The struct must be allocated by the
 *                        caller.
 *
 * @return 0 on success or -1 on error.
 *
 * In case of error, *error will be written with a description of the error.
 */
int nljson_init(nljson_t **hdl,
		uint32_t json_format_flags,
		uint32_t nljson_flags,
		const char *policy_json,
		struct nljson_error *error);

/**
 * Init function reading the nla JSON policy from a file.
 *
 * @arg hdl               Handle that will be allocated
 *
 * @arg json_format_flags Flags for the JSON decoding of the nla policy.
 *                        Passed directly to the jansson library.
 *                        See jansson documentation for more info.
 *
 * @arg nljson_flags      Flags for the JSON encoding of the nla stream.
 *                        See description of each flag for more info.
 *                        These flags will be used by the encode functions
 *                        when called with the handle obtained from this
 *                        function.
 *
 * @arg policy_json       The nla policy definition string.
 *                        Must be a valid JSON string.
 *                        The policy will be used by the encode functions
 *                        when called with the handle obtained from this
 *                        function.
 *                        If NULL, the handle will not contain any policy.
 *                        See description of encode functions for more
 *                        details.
 *
 * @arg error             Error output. The struct must be allocated by the
 *                        caller.
 *
 * @return 0 on success or -1 on error.
 *
 * In case of error, *error will be written with a description of the error.
 */
int nljson_init_file(nljson_t **hdl,
		     uint32_t json_format_flags,
		     uint32_t nljson_flags,
		     const char *policy_file,
		     struct nljson_error *error);

/**
 * Init function using a callback function to read the nla JSON
 * policy.
 *
 * @arg hdl               Handle that will be allocated
 *
 * @arg json_format_flags Flags for the JSON decoding of the nla policy.
 *                        Passed directly to the jansson library.
 *                        See jansson documentation for more info.
 *
 * @arg nljson_flags      Flags for the JSON encoding of the nla stream.
 *                        See description of each flag for more info.
 *                        These flags will be used by the encode functions
 *                        when called with the handle obtained from this
 *                        function.
 *
 * @arg read_policy_cb    will be called continuously by nljson_init
 *                        when reading the JSON encoded nla policy
 *                        definition.
 *                        The callback is expected to fill buf with
 *                        at most size bytes and return the number
 *                        of bytes written.
 *                        If NULL, no policy definition will be read and
 *                        the handle will not contain any policy.
 *                        See description of encode functions for more
 *                        details.
 *
 * @arg cb_data           pointer that will passed to read_policy_cb.
 *
 * @arg error             Error output. The struct must be allocated by the
 *                        caller.
 *
 * @return 0 on success or -1 on error.
 *
 * In case of error, *error will be written with a description of the error.
 */
int nljson_init_cb(nljson_t **hdl,
		   uint32_t json_format_flags,
		   uint32_t nljson_flags,
		   size_t (*read_policy_cb)(void *buf, size_t size, void *data),
		   void *cb_data,
		   struct nljson_error *error);

/**
 * De-initializes the nljson handle by freeing all memory allocated
 * by nljson_init and sets the handle pointer to NULL.
 *
 * @arg hdl               The handle that will be freed
 */
void nljson_deinit(nljson_t **hdl);

/**
 * Encode family of functions.
 *
 * These functions will encode a stream of netlink attributes (nla_stream)
 * into a JSON encoded string using the nla policy that was created in the
 * init step.
 *
 * If the handle (hdl) is NULL (no nljson_init function has been called)
 * or if the handle does not contain any policy (no policy was provided
 * to nljson_init) all encoded attributes will have the value set to
 * UNKNOWN_ATTR_<attr_type> and data_type set to NLA_UNSPEC.
 * <attr_type> is the attr_type value of the attribute.
 *
 * This behavior is overridden if NLJSON_FLAG_SKIP_UNKNOWN_ATTRS was
 * provided to any of the nljson_init functions.
 * In this case, all unknown attributes (attributes not present in the
 * policy) will be skipped. A consequence of this is that there will be no
 * output at all if the handle does not contain a policy.
 */

/**
 * Encodes a stream of nl attributes and stores the result in output.
 * If output is not big enough, an error will be returned.
 *
 * @arg hdl               The nljson handle. Must be allocated by one
 *                        of the init functions.
 *
 * @arg nla_stream        Stream of bytes containing netlink attributes
 *
 * @arg nla_stream_len    The length of the netlink attribute byte stream.
 *
 * @arg output            The output buffer the encoded JSON string will
 *                        be written to.
 *
 * @arg output_len        The length of the output buffer.
 *
 * @arg bytes_consumed    The number of bytes read (consumed) from
 *                        nla_stream.
 *
 * @arg bytes_produced    The number of output bytes produced, i.e. the
 *                        length of the JSON output.
 *
 * @arg json_format_flags Flags for the JSON output formatting.
 *                        Passed directly to the jansson library.
 *                        See jansson documentation for more info.
 *
 * @arg error             Error output. The struct must be allocated by the
 *                        caller.
 *
 * @return 0 on success or -1 on error.
 *
 * In case of error, *error will be written with a description of the error.
 */
int nljson_encode_nla(nljson_t *hdl,
		      const void *nla_stream,
		      size_t nla_stream_len,
		      char *output,
		      size_t output_len,
		      size_t *bytes_consumed,
		      size_t *bytes_produced,
		      uint32_t json_format_flags,
		      struct nljson_error *error);

/**
 * Similar to nljson_encode_nla but the output buffer is allocated
 * by the function and returned to the caller.
 * The caller is responsible for deallocating the buffer.
 * In case of error, NULL will be returned.
 *
 * @arg hdl               The nljson handle. Must be allocated by one
 *                        of the init functions.
 *
 * @arg nla_stream        Stream of bytes containing netlink attributes
 *
 * @arg nla_stream_len    The length of the netlink attribute byte stream.
 *
 * @arg bytes_consumed    The number of bytes read (consumed) from
 *                        nla_stream.
 *
 * @arg bytes_produced    The number of output bytes produced, i.e. the
 *                        length of the JSON output.
 *
 * @arg json_format_flags Flags for the JSON output formatting.
 *                        Passed directly to the jansson library.
 *                        See jansson documentation for more info.
 *
 * @arg error             Error output. The struct must be allocated by the
 *                        caller.
 *
 * @return pointer to output buffer on success or NULL on error.
 *
 * In case of error, *error will be written with a description of the error.
 */
char *nljson_encode_nla_alloc(nljson_t *hdl,
			      const void *nla_stream,
			      size_t nla_stream_len,
			      size_t *bytes_consumed,
			      size_t *bytes_produced,
			      uint32_t json_format_flags,
			      struct nljson_error *error);

/**
 * Similar to nljson_encode_nla but the output is passed (in chunks)
 * to the callback encode_cb.
 *
 * @arg hdl               The nljson handle. Must be allocated by one
 *                        of the init functions.
 *
 * @arg nla_stream        Stream of bytes containing netlink attributes
 *
 * @arg nla_stream_len    The length of the netlink attribute byte stream.
 *
 * @arg bytes_consumed    The number of bytes read (consumed) from
 *                        nla_stream.
 *
 * @arg encode_cb         will be called continuously when writing
 *                        the JSON encoded nla output.
 *
 * @arg cb_data           pointer that will passed to encode_cb.
 *
 * @arg json_format_flags Flags for the JSON output formatting.
 *                        Passed directly to the jansson library.
 *                        See jansson documentation for more info.
 *
 * @arg error             Error output. The struct must be allocated by the
 *                        caller.
 *
 * @return 0 on success or -1 on error.
 *
 * In case of error, *error will be written with a description of the error.
 */
int nljson_encode_nla_cb(nljson_t *hdl,
			 const void *nla_stream,
			 size_t nla_stream_len,
			 size_t *bytes_consumed,
			 int (*encode_cb)(const char *buf,
					  size_t size,
					  void *data),
			 void *cb_data,
			 uint32_t json_format_flags,
			 struct nljson_error *error);

/**
 * Decode family of functions.
 *
 * These functions will decode JSON encoded netlink attributes into
 * a stream of binary data. The binary data stream (nla_stream) can be
 * written directly into an nlmsg.
 */

/**
 * Decodes a JSON encoded string of nl attributes into the byte stream
 * nla_stream.
 *
 * @arg input             JSON encoded input string.
 *
 * @arg nla_stream        Output: stream of bytes containing netlink attributes
 *
 * @arg nla_stream_len    The length of the output buffer.
 *
 * @arg bytes_consumed    The number of bytes read (consumed) from
 *                        input.
 *
 * @arg bytes_produced    The number of output bytes produced, i.e. the
 *                        length of the nla_stream.
 *
 * @arg json_decode_flags Flags for the JSON input parsing.
 *                        Passed directly to the jansson library.
 *                        See jansson documentation for more info.
 *
 * @arg error             Error output. The struct must be allocated by the
 *                        caller.
 *
 * @return 0 on success or -1 on error.
 *
 * In case of error, *error will be written with a description of the error.
 */
int nljson_decode_nla(const char *input,
		      void *nla_stream,
		      size_t nla_stream_buf_len,
		      size_t *bytes_consumed,
		      size_t *bytes_produced,
		      uint32_t json_decode_flags,
		      struct nljson_error *error);

/**
 * Similar to nljson_decode_nla but the output buffer is allocated
 * by the function and returned to the caller.
 * The caller is responsible for deallocating the buffer.
 *
 * @arg input             JSON encoded input string.
 *
 * @arg bytes_consumed    The number of bytes read (consumed) from
 *                        input.
 *
 * @arg bytes_produced    The number of output bytes produced, i.e. the
 *                        length of the nla_stream.
 *
 * @arg json_decode_flags Flags for the JSON input parsing.
 *                        Passed directly to the jansson library.
 *                        See jansson documentation for more info.
 *
 * @arg error             Error output. The struct must be allocated by the
 *                        caller.
 *
 * @return pointer to output buffer on success or NULL on error.
 *
 * In case of error, *error will be written with a description of the error.
 */
void *nljson_decode_nla_alloc(const char *input,
			      size_t *bytes_consumed,
			      size_t *bytes_produced,
			      uint32_t json_decode_flags,
			      struct nljson_error *error);


/**
 * Similar to nljson_decode_nla but the output is passed (in chunks)
 * to the callback decode_cb.
 *
 * @arg input             JSON encoded input string.
 *
 * @arg bytes_consumed    The number of bytes read (consumed) from
 *                        input.
 *
 * @arg decode_cb         will be called continuously when writing
 *                        the decoded nla output stream.
 *
 * @arg cb_data           pointer that will passed to decode_cb.
 *
 * @arg json_decode_flags Flags for the JSON input parsing.
 *                        Passed directly to the jansson library.
 *                        See jansson documentation for more info.
 *
 * @arg error             Error output. The struct must be allocated by the
 *                        caller.
 *
 * @return 0 on success or -1 on error.
 *
 * In case of error, *error will be written with a description of the error.
 */
int nljson_decode_nla_cb(const char *input,
			 size_t *bytes_consumed,
			 int (*decode_cb)(const void *buf,
					  size_t size,
					  void *data),
			 void *cb_data,
			 uint32_t json_decode_flags,
			 struct nljson_error *error);

#endif

