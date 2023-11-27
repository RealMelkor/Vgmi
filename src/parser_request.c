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
#include "sandbox.h"
#include "gemini.h"
#include "page.h"
#include "request.h"
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

	*code = gemini_status_code(i);
	return 0;
}

void parser_request(int in, int out) {
	parser_sandbox(out, "vgmi [request]");
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
		if (request.status == GMI_SUCCESS) {
			if (!request.meta[0]) {
				STRLCPY(request.meta, "text/gemini");
			}
			request.page.mime = parser_mime(V(request.meta));
			request.page.offset = bytes;
		} else {
			request.page.mime = 0;
			request.page.offset = 0;
		}
		write(out, P(request.page.mime));
		write(out, P(request.page.offset));

		if (request.status == GMI_SUCCESS &&
				is_gemtext(V(request.meta))) {
			if (parse_links(in, length - bytes, out)) {
				break;
			}
		} else {
			uint8_t byte;
			size_t i;
			for (i = bytes; i < length; i++) read(in, &byte, 1);
		}
	}
}
