/* quirc -- QR-code recognition library
 * Copyright (C) 2010-2012 Daniel Beer <dlbeer@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "lib/quirc.h"
#include <stdio.h>
#include <string.h>

static void dump_data(const struct quirc_data *data)
{
	printf("    Version: %d\n", data->version);
	printf("    ECC level: %c\n", "MLHQ"[data->ecc_level]);
	printf("    Mask: %d\n", data->mask);
	printf("    Data type: %d (%s)\n",
			data->data_type, data_type_str(data->data_type));
	printf("    Length: %d\n", data->payload_len);
	printf("    Payload: %s\n", data->payload);

	if (data->eci)
		printf("    ECI: %d\n", data->eci);
}

static void position(const struct quirc_code *code)
{
	int u, v;

	printf("    %d cells, corners:", code->size);
	for (u = 0; u < 4; u++)
		printf(" (%d,%d)", code->corners[u].x,
				code->corners[u].y);
	printf("\n");
}


static void dump_info(struct quirc *q)
{
	int count = quirc_count(q);
	int i;

	printf("%d QR-codes found:\n\n", count);
	for (i = 0; i < count; i++) {
		struct quirc_code code;
		struct quirc_data data;
		quirc_decode_error_t err;

		quirc_extract(q, i, &code);
		err = quirc_decode(&code, &data);

		dump_cells(&code);
		printf("\n");

		if (err) {
			printf("  Decoding FAILED: %s\n", quirc_strerror(err));
		} else {
			printf("  Decoding successful:\n");
			dump_data(&data);
		}

		printf("\n");
	}
}




int main(int argc, char **argv)
{
	struct quirc *q;

	printf("quirc inspection program\n");
	printf("Copyright (C) 2010-2012 Daniel Beer <dlbeer@gmail.com>\n");
	printf("Library version: %s\n", quirc_version());
	printf("\n");

	if (argc < 2) {
		fprintf(stderr, "Usage: %s <testfile.jpg|testfile.png>\n", argv[0]);
		return -1;
	}

	q = quirc_new();
	if (!q) {
		perror("can't create quirc object");
		return -1;
	}

	int status = -1;
	if (check_if_png(argv[1])) {
		status = load_png(q, argv[1]);
	} else {
		status = load_jpeg(q, argv[1]);
	}
	if (status < 0) {
		quirc_destroy(q);
		return -1;
	}

	quirc_end(q);
	dump_info(q);

	quirc_destroy(q);
	return 0;
}
