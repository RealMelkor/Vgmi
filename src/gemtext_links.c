/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include "macro.h"
#define GEMTEXT_INTERNAL
#define GEMTEXT_INTERNAL_FUNCTIONS
#include "gemtext.h"
#include "strnstr.h"
#include "utf8.h"

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

int gemtext_links(const char *data, size_t length, int fd) {

	int newline = 1;
	size_t i;

	i = strnstr(data, "\r\n", length) - data;

	for (i = 0; i < length; ) {

		uint32_t ch;
		size_t j;

		j = utf8_char_to_unicode(&ch, &data[i]);
		if (j < 1) j = 1;
		i += j;

		if (newline && ch == '=' && data[i] == '>') {
			char url[MAX_URL], buf[MAX_URL];
			i++;
			j = 0;
			while (WHITESPACE(data[i]) && i < length)
				i += utf8_char_length(data[i]);
			while (!SEPARATOR(data[i]) && i < length &&
					j < sizeof(url)) {
				uint32_t ch;
				int len;
				len = utf8_char_to_unicode(&ch, &data[i]);
				if (ch < ' ') ch = '\0';
				memcpy(&url[j], &ch, len);
				j += len;
				i += len;
			}
			url[j] = '\0';
			j = format_link(V(url), V(buf));
			write(fd, P(j));
			write(fd, buf, j);
		}

		newline = ch == '\n';
	}
	i = -1;
	write(fd, P(i));
	return 0;
}
