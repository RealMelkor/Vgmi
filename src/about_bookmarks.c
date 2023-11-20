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

	length = sizeof(header) + sizeof(title);
	data = malloc(length);
	if (!data) return ERROR_MEMORY_FAILURE;
	strlcpy(data, header, length);
	strlcpy(&data[sizeof(header) - 1], title, length - sizeof(header));
	length -= 2;

	for (i = 0; i < bookmark_length; i++) {

		char buf[1024];
		int len;
		void *tmp;

		len = snprintf(V(buf), "=>%s %s\n=>/%ld Delete\n\n",
				bookmarks[i].url, bookmarks[i].name, i) + 1;
		tmp = realloc(data, length + len);
		if (!tmp) {
			free(data);
			return ERROR_MEMORY_FAILURE;
		}
		data = tmp;
		strlcpy(&data[length], buf, len);
		length += len - 1;
	}

	readonly(data, length, out);
	free(data);
	*length_out = length;
	return 0;
}
