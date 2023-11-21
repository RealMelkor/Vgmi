/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "macro.h"
#include "strlcpy.h"
#include "error.h"
#include "memory.h"
#include "bookmarks.h"
#define ABOUT_INTERNAL
#include "about.h"

int about_bookmarks_param(const char *param) {
	int id = atoi(param), ret;
	if (!id && strcmp(param, "0")) return ERROR_INVALID_ARGUMENT;
	if ((ret = bookmark_remove(id))) return ret;
	if ((ret = bookmark_rewrite())) return ret;
	return 0;
}

int about_bookmarks(char **out, size_t *length_out) {

	char *data = NULL;
	size_t length = 0;
	size_t i;
	const char title[] = "# Bookmarks\n\n";

	if (!(data = dyn_strcat(NULL, &length, V(header)))) goto fail;
	if (!(data = dyn_strcat(data, &length, V(title)))) goto fail;

	for (i = 0; i < bookmark_length; i++) {

		char buf[1024];
		int len;

		len = snprintf(V(buf), "=>%s %s\n=>/%ld Delete\n\n",
				bookmarks[i].url, bookmarks[i].name, i) + 1;
		if (!(data = dyn_strcat(data, &length, buf, len))) goto fail;
	}

	readonly(data, length, out);
	free(data);
	*length_out = length;
	return 0;
fail:
	free(data);
	return ERROR_MEMORY_FAILURE;
}
