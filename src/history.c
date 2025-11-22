/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@rmf-dev.com>
 */
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#include <pthread.h>
#include "macro.h"
#include "strscpy.h"
#include "error.h"
#include "config.h"
#include "utf8.h"
#include "url.h"
#include "storage.h"
#define HISTORY_INTERNAL
#include "history.h"
#define PARSER_INTERNAL
#include "parser.h"

#define HISTORY_DISABLED (!config.enableHistory || !config.maximumHistorySize)
#define HISTORY "history.txt"
#define HISTORY_RELOAD 400 /* reload history after [x] requests */
pthread_mutex_t history_mutex = PTHREAD_MUTEX_INITIALIZER;

struct history_entry *history = NULL;
int history_length = 0;

int history_load(const char *path) {

	struct history_entry entry = {0};
	struct history_entry *last = NULL;
	FILE *f;
	unsigned int i, part, count;

	if (!(f = storage_fopen(path, "r"))) return ERROR_STORAGE_ACCESS;

	count = i = part = 0;
	for (;;) {
		uint32_t ch;
		char *ptr;
		if (utf8_fgetc(f, &ch)) break;
		if (!renderable(ch)) continue;
		if (ch == '\n') {
			struct history_entry *new;
			i = 0;
			part = 0;
			new = malloc(sizeof(*new));
			if (!new) {
				fclose(f);
				return ERROR_MEMORY_FAILURE;
			}
			*new = entry;
			if (last) {
				last->next = new;
			} else {
				new->next = NULL;
				history = new;
			}
			last = new;
			memset(&entry, 0, sizeof(entry));
			count++;
			if (count >= (unsigned)config.maximumHistorySize)
				break;
			continue;
		}
		if (!part && ch <= ' ') {
			part = 1;
			i = 0;
			continue;
		}
		if (part == 1) {
			if (ch <= ' ') continue;
			part = 2;
		}
		if (i >= (part ? sizeof(entry.title) : sizeof(entry.url))) {
			continue;
		}
		ptr = part ? entry.title : entry.url;
		i += utf8_unicode_to_char(&ptr[i], ch);
	}
	fclose(f);
	history_length = count;
	return 0;
}

void history_init(void) {
	if (HISTORY_DISABLED) return;
	history_load(HISTORY);
}

int history_write(const char *path) {

	FILE *f;
	struct history_entry *entry;
	int count = 0;

	if (HISTORY_DISABLED) return 0;
	f = storage_fopen(path, "w");
	if (!f) return -1;
	for (entry = history; entry; entry = entry->next) {
		if (count++ >= config.maximumHistorySize) break;
		fprintf(f, "%s ", entry->url);
		utf8_fprintf(f, V(entry->title));
		fprintf(f, "\n");
	}
	fclose(f);

	return 0;
}

int history_clear(void) {
	history_free();
	history = NULL;
	history_length = 0;
	return history_save();
}

int history_save(void) {
	return history_write(HISTORY);
}

int history_add(const char *url, const char *title) {

	struct history_entry *entry;
	int limit;

	if (HISTORY_DISABLED) return 0;
	pthread_mutex_lock(&history_mutex);
	limit = config.maximumHistorySize + config.maximumHistoryCache;
	if (history_length > limit) {
		/* reload history to prevents overloading memory */
		history_save();
		history_free();
		history_load(HISTORY);
	}
	history_length++;
	entry = malloc(sizeof(struct history_entry));
	if (!entry) {
		pthread_mutex_unlock(&history_mutex);
		return ERROR_MEMORY_FAILURE;
	}
	url_hide_query(title, V(entry->title));
	if (strstr(url, "gemini://") == url) {
		url_hide_query(url, V(entry->url));
	} else {
		STRSCPY(entry->url, url);
		STRSCPY(entry->title, title);
	}
	entry->next = history;
	history = entry;
	pthread_mutex_unlock(&history_mutex);
	return 0;
}

void history_free(void) {
	struct history_entry *next;
	pthread_mutex_lock(&history_mutex);
	while (history) {
		next = history->next;
		free(history);
		history = next;
	}
	pthread_mutex_unlock(&history_mutex);
}
