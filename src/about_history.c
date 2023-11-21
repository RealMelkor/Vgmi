/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#include <stdio.h>
#include <stdlib.h>
#include "macro.h"
#include "strlcpy.h"
#include "error.h"
#define ABOUT_INTERNAL
#include "about.h"
#define HISTORY_INTERNAL
#include "history.h"

int about_history(char **out, size_t *length_out) {

	size_t length = 0;
	char *data = NULL;
	struct history_entry *entry;
	const char title[] = "# History\n\n";

	if (!(data = dyn_strcat(NULL, &length, V(header)))) goto fail;
	if (!(data = dyn_strcat(data, &length, V(title)))) goto fail;

	for (entry = history; entry; entry = entry->next) {
		char buf[sizeof(*entry)];
		int len = snprintf(V(buf), "=>%s %s\n",
				entry->url, entry->title) + 1;
		if (!(data = dyn_strcat(data, &length, buf, len))) goto fail;
	}

	*out = data;
	*length_out = length;
	return 0;
fail:
	free(data);
	return ERROR_MEMORY_FAILURE;
}
