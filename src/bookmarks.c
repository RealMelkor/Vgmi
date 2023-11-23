/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "strlcpy.h"
#include "macro.h"
#include "error.h"
#include "storage.h"
#include "bookmarks.h"
#include "parser.h"
#include "utf8.h"

#define FILENAME "bookmarks.txt"
struct bookmark *bookmarks = NULL;
size_t bookmark_length = 0;

int bookmark_load() {

	size_t i, j;
	FILE *f = storage_fopen(FILENAME, "r");
	int space, eof;

	if (!f) { /* default bookmarks */
		bookmark_add("gemini://geminispace.info", "Geminispace");
		bookmark_add("gemini://gmi.rmf-dev.com/repo/Vaati/Vgmi/readme",
				"Vgmi");
		bookmark_add("about:about", "Settings");
		return 0;
	}
	eof = 0;
	while (!eof) {
		uint32_t ch;
		void *ptr;
		struct bookmark bookmark = {0};
		ch = j = space = 0;
		for (i = 0; ; i++) {
			int len;
			if (utf8_fgetc(f, &ch)) {
				eof = 1;
				break;
			}
			if (ch == '\n') break;
			if (space < 2 && WHITESPACE(ch)) {
				space = 1;
				continue;
			}
			if (space == 1) {
				space = 2;
				j = 0;
			}
			len = utf8_unicode_length(ch);
			if (space) {
				if (len + j > sizeof(bookmark.name)) {
					bookmark.name[j] = 0;
					break;
				}
				utf8_unicode_to_char(&bookmark.name[j], ch);
			} else {
				if (len + j > sizeof(bookmark.url)) {
					bookmark.url[j] = 0;
					break;
				}
				utf8_unicode_to_char(&bookmark.url[j], ch);
			}
			j += len;
		}
		if (*bookmark.url) {
			bookmark_length++;
			ptr = realloc(bookmarks,
					sizeof(bookmark) * bookmark_length);
			if (!ptr) {
				free(bookmarks);
				bookmarks = NULL;
				fclose(f);
				return ERROR_MEMORY_FAILURE; 
			}
			bookmarks = ptr;
			bookmarks[bookmark_length - 1] = bookmark;
		}
		if (!eof && ch != '\n') {
			int ch;
			while (1) {
				ch = fgetc(f);
				if (ch == '\n' || ch == EOF) break;
			}
			if (ch == EOF) break;
		}
	}
	fclose(f);
	return 0;
}

int bookmark_rewrite() {

	size_t i;
	FILE *f = storage_fopen(FILENAME, "w");

	if (!f) return ERROR_ERRNO;
	for (i = 0; i < bookmark_length; i++)
		fprintf(f, "%s %s\n", bookmarks[i].url, bookmarks[i].name);
	fclose(f);

	return 0;
}

int bookmark_add(const char *url, const char *name) {

	void *ptr = realloc(bookmarks,
			(bookmark_length + 1) * sizeof(struct bookmark));
	if (!ptr) return ERROR_MEMORY_FAILURE;
	bookmarks = ptr;
	STRLCPY(bookmarks[bookmark_length].url, url);
	STRLCPY(bookmarks[bookmark_length].name, name);
	bookmark_length++;

	return 0;
}

int bookmark_remove(size_t id) {
	size_t i;
	if (id >= bookmark_length) return -1;
	bookmark_length--;
	for (i = id; i < bookmark_length; i++) bookmarks[i] = bookmarks[i + 1];
	return 0;
}
