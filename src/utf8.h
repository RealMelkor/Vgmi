/*
 * MIT License
 * Copyright (c) 2010-2020 nsf <no.smile.face@gmail.com>
 *		 2015-2022 Adam Saponara <as@php.net>
 */
int utf8_char_length(char c);
int utf8_char_to_unicode(uint32_t *out, const char *c);
int utf8_unicode_to_char(char *out, uint32_t c);
const char *utf8_next(const char **ptr);
