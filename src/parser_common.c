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
	return !(codepoint == 0xFEFF || codepoint == 127 ||
		((mk_wcwidth(codepoint) < 1 || codepoint < ' ') &&
		 codepoint != '\n' && codepoint != '\t'));
}

int readnext(int fd, uint32_t *ch, size_t *pos, size_t length) {

	char buf[16] = {0};
	int len;

	if (read(fd, buf, 1) != 1) return -1;
	len = utf8_char_length(*buf);
	if ((size_t)len >= sizeof(buf)) return -1;
	if (*pos + len > length) {
		if (*pos + 1 < length) {
			size_t bytes = length - *pos - 1;
			if (vread(fd, buf, bytes)) return -1;
		}
		*ch = '\0';
		*pos = length;
		return 0;
	}
	if (len > 1 && vread(fd, &buf[1], len - 1)) return -1;
	if (len > 0) *pos += len;
	utf8_char_to_unicode(ch, buf);

	return 0;
}

int vread(int fd, void *buf, size_t nbytes) {
	ssize_t len;
	char *ptr = buf;
	while (nbytes) {
		len = read(fd, ptr, nbytes);
		if (len < 1) return -1;
		nbytes -= len;
		ptr += len;
	}
	return 0;
}
