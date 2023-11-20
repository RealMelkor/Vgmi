/*
 * MIT License
 * Copyright (c) 2010-2020 nsf <no.smile.face@gmail.com>
 *		 2015-2022 Adam Saponara <as@php.net>
 */
#include <stddef.h>
#include <stdint.h>
#include "wcwidth.h"

static const unsigned char utf8_length[256] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5,
	5, 6, 6, 1, 1};

static const unsigned char utf8_mask[6] = {0x7f, 0x1f, 0x0f, 0x07, 0x03, 0x01};

int utf8_char_length(char c) {
	return utf8_length[(unsigned char)c];
}

int utf8_unicode_length(uint32_t c) {
	int len;
	if (c < 0x80) {
		len = 1;
	} else if (c < 0x800) {
		len = 2;
	} else if (c < 0x10000) {
		len = 3;
	} else if (c < 0x200000) {
		len = 4;
	} else if (c < 0x4000000) {
		len = 5;
	} else {
		len = 6;
	}
	return len;
}

int utf8_char_to_unicode(uint32_t *out, const char *c) {

	int i;
	unsigned char len, mask;
	uint32_t result;

	len = utf8_char_length(*c);
	mask = utf8_mask[len - 1];
	result = c[0] & mask;
	for (i = 1; i < len; ++i) {
		result <<= 6;
		result |= c[i] & 0x3f;
	}

	*out = result;
	return (int)len;
}

int utf8_unicode_to_char(char *out, uint32_t c) {
	int len = 0;
	int first;
	int i;

	if (c < 0x80) {
		first = 0;
		len = 1;
	} else if (c < 0x800) {
		first = 0xc0;
		len = 2;
	} else if (c < 0x10000) {
		first = 0xe0;
		len = 3;
	} else if (c < 0x200000) {
		first = 0xf0;
		len = 4;
	} else if (c < 0x4000000) {
		first = 0xf8;
		len = 5;
	} else {
		first = 0xfc;
		len = 6;
	}

	for (i = len - 1; i > 0; --i) {
		out[i] = (c & 0x3f) | 0x80;
		c >>= 6;
	}
	out[0] = c | first;

	return len;
}

const char *utf8_next(const char **ptr) {
	int i = utf8_char_length(**ptr);
	*ptr += i;
	return *ptr;
}

int utf8_previous(const char *ptr, int i) {
	if (i) i--;
	while (i > 0 && (ptr[i] & 0xC0) == 0x80) i--;
	return i;
}

int utf8_width(const char *ptr, size_t length) {

	int width;
	size_t i;

	width = 0;
	for (i = 0; i < length; ) {
		uint32_t ch;
		i += utf8_char_to_unicode(&ch, &ptr[i]);
		if (!ch) break;
		width += mk_wcwidth(ch);
	}
	return width;
}

int utf8_cpy(char *dst, const char *src, size_t length) {
	size_t i;
	for (i = 0; i < length; ) {
		size_t len = utf8_char_length(src[i]);
		if (i + len >= length) {
			dst[i] = '\0';
			break;
		}
		while (len--) {
			dst[i] = src[i];
			i++;
		}
	}
	return 0;
}
