#ifndef _NLJSON_H_
#define _NLJSON_H_

#include <stdint.h>
#include <stdio.h>
#define NLJSON_OK (0)

typedef struct _nljson nljson_t;

/**
 * Init family of functions.
 *
 * Initializes an nljson handle.
 * The init functions must be called prior to any other nljson
 * function calls.
 * These functions will allocate the handle and create the nl attribute
 * policy from the JSON input. The different init functions will read
 * the JSON policy in different ways.
 */

/**
 * Init function reading the nla JSON policy from a buffer.
 *
 * @arg hdl               Handle that will be allocated
 *
 * @arg json_format_flags Flags for the JSON input decoding of the
 *                        nla policy.
 *                        Passed directly to the jansson library.
 *                        See jansson documentation for more info.
 *
 * @arg policy_json       The nla policy definition string.
 *                        Must be a valid JSON string.
 *                        If NULL, no policy will be created and
 *                        all decoded attributes will have the value
 *                        set to UNKNOWN_ATTR_<attr_type> and data_type
 *                        set to NLA_UNSPEC.
 */
int nljson_init(nljson_t **hdl,
		uint32_t json_format_flags,
		const char *policy_json);

/**
 * Init function reading the nla JSON policy from a file.
 *
 * @arg hdl               Handle that will be allocated
 *
 * @arg json_format_flags Flags for the JSON input decoding of the
 *                        nla policy.
 *                        Passed directly to the jansson library.
 *                        See jansson documentation for more info.
 *
 * @arg policy_file       The path to a nla policy definition file.
 *                        The path must point to a valid JSON file.
 *                        If NULL, no policy will be created and
 *                        all decoded attributes will have the value
 *                        set to UNKNOWN_ATTR_<attr_type> and data_type
 *                        set to NLA_UNSPEC.
 */
int nljson_init_file(nljson_t **hdl,
		     uint32_t json_format_flags,
		     const char *policy_file);

/**
 * Init function using a callback function to read the nla JSON
 * policy.
 *
 * @arg hdl               Handle that will be allocated
 *
 * @arg json_format_flags Flags for the JSON input decoding of the
 *                        nla policy.
 *                        Passed directly to the jansson library.
 *                        See jansson documentation for more info.
 * @arg read_policy_cb    will be called continuously by nljson_init
 *                        when reading the JSON encoded nla policy
 *                        definition.
 *                        The callback is expected to fill buf with
 *                        at most size bytes and return the number
 *                        of bytes written.
 *                        If NULL, no policy will be created and
 *                        all decoded attributes will have the value
 *                        set to UNKNOWN_ATTR_<attr_type> and data_type
 *                        set to NLA_UNSPEC.
 * @arg cb_data           pointer that will passed to read_policy_cb.
 */
int nljson_init_cb(nljson_t **hdl,
		   uint32_t json_format_flags,
		   size_t (*read_policy_cb)(void *buf, size_t size, void *data),
		   void *cb_data);

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
 * These functions will encode a stream of netlink attribute (nla_stream)
 * into a JSON encode string using the nla policy that was created in the
 * init step.
 *
 * If the handle (hdl) is NULL (no nljson_init function has been called)
 * all decoded attributes will have the value set to UNKNOWN_ATTR_<attr_type>
 * and data_type set to NLA_UNSPEC
 */

/**
 * Encodes a stream of nl attributes and stores the result in output.
 * If output is not big enough, an error will be returned.
 *
 * @arg hdl               The nljson handle. Must be allocated by one
 *                        of the init functions.
 *                        The handle holds the nla policy used when
 *                        parsing nla_stream.
 *                        If NULL, all decoded attributes will have the
 *                        value set to UNKNOWN_ATTR_<attr_type> and
 *                        data_type set to NLA_UNSPEC
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
 * @arg json_format_flags Flags for the JSON output formating.
 *                        Passed directly to the jansson library.
 *                        See jansson documentation for more info.
 */
