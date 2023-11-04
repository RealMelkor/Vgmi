/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include "strlcpy.h"
#include "strnstr.h"
#include "macro.h"
#include "error.h"
#include "page.h"
#include "request.h"
#include "gemtext.h"
#define PARSER_INTERNAL
#include "parser.h"

/* fetch status code and metadata */
int parse_response(int fd, size_t length, char *meta, size_t len, int *code,
			int *bytes_read) {

	char *ptr;
	char buf[MAX_URL];
	size_t i;
	int found;

	found = 0;
	for (i = 0; i < sizeof(buf) && i < len && i < length; i++) {
		int ret = read(fd, &buf[i], 1);
		if (ret != 1) return -1;
		if (i && buf[i] == '\n' && buf[i - 1] == '\r') {
			found = 1;
			break;
		}
	}
	if (!found || i < 1) return -1;
	*bytes_read = i + 1;
	buf[i - 1] = '\0';

	ptr = strchr(buf, ' ');
	if (!ptr) return ERROR_INVALID_METADATA;
	*ptr = 0;
	i = atoi(buf);
	if (!i) return ERROR_INVALID_STATUS;

	memset(meta, 0, len);
	strlcpy(meta, ptr + 1, len);

	*code = i;
	return 0;
}

int is_gemtext(char *meta, size_t len) {

	const char *gmi = "text/gemini";
	char *ptr;

	if (!strcmp(meta, gmi)) return 1;
	ptr = strnstr(meta, ";", len);
	if (!ptr) return 0;
	return memcmp(meta, gmi, ptr - meta) == 0;
}

void parser_request(int in, int out) {
	while (1) { /* TODO: send error code */

		int ret, bytes;
		size_t length;
		struct request request = {0};

		if (vread(in, &length, sizeof(length))) break;
		ret = parse_response(in, length, V(request.meta),
				&request.status, &bytes);
		if (ret) break;

		write(out, P(request.status));
		write(out, V(request.meta));

		if (is_gemtext(V(request.meta))) {
			if (gemtext_links(in, length - bytes, out)) {
				break;
			}
		} else {
			/* TODO: check if the data seems renderable */
			uint8_t byte;
			size_t count, i;
			for (i = bytes; i < length; i++) {
				read(in, &byte, 1);
			}
			count = -1;
			write(out, P(count));
		}
	}
}
