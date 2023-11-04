/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
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
		(codepoint < ' ' && codepoint != '\n' && codepoint != '\t'));
}

int readnext(int fd, uint32_t *ch, size_t *pos) {

	char buf[64];
	int len;

	if (read(fd, buf, 1) != 1) return -1;
	len = utf8_char_length(*buf);
	if (len > 1 && read(fd, &buf[1], len - 1) != len - 1) return -1;
	if (len > 0 && pos) *pos += len;
	utf8_char_to_unicode(ch, buf);
	return 0;
}

int skip_meta(int fd, size_t *pos, size_t length) {
	uint32_t prev = 0;
	int found = 0;
	for (*pos = 0; *pos < length; ) {
		uint32_t ch;
		if (readnext(fd, &ch, pos)) return -1;
		if (ch == '\n' && prev == '\r') {
			found = 1;
			break;
		}
		prev = ch;
	}
	return -(!found);
}

int vread(int fd, void *buf, size_t nbytes) {
	ssize_t len = read(fd, buf, nbytes);
	return len != (ssize_t)nbytes;
}
