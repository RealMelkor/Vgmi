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

char newtab_page_header[] =
HEADER \
"# Vgmi - " VERSION "\n\n" \
"A Gemini client written in C with vim-like keybindings\n\n## Bookmarks\n\n";

int about_newtab(char **out, size_t *length_out) {
	void *ptr;
	size_t length = sizeof(newtab_page_header), i;
	char *data = malloc(length);
	if (!data) return ERROR_MEMORY_FAILURE;
	strlcpy(data, newtab_page_header, length);
	for (i = 0; i < bookmark_length; i++) {
		char line[sizeof(struct bookmark) + 8];
		size_t line_length;
		line_length = snprintf(V(line), "=>%s %s\n",
				bookmarks[i].url, bookmarks[i].name);
		ptr = realloc(data, length + line_length + 1);
		if (!ptr) {
			free(data);
			return ERROR_MEMORY_FAILURE;
		}
		data = ptr;
		strlcpy(&data[length - 1], line, line_length + 1);
		length += line_length;
	}
	ptr = realloc(data, length + sizeof(HELP_INFO));
	if (!ptr) {
		free(data);
		return ERROR_MEMORY_FAILURE;
	}
	data = ptr;
	data[length - 1] = '\n';
	strlcpy(&data[length], HELP_INFO, sizeof(HELP_INFO));
	length += sizeof(HELP_INFO);
	*out = data;
	*length_out = length;
	return 0;
}
