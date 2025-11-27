/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@rmf-dev.com>
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include "strscpy.h"
#include "macro.h"
#include "error.h"
#include "storage.h"
#include "bookmarks.h"
#include "parser.h"
#include "utf8.h"

#define FILENAME "bookmarks.txt"
struct bookmark *bookmarks = NULL;
size_t bookmark_length = 0;
pthread_mutex_t bookmark_mutex = PTHREAD_MUTEX_INITIALIZER;

int bookmark_load(void) {

	size_t j;
	FILE *f = storage_fopen(FILENAME, "r");
	int space, eof;

	if (!f) { /* default bookmarks */
		bookmark_add("gemini://tlgs.one",
				"TLGS - \"Totally Legit\" Gemini Search");
		bookmark_add("about:about", "Settings");
		return 0;
	}
	eof = 0;
	while (!eof) {
		uint32_t ch;
		void *ptr;
		struct bookmark bookmark = {0};
		ch = j = space = 0;
		for (;;) {
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
				if (len + j >= sizeof(bookmark.name)) {
					bookmark.url[0] = 0;
					break;
				}
				utf8_unicode_to_char(&bookmark.name[j], ch);
			} else {
				if (len + j >= sizeof(bookmark.url)) {
					bookmark.url[0] = 0;
					break;
				}
				utf8_unicode_to_char(&bookmark.url[j], ch);
			}
			j += len;
		}
		if (*bookmark.url) {
			ptr = realloc(bookmarks,
				sizeof(bookmark) * (bookmark_length + 1));
			if (!ptr) {
				free(bookmarks);
				bookmarks = NULL;
				fclose(f);
				return ERROR_MEMORY_FAILURE; 
			}
			bookmarks = ptr;
			bookmarks[bookmark_length] = bookmark;
			bookmark_length++;
		}
		if (!eof && ch != '\n') {
			int ch = 0;
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

int bookmark_rewrite(void) {

	size_t i;
	FILE *f = storage_fopen(FILENAME, "w");

	if (!f) return ERROR_ERRNO;
	for (i = 0; i < bookmark_length; i++)
		fprintf(f, "%s %s\n", bookmarks[i].url, bookmarks[i].name);
	fclose(f);

	return 0;
}

int bookmark_add(const char *url, const char *name) {
	void *ptr;
	pthread_mutex_lock(&bookmark_mutex);
	if (bookmark_length > SIZE_MAX / sizeof(struct bookmark) - 1)
		return ERROR_INTEGER_OVERFLOW;
	ptr = realloc(bookmarks,
			(bookmark_length + 1) * sizeof(struct bookmark));
	if (!ptr) {
		pthread_mutex_unlock(&bookmark_mutex);
		return ERROR_MEMORY_FAILURE;
	}
	bookmarks = ptr;
	STRSCPY(bookmarks[bookmark_length].url, url);
	STRSCPY(bookmarks[bookmark_length].name, name);
	bookmark_length++;
	pthread_mutex_unlock(&bookmark_mutex);
	return 0;
}

int bookmark_remove(size_t id) {
	pthread_mutex_lock(&bookmark_mutex);
	if (id >= bookmark_length) {
		pthread_mutex_unlock(&bookmark_mutex);
		return -1;
	}
	bookmark_length--;
	if (id < bookmark_length) {
		memmove(&bookmarks[id], &bookmarks[id + 1],
			(bookmark_length - id) * sizeof(struct bookmark));

	}
	pthread_mutex_unlock(&bookmark_mutex);
	return 0;
}
