/*
 * ISC License
 * Copyright (c) 2024 RMF <rawmonk@rmf-dev.com>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "macro.h"
#include "strscpy.h"
#include "error.h"
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

	char *data = NULL, *tmp = NULL;
	size_t length = 0;
	size_t i;
	const char title[] = "# Bookmarks\n\n";

	if (!(data = dyn_strcat(NULL, &length, V(header)))) goto fail;
	if (!(tmp = dyn_strcat(data, &length, V(title)))) goto fail;
	data = tmp;

	pthread_mutex_lock(&bookmark_mutex);
	for (i = 0; i < bookmark_length; i++) {

		char buf[sizeof(bookmarks[i]) + 512] = {0};
		int len;

		len = snprintf(V(buf), "=>%s %s\n=>/%ld Delete\n\n",
				bookmarks[i].url, bookmarks[i].name, i) + 1;
		if (!(tmp = dyn_strcat(data, &length, buf, len))) {
			pthread_mutex_unlock(&bookmark_mutex);
			goto fail;
		}
		data = tmp;
	}
	pthread_mutex_unlock(&bookmark_mutex);

	*out = data;
	*length_out = length;
	return 0;
fail:
	if (tmp) data = tmp;
	free(data);
	return ERROR_MEMORY_FAILURE;
}
