/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@rmf-dev.com>
 */
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
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
		if (vread(fd, &buf[i], 1)) break;
		if (i && buf[i] == '\n' && buf[i - 1] == '\r') {
			found = 1;
			break;
		}
	}
	*bytes_read = i + found;
	if (!found || i < 1) {
		return -1;
	}
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
	if (parser_sandbox(out, "vgmi [request]")) return;
	while (1) {

		int ret, bytes = 0;
		size_t length = 0;
		struct request request = {0};

		if (vread(in, P(length))) break;
		ret = parse_response(in, length, V(request.meta),
				&request.status, &bytes);
		if (request.status == -1) ret = ERROR_INVALID_STATUS;
		if (ret) {
			uint8_t byte;
			request.status = -1;
			for (; bytes < (signed)length; bytes++)
				if (vread(in, P(byte))) break;
			if ((size_t)bytes != length) break;
			if (vwrite(out, P(request.status))) break;
			if (vwrite(out, P(ret))) break;
			continue;
		}

		if (vwrite(out, P(request.status))) break;
		if (request.status == GMI_SUCCESS && !request.meta[0]) {
			STRLCPY(request.meta, "text/gemini");
		}
		if (vwrite(out, V(request.meta))) break;
		if (request.status == GMI_SUCCESS) {
			request.page.mime = parser_mime(V(request.meta));
			request.page.offset = bytes;
		} else {
			request.page.mime = 0;
			request.page.offset = 0;
		}
		if (vwrite(out, P(request.page.mime))) break;
		if (vwrite(out, P(request.page.offset))) break;

		if (request.status == GMI_SUCCESS &&
				is_gemtext(V(request.meta))) {
			if (parse_links(in, length - bytes, out)) {
				break;
			}
		} else {
			uint8_t byte;
			size_t i;
			for (i = bytes; i < length; i++)
				if (vread(in, &byte, 1)) break;
			if ((size_t)i != length) break;
		}
	}
}
