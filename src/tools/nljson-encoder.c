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

static char policy_file[256];
static char input_file[256];
static char output_file[256];

static uint8_t in_buf[IN_BUF_LEN];
static char out_buf[IN_BUF_LEN];

static uint32_t json_format_flags;
static bool policy_file_set, input_file_set, output_file_set;

static void print_usage(const char *argv0)
{
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "%s [ -p | --policy policy_file ] [ -i | --input input_file ] ( -o | --output output_file ) ( -f | --flags json_flags )\n", argv0);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  -p, --policy             netlink attribute policy file in JSON format.\n");
	fprintf(stderr, "                           If omitted, the encoded JSON nla output will have all attributes set as NLA_UNSPEC.\n");
	fprintf(stderr, "  -f, --flags              format flags for the JSON encoded output.\n");
	fprintf(stderr, "                           See jansson library documentation for more details.\n");
	fprintf(stderr, "  -i, --input              netlink attribute input file.\n");
	fprintf(stderr, "                           If omitted, the nla byte stream will be read from stdin.\n");
	fprintf(stderr, "  -o, --output             JSON encoded output stream.\n");
	fprintf(stderr, "                           If omitted, the JSON output will be written to stdout.\n");

}

static void do_encode(void)
{
	int rc = 0, in_fd, out_fd;
	nljson_t *hdl = NULL;
	size_t in_buf_offset = 0;

	if (policy_file_set)
		rc = nljson_init_file(&hdl, 0, policy_file);

	if (rc)
		goto out;

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
	 * Reads the input stream and encodes the data.
	 * Trailing input data (data not processed by the encoding functions)
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
			rc = nljson_encode_nla(hdl, in_buf, read_len, out_buf,
					       sizeof(out_buf),
					       &consumed, &produced,
					       json_format_flags);
			if (rc || (produced == 0) || (consumed == 0))
				break;
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
	if (hdl)
		nljson_deinit(&hdl);
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
		{"policy", required_argument, 0, 'p'},
		{"flags", required_argument, 0, 'f'},
		{"input", required_argument, 0, 'i'},
		{"output", required_argument, 0, 'o'},
		{NULL, 0, 0, 0},
	};

	while ((opt = getopt_long(argc, argv, "hp:f:i:o:", long_opts, &optind)) != -1) {
		switch (opt) {
		case 'p':
			strncpy(policy_file, optarg, sizeof(policy_file));
			policy_file_set = true;
			break;
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

	do_encode();
	return 0;
}
