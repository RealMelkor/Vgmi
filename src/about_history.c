/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#include <stdio.h>
#include <stdlib.h>
#include "macro.h"
#include "strlcpy.h"
#define ABOUT_INTERNAL
#include "about.h"
#define HISTORY_INTERNAL
#include "history.h"

char *about_history(size_t *length_out) {

	size_t length = 0;
	char *data = NULL;
	struct history_entry *entry;
	const char title[] = "# History\n\n";

	data = malloc(sizeof(header) + sizeof(title) + 1);
	if (!data) return NULL;
	length = sizeof(header) + sizeof(title) - 1;
	strlcpy(data, header, length + 1);
	strlcpy(&data[sizeof(header) - 1], title, length + 1 - sizeof(header));
	length -= 1;

	for (entry = history; entry; entry = entry->next) {
		char buf[sizeof(*entry)], *tmp;
		int len = snprintf(V(buf), "=>%s %s\n",
				entry->url, entry->title) + 1;
		tmp = realloc(data, length + len + 1);
		if (!tmp) {
			free(data);
			return NULL;
		}
		data = tmp;
		strlcpy(&data[length], buf, len);
		length += len - 1;
	}
	*length_out = length;
	return data;
}
