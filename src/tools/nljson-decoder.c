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

static char input_file[256];
static char output_file[256];

static void print_usage(const char *argv0)
{
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "%s [ -i | --input input_file ] ( -o | --output output_file ) ( -f | --flags json_flags )\n", argv0);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  -f, --flags              format flags for the JSON decoder.\n");
	fprintf(stderr, "                           See jansson library documentation for more details.\n");
	fprintf(stderr, "  -i, --input              JSON encoded input file.\n");
	fprintf(stderr, "                           If omitted, the JSON input will be read from stdin.\n");
	fprintf(stderr, "  -o, --output             netlink attribute output stream.\n");
	fprintf(stderr, "                           If omitted, the nla byte stream will be written to stdout.\n");

}

static void do_decode(bool input_file_set, bool output_file_set,
		      uint32_t json_format_flags)
{
	int rc = 0, in_fd, out_fd;
	char in_buf[1024];
	uint8_t out_buf[1024];
	size_t in_buf_offset = 0;

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
		size_t bytes_consumed, bytes_produced;

		read_len = read(in_fd, in_buf + in_buf_offset,
				sizeof(in_buf) - in_buf_offset);
		if (read_len <= 0)
			break;

		while (read_len > 0) {
			rc = nljson_decode_nla(in_buf, out_buf,
					       sizeof(out_buf),
					       &bytes_consumed, &bytes_produced,
					       json_format_flags);
			if (rc || (bytes_produced == 0))
				break;
			write_len = write(out_fd, out_buf, bytes_produced);
			if (write_len != bytes_produced)
				break;
			if (bytes_consumed < read_len) {
				in_buf_offset = read_len - bytes_consumed;
				memmove(in_buf, in_buf + bytes_consumed, in_buf_offset);
			} else {
				in_buf_offset = 0;
			}
			read_len -= bytes_consumed;
		}
	}
out:
	if (in_fd > 0)
		close(in_fd);
	if (out_fd > 1)
		close(out_fd);
}

int main(int argc, char *argv[])
{
	int opt, optind = 0;
	uint32_t json_format_flags = 0;
	char *tmp;
	bool input_file_set = false, output_file_set = false;
	struct option long_opts[] = {
		{"help", no_argument, 0, 'h'},
		{"flags", required_argument, 0, 'f'},
		{"input", required_argument, 0, 'i'},
		{"output", required_argument, 0, 'o'},
		{NULL, 0, 0, 0},
	};

	while ((opt = getopt_long(argc, argv, "hf:i:o:", long_opts, &optind)) != -1) {
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
		case 'h':
		default:
			print_usage(argv[0]);
			return 0;
		}
	}

	do_decode(input_file_set, output_file_set, json_format_flags);
	return 0;
}

