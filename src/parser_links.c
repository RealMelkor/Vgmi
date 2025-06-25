/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@rmf-dev.com>
 */
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "macro.h"
#include "strlcpy.h"
#include "strnstr.h"
#include "utf8.h"
#include "url.h"
#include "error.h"
#include "page.h"
#include "request.h"
#define PARSER_INTERNAL
#include "parser.h"

int format_link(const char *link, size_t length,
			char *out, size_t out_length) {
	char buf[1024], *ptr, *end;
	/* remove ./ and ../ at the start */
	if (length > 2 && !memcmp(link, "./", 2)) {
		end = buf + STRLCPY(buf, &link[2]);
	} else if (length > 3 && !memcmp(link, "../", 3)) {
		end = buf + STRLCPY(buf, &link[3]);
	} else {
		end = buf + STRLCPY(buf, link);
	}
	if (end >= buf + sizeof(buf)) end = buf + sizeof(buf) - 1;
	/* remove /./ */
	while ((ptr = strnstr(buf, "/./", sizeof(buf)))) {
		while (ptr++ < end - 2) {
			*ptr = *(ptr + 2);
		}
	}
	/* parse /../ */
	while ((ptr = strnstr(buf, "/../", sizeof(buf)))) {
		char *to = ptr + sizeof("/..") - 1;
		while (ptr > buf && *(--ptr) != '/') ;
		while (*ptr) *(ptr++) = *(to++);
	}
	/* add a slash at the end if the URL doesn't have one */
	if (strstr(buf, "gemini://") == buf) {
		if (!strchr(&buf[sizeof("gemini://")], '/')) {
			size_t len = strnlen(buf, sizeof(buf));
			if (len < sizeof(buf) - 1) {
				buf[len] = '/';
				buf[len + 1] = '\0';
			}
		}
	}
	strlcpy(out, buf, out_length);
	return 0;
}

int parse_links(int in, size_t length, int out) {

	int newline, link, header, ignore, ignore_mode;
	size_t i, pos;
	char title[1024] = {0};

	pos = link = header = ignore_mode = ignore = 0;
	newline = 1;
	link = 0;
	for (i = 0; i < length; ) {

		uint32_t ch;

		if (readnext(in, &ch, &i, length)) return -1;
		if (newline && ch == '`') {
			ignore = 1;
			newline = 0;
			continue;
		}
		if (ignore) {
			if (ch == '`') {
				if (ignore++ < 2) continue;
				ignore_mode = !ignore_mode;
				ignore = 0;
				continue;
			}
			ignore = 0;
		}
		if (ignore_mode) {
			newline = ch == '\n';
			continue;
		}
		if (header == 2) {
			if (pos + utf8_unicode_length(ch) >= sizeof(title)) {
				header = 0;
				continue;
			}
			if (ch == '\n') {
				header = 0;
				newline = 1;
				continue;
			}
			if (ch == '\t') ch = ' ';
			if (renderable(ch))
				pos += utf8_unicode_to_char(&title[pos], ch);
		}
		if (header == 1 && WHITESPACE(ch)) {
			header++;
		}
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
			if (!pos && ch == '#') {
				header = 1;
			}
			newline = 0;
			continue;
		}

		while (i < length) {
			if (readnext(in, &ch, &i, length)) return -1;
			if (!WHITESPACE(ch)) break;
		}
		if (i >= length) break;

		link = 0;
		header = 0;

		if (ch == '\n') {
			newline = 1;
			continue;
		}

		{
			char link[MAX_URL] = {0};
			char buf[MAX_URL];
			size_t link_length;
			link_length = utf8_unicode_to_char(link, ch);

			while (i < length) {
				size_t next;
				if (readnext(in, &ch, &i, length)) return -1;
				if (SEPARATOR(ch)) break;
				next = link_length + utf8_unicode_length(ch);
				if (next >= sizeof(link)) {
					link_length = next;
					break;
				}
				utf8_unicode_to_char(&link[link_length], ch);
				link_length = next;
			}
			link_length++;
			/* ignore links above the length limit */
			if (link_length > sizeof(link)) {
				newline = ch == '\n';
				continue;
			}

			format_link(link, link_length, V(buf));
			url_parse_idn(buf, V(link));
			url_convert(link, V(buf));
			link_length = strnlen(V(buf));
			if (link_length < 1) {
				link_length = STRLCPY(buf, ".");
			}
			write(out, P(link_length));
			write(out, buf, link_length);
		}

		newline = 1;

	}
	i = -1;
	write(out, P(i));
	write(out, V(title));
	return 0;
}
