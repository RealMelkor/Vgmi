/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include "macro.h"
#include "utf8.h"
#include "wcwidth.h"
#define PAGE_INTERNAL
#include "page.h"
#include "request.h"
#define PARSER_INTERNAL
#include "parser.h"

int renderable(uint32_t codepoint) {
	return !(codepoint == 0xFEFF ||
		((mk_wcwidth(codepoint) < 1 || codepoint < ' ') &&
		 codepoint != '\n' && codepoint != '\t'));
}

int readnext(int fd, uint32_t *ch, size_t *pos, size_t length) {

	char buf[64] = {0};
	size_t len;

	if (read(fd, buf, 1) != 1) return -1;
	len = utf8_char_length(*buf);
	if (len >= sizeof(buf)) return -1;
	if (*pos + len > length) {
		if (*pos + 1 < length) {
			size_t bytes = length - *pos - 1;
			if (vread(fd, buf, bytes)) return -1;
		}
		*ch = '\0';
		*pos = length;
		return 0;
	}
	if (len > 1 && read(fd, &buf[1], len - 1) != (int)len - 1) return -1;
	if (len > 0) *pos += len;
	utf8_char_to_unicode(ch, buf);

	return 0;
}

int vread(int fd, void *buf, size_t nbytes) {
	ssize_t len = read(fd, buf, nbytes);
	return len != (ssize_t)nbytes;
}
