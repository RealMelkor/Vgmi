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
#include "page.h"
#include "request.h"
#include "parser.h"

int metadata_trim(const char *metadata, size_t len, char *out, size_t outlen) {
	size_t i;
	for (i = 0; i < len && i < outlen; i++) {
		if (WHITESPACE(metadata[i]) || metadata[i] == ';') break;
		out[i] = metadata[i];
	}
	out[i] = '\0';
	return 0;
}

int parser_mime(char *meta, size_t len) {
	char mime[1024];
	metadata_trim(meta, len, V(mime));
	if (!STRCMP(mime, "text/gemini")) return MIME_GEMTEXT;
	if (!STRCMP(mime, "text/plain")) return MIME_PLAIN;
	if (!memcmp(mime, "text/", sizeof("text/") - 1)) return MIME_TEXT;
	if (!memcmp(mime, "image/", sizeof("image/") - 1)) return MIME_IMAGE;
	return MIME_UNKNOWN;
}

int is_gemtext(char *meta, size_t len) {
	return parser_mime(meta, len) == MIME_GEMTEXT;
}
