/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#include <pthread.h>
#include "macro.h"
#include "strlcpy.h"
#include "error.h"
#include "utf8.h"
#include "storage.h"
#define HISTORY_INTERNAL
#include "history.h"

#define HISTORY "history.txt"
pthread_mutex_t history_mutex = PTHREAD_MUTEX_INITIALIZER;

struct history_entry *history = NULL;

int history_load(const char *path) {
	struct history_entry entry = {0};
	struct history_entry *last = NULL;
	FILE *f = storage_fopen(path, "r");
	unsigned int i, part;
	if (!f) return ERROR_STORAGE_ACCESS;

	i = part = 0;
	while (1) {
		uint32_t ch = fgetc(f);
		char *ptr;
		int len = utf8_char_length(ch);
		if (len > 1) {
			char buf[32];
			int pos = 0;
			if ((unsigned)len >= sizeof(buf)) break;
			buf[pos] = ch;
			for (pos = 1; pos < len; pos++) {
				buf[pos] = fgetc(f);
				if (buf[pos] == EOF) break;
			}
			if (buf[pos] == EOF) break;
			buf[pos] = 0;
			utf8_char_to_unicode(&ch, buf);
		}
		if ((int)ch == EOF) break;
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
	return 0;
}

void history_init() {
	history_load(HISTORY);
}

int history_write(const char *path) {

	FILE *f;
	struct history_entry *entry;

	f = storage_fopen(path, "w");
	if (!f) return -1;
	for (entry = history; entry; entry = entry->next) {
		fprintf(f, "%s %s\n", entry->url, entry->title);
	}
	fclose(f);

	return 0;
}

int history_save() {
	return history_write(HISTORY);
}

int history_add(const char *url, const char *title) {
	struct history_entry *entry;
	entry = malloc(sizeof(*entry));
	if (!entry) return ERROR_MEMORY_FAILURE;
	UTF8CPY(entry->title, title);
	UTF8CPY(entry->url, url);
	pthread_mutex_lock(&history_mutex);
	entry->next = history;
	history = entry;
	pthread_mutex_unlock(&history_mutex);
	return 0;
}

void history_free() {
	struct history_entry *next;
	while (history) {
		next = history->next;
		free(history);
		history = next;
	}
}
