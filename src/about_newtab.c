/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@rmf-dev.com>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "macro.h"
#include "strlcpy.h"
#include "error.h"
#include "bookmarks.h"
#define ABOUT_INTERNAL
#include "about.h"

char newtab_page_header[] =
HEADER \
"# Vgmi - " VERSION "\n\n" \
"A Gemini client written in C with vim-like keybindings\n\n## Bookmarks\n\n";

int about_newtab(char **out, size_t *length_out) {
	size_t length = 0, i;
	char *data = NULL;
	if (!(data = dyn_strcat(NULL, &length, V(newtab_page_header))))
		goto fail;
	for (i = 0; i < bookmark_length; i++) {
		char line[sizeof(struct bookmark) + 8];
		size_t line_length;
		line_length = snprintf(V(line), "=>%s %s\n",
				bookmarks[i].url, bookmarks[i].name) + 1;
		if (!(data = dyn_strcat(data, &length, line, line_length)))
			goto fail;
	}
	if (!(data = dyn_strcat(data, &length, V("\n")))) goto fail;
	if (!(data = dyn_strcat(data, &length, V(HELP_INFO)))) goto fail;

	*out = data;
	*length_out = length;
	return 0;
fail:
	free(data);
	return ERROR_MEMORY_FAILURE;
}
