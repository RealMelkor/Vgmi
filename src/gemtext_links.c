/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "macro.h"
#include "strlcpy.h"
#include "strnstr.h"
#define GEMTEXT_INTERNAL
#define GEMTEXT_INTERNAL_FUNCTIONS
#include "gemtext.h"
#include "utf8.h"
#include "error.h"

static int format_link(const char *link, size_t length,
			char *out, size_t out_length) {
	int i = 0, j = 0;
	uint32_t prev = 0;
	while (link[i]) {
		uint32_t ch;
		int len;
		len = utf8_char_to_unicode(&ch, &link[i]);
		if ((prev == '/' || prev == 0) && ch == '.') {
			if (link[i + len] == '/') {
				j -= 1;
				i += len;
				continue;
			} else if (link[i + len] == '.' &&
					link[i + len + 1] == '/'){
				j -= 2;
				if (j < 0) j = 0;
				while (out[j] != '/' && j)
					j = utf8_previous(out, j);
				i += len + 1;
				continue;
			}
		}
		if (i + len >= (ssize_t)length ||
				j + len >= (ssize_t)out_length) {
			out[j] = '\0';
			break;
		}
		if (ch < ' ') ch = '\0';
		memcpy(&out[j], &link[i], len);
		i += len;
		j += len;
		prev = ch;
	}
	out[j] = '\0';
	if (strstr(out, "gemini://") == out) {
		if (!strchr(&out[sizeof("gemini://")], '/')) {
			out[j++] = '/';
			out[j] = '\0';
		}
	}
	return j;
}

int gemtext_status(int fd, size_t length, char *meta, size_t len, int *code,
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

int gemtext_links(int in, size_t length, int out, int *status, char *meta,
			size_t meta_length) {

	int newline, link, ret, bytes;
	size_t i;

	ret = gemtext_status(in, length, meta, meta_length, status, &bytes);
	if (ret) return ret;
	length -= bytes;

	write(out, status, sizeof(*status));
	write(out, meta, meta_length);

	newline = 1;
	link = 0;
	/* bytes missing? */
	for (i = 0; i < length; ) {

		uint32_t ch;

		if (readnext(in, &ch, &i)) return -1;
		if (!(link && ch == '>')) {
			if (ch == '\n') {
				newline = 1;
				link = 0;
				continue;
			}
			if (!newline) {
				link = 0;
				continue;
			}
			if (ch == '=') {
				link = 1;
			}
			newline = 0;
			continue;
		}

		while (i < length) {
			if (readnext(in, &ch, &i)) return -1;
			if (!WHITESPACE(ch)) break;
		}

		link = 0;

		if (i >= length || ch == '\n') continue;

		{
			char link[MAX_URL] = {0};
			char buf[MAX_URL];
			size_t link_length;
			link_length = utf8_unicode_to_char(link, ch);

			while (i < length && link_length < sizeof(link)) {
				if (readnext(in, &ch, &i)) return -1;
				if (SEPARATOR(ch)) break;
				link_length += utf8_unicode_to_char(
						&link[link_length], ch);
			}
			link_length++;

			link_length = format_link(link, link_length, V(buf));
			write(out, P(link_length));
			write(out, buf, link_length);
		}

		newline = 1;

	}
	i = -1;
	write(out, P(i));
	return 0;
}
