/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@rmf-dev.com>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "macro.h"
#include "strscpy.h"
#include "error.h"
#include "config.h"
#define ABOUT_INTERNAL
#include "about.h"
#define HISTORY_INTERNAL
#include "history.h"
#include "utf8.h"

#define MAXIMUM_LIST_LENGTH 2000

int about_history_param(const char *param) {
	if (strcmp(param, "clear")) return ERROR_INVALID_ARGUMENT;
	return history_clear();
}

int about_history(char **out, size_t *length_out) {

	size_t length = 0, i = 0;
	char *data = NULL;
	struct history_entry *entry;
	const char title[] = "# History\n\n=>clear Clear History\n\n";

	if (!(data = dyn_strcat(NULL, &length, V(header)))) goto fail;
	if (!(data = dyn_strcat(data, &length, V(title)))) goto fail;
	if (!config.enableHistory) {
		const char str[] = "History is disabled\n";
		if (!(data = dyn_strcat(data, &length, V(str)))) goto fail;
	}

	for (entry = history; entry; entry = entry->next) {
		char buf[sizeof(*entry)];
		int len = snprintf(V(buf), "=>%s %s\n", entry->url, entry->title) + 1;
		if (!(data = dyn_strcat(data, &length, buf, len))) goto fail;
		if (i++ == MAXIMUM_LIST_LENGTH) break;
	}

	*out = data;
	*length_out = length;
	return 0;
fail:
	free(data);
	return ERROR_MEMORY_FAILURE;
}
