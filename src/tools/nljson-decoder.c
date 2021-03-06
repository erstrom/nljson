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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <nljson.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <nljson_tools_config.h>

#define IN_BUF_LEN (1024)
#define OUT_BUF_LEN (1024)
#define ASCII_BUF_LEN (3 * OUT_BUF_LEN + 1)

static char input_file[256];
static char output_file[256];

static char in_buf[IN_BUF_LEN], ascii_buf[ASCII_BUF_LEN];
static uint8_t out_buf[OUT_BUF_LEN];

static uint32_t json_format_flags;
static bool input_file_set, output_file_set, ascii_output;

static void print_usage(const char *argv0)
{
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "%s OPTIONS\n", argv0);
	fprintf(stderr, "\n");
	fprintf(stderr, "nljson-decoder reads JSON encoded netlink attributes from\n");
	fprintf(stderr, "stdin or an input file and decodes it into a stream of netlink\n");
	fprintf(stderr, "attributes (nla stream). The decoded output is written to stdout \n");
	fprintf(stderr, "or a file.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  -f, --flags      format flags for the JSON decoder.\n");
	fprintf(stderr, "                   See jansson library documentation for more details.\n");
	fprintf(stderr, "  -i, --input      JSON encoded input file.\n");
	fprintf(stderr, "                   If omitted, the JSON input will be read from stdin.\n");
	fprintf(stderr, "  -o, --output     netlink attribute output stream.\n");
	fprintf(stderr, "                   If omitted, the nla byte stream will be written to stdout.\n");
	fprintf(stderr, "  -a, --ascii      ASCII output. Print output in ASCII format.\n");
	fprintf(stderr, "  --version        Print version info and exit.\n");
	fprintf(stderr, "\n");

}

static void print_version(void)
{
#if GIT_SHA_AVAILABLE
	fprintf(stderr, "\n%s-%s\n\n", VERSION, GIT_SHA);
#else
	fprintf(stderr, "\n%s-\n\n", VERSION);
#endif
}

static int write_ascii(int fd, const uint8_t *buf, size_t len)
{
	size_t i;
	int n = 0, write_ret;

	for (i = 0; i < len; i++)
		n += snprintf(ascii_buf + n, sizeof(ascii_buf) - n, "%02X ",
			      buf[i]);
	n += snprintf(ascii_buf + n, sizeof(ascii_buf) - n, "\n");

	write_ret = write(fd, ascii_buf, n);
	if (write_ret != n)
		return 0;
	else
		return len;
}

static void do_decode(void)
{
	int rc = 0, in_fd, out_fd;
	size_t in_buf_len = 0;
	struct nljson_error error;
	bool decode_error = false;

	if (input_file_set)
		in_fd = open(input_file, O_RDONLY);
	else
		in_fd = 0;

	if (in_fd < 0)
		goto out;

	if (output_file_set)
		out_fd = open(output_file, O_WRONLY);
	else
		out_fd = 1;

	if (out_fd < 0)
		goto out;

	/**
	 * Main processing loop:
	 * Reads the input stream and decodes the data.
	 * Trailing input data (data not processed by the decoding function)
	 * is saved for the next iteration.
	 */
	for (;;) {
		ssize_t read_len, write_len;
		size_t consumed, produced;
		bool eof_reached = false;

		read_len = read(in_fd, in_buf + in_buf_len,
				sizeof(in_buf) - in_buf_len);
		if (read_len <= 0) {
			read_len = 0;
			eof_reached = true;
		}

		in_buf_len += read_len;

		while (in_buf_len > 0) {
			size_t i;

			rc = nljson_decode_nla(in_buf, out_buf,
					       sizeof(out_buf),
					       &consumed, &produced,
					       json_format_flags,
					       &error);

			if (rc) {
				/* We don't print the error here since the
				 * error could be caused by an incomplete
				 * JSON string and we could get more data
				 * in the next iteration.
				 */
				decode_error = true;
				break;
			}

			decode_error = false;

			if ((produced == 0) || (consumed == 0))
				break;

			if (ascii_output)
				write_len = write_ascii(out_fd, out_buf,
							produced);
			else
				write_len = write(out_fd, out_buf, produced);
			if ((size_t) write_len != produced)
				break;
			if (consumed > (size_t) in_buf_len) {
				fprintf(stderr, "Error: Consumed %u bytes "
					"out of %u", consumed, produced);
				break;
			}

			in_buf_len -= consumed;
			memmove(in_buf, in_buf + consumed, in_buf_len);

			/* Make sure in_buf begins with a '{', otherwise
			 * nljson_decode_nla will fail.
			 */
			for (i = 0; i < in_buf_len; i++) {
				if (in_buf[i] == '{')
					break;
			}

			if (i > 0) {
				in_buf_len -= i;
				memmove(in_buf, in_buf + i, in_buf_len);
			}
		}

		if (eof_reached)
			break;
	}

	if (decode_error)
		/* The last iteration was an error */
		fprintf(stderr, "Decoding error: %s\n", error.err_msg);
out:
	if (in_fd > 0)
		close(in_fd);
	if (out_fd > 1)
		close(out_fd);
}

int main(int argc, char *argv[])
{
	int opt, optind = 0;
	char *tmp;
	struct option long_opts[] = {
		{"help", no_argument, 0, 'h'},
		{"flags", required_argument, 0, 'f'},
		{"input", required_argument, 0, 'i'},
		{"output", required_argument, 0, 'o'},
		{"ascii", no_argument, 0, 'a'},
		{"version", no_argument, 0, 1000},
		{NULL, 0, 0, 0},
	};

	while ((opt = getopt_long(argc, argv, "hf:i:o:a", long_opts, &optind)) != -1) {
		switch (opt) {
		case 'f':
			json_format_flags = strtoul(optarg, &tmp, 0);
			if (*tmp != '\0') {
				fprintf(stderr, "Bad JSON format flags: %s\n",
					optarg);
				return -1;
			}
			break;
		case 'i':
			strncpy(input_file, optarg, sizeof(input_file));
			input_file_set = true;
			break;
		case 'o':
			strncpy(output_file, optarg, sizeof(output_file));
			output_file_set = true;
			break;
		case 'a':
			ascii_output = true;
			break;
		case 1000:
			print_version();
			return 0;
		case 'h':
		default:
			print_usage(argv[0]);
			return 0;
		}
	}

	do_decode();
	return 0;
}