int nljson_encode_nla(nljson_t *hdl,
		      const void *nla_stream,
		      size_t nla_stream_len,
		      char *output,
		      size_t output_len,
		      size_t *bytes_consumed,
		      size_t *bytes_produced,
		      uint32_t json_format_flags);

/**
 * Similar to nljson_encode_nla but the output buffer is allocated
 * by the function and returned to the caller.
 * The caller is responsible for deallocating the buffer.
 * In case of error, NULL will be returned.
 *
 * @arg hdl               The nljson handle. Must be allocated by one
 *                        of the init functions.
 *                        The handle holds the nla policy used when
 *                        parsing nla_stream.
 *                        If NULL, all decoded attributes will have the
 *                        value set to UNKNOWN_ATTR_<attr_type> and
 *                        data_type set to NLA_UNSPEC
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
 * @arg json_format_flags Flags for the JSON output formating.
 *                        Passed directly to the jansson library.
 *                        See jansson documentation for more info.
 */
char *nljson_encode_nla_alloc(nljson_t *hdl,
			      const void *nla_stream,
			      size_t nla_stream_len,
			      size_t *bytes_consumed,
			      size_t *bytes_produced,
			      uint32_t json_format_flags);

/**
 * Similar to nljson_encode_nla but the output is passed (in chunks)
 * to the callback encode_cb.
 *
 * @arg hdl               The nljson handle. Must be allocated by one
 *                        of the init functions.
 *                        The handle holds the nla policy used when
 *                        parsing nla_stream.
 *                        If NULL, all decoded attributes will have the
 *                        value set to UNKNOWN_ATTR_<attr_type> and
 *                        data_type set to NLA_UNSPEC
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
 * @arg json_format_flags Flags for the JSON output formating.
 *                        Passed directly to the jansson library.
 *                        See jansson documentation for more info.
 */
int nljson_encode_nla_cb(nljson_t *hdl,
			 const void *nla_stream,
			 size_t nla_stream_len,
			 size_t *bytes_consumed,
			 int (*encode_cb)(const char *buf, size_t size, void *data),
			 void *cb_data,
			 uint32_t json_format_flags);

/**
 * Decode family of functions.
 *
 * These functions will decode JSON encoded netlink attributes into
 * a stream of binary data. The binary data stream (nla_stream) can be
 * written directly into an nlmsg.
 */

/**
 * Decodes a JSON encoded string of nl attributes into the byte stream
 * nla_stream. nla_stream is a continuous stream of nl attributes that
 * can be written directly into an nlmsg.
 *
 * @arg input             JSON encoded input string.
 *
 * @arg input_len         Length of the input string.
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
 */
int nljson_decode_nla(const char *input,
		      void *nla_stream,
		      size_t nla_stream_buf_len,
		      size_t *bytes_consumed,
		      size_t *bytes_produced,
		      uint32_t json_decode_flags);

/**
 * Similar to nljson_decode_nla but the output buffer is allocated
 * by the function and returned to the caller.
 * The caller is responsible for deallocating the buffer.
 * In case of error, NULL will be returned.
 *
 * @arg input             JSON encoded input string.
 *
 * @arg input_len         Length of the input string.
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
 */
void *nljson_decode_nla_alloc(const char *input,
			      size_t *bytes_consumed,
			      size_t *bytes_produced,
			      uint32_t json_decode_flags);


/**
 * Similar to nljson_decode_nla but the output is passed (in chunks)
 * to the callback decode_cb.
 *
 * @arg input             JSON encoded input string.
 *
 * @arg input_len         Length of the input string.
 *
 * @arg bytes_consumed    The number of bytes read (consumed) from
 *                        input.
 *
 * @arg decode_cb         will be called continuously when writing
 *                        the decoded nla output stream.
 *
 * @arg json_decode_flags Flags for the JSON input parsing.
 *                        Passed directly to the jansson library.
 *                        See jansson documentation for more info.
 */
int nljson_decode_nla_cb(const char *input,
			 size_t *bytes_consumed,
			 int (*decode_cb)(const void *buf, size_t size, void *data),
			 void *cb_data,
			 uint32_t json_decode_flags);

#endif

