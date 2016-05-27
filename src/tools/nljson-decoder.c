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
	fprintf(stderr, "%s [ -i | --input input_file ] ( -o | --output output_file ) ( -f | --flags json_flags ) ( -a | --ascii )\n", argv0);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  -f, --flags              format flags for the JSON decoder.\n");
	fprintf(stderr, "                           See jansson library documentation for more details.\n");
	fprintf(stderr, "  -i, --input              JSON encoded input file.\n");
	fprintf(stderr, "                           If omitted, the JSON input will be read from stdin.\n");
	fprintf(stderr, "  -o, --output             netlink attribute output stream.\n");
	fprintf(stderr, "                           If omitted, the nla byte stream will be written to stdout.\n");
	fprintf(stderr, "  -a, --ascii              ASCII output. Print output in ASCII format.\n");

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
		size_t consumed, produced;

		read_len = read(in_fd, in_buf + in_buf_offset,
				sizeof(in_buf) - in_buf_offset);
		if (read_len <= 0)
			break;

		while (read_len > 0) {
			rc = nljson_decode_nla(in_buf, out_buf,
					       sizeof(out_buf),
					       &consumed, &produced,
					       json_format_flags);
			if (rc || (produced == 0) || (consumed == 0))
				break;
			if (ascii_output)
				write_len = write_ascii(out_fd, out_buf,
							produced);
			else
				write_len = write(out_fd, out_buf, produced);
			if ((size_t) write_len != produced)
				break;
			if (consumed < (size_t) read_len) {
				in_buf_offset = read_len - consumed;
				memmove(in_buf, in_buf + consumed,
					in_buf_offset);
			} else {
				in_buf_offset = 0;
			}
			read_len -= consumed;
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
	char *tmp;
	struct option long_opts[] = {
		{"help", no_argument, 0, 'h'},
		{"flags", required_argument, 0, 'f'},
		{"input", required_argument, 0, 'i'},
		{"output", required_argument, 0, 'o'},
		{"ascii", no_argument, 0, 'a'},
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
		case 'h':
		default:
			print_usage(argv[0]);
			return 0;
		}
	}

	do_decode();
	return 0;
}

