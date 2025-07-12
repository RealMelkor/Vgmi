/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@rmf-dev.com>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "macro.h"
#include "strscpy.h"
#include "error.h"
#define CONFIG_INTERNAL
#include "config.h"
#define ABOUT_INTERNAL
#include "about.h"

int about_config_arg(char *param, char **out, size_t *length_out) {
	int id, len;
	size_t length = 0;
	char *data;
	char *query = strchr(param, '?');
	char buf[1024];
	if (query) {
		*query = '\0';
		query++;
	}
	id = atoi(param);
	if ((!id && strcmp(param, "0")) || (unsigned)id >= LENGTH(fields)) {
		return ERROR_INVALID_ARGUMENT;
	}
	if (query) {
		int ret = config_set_field(id, query);
		if (ret) return ret;
		return about_config(out, length_out);
	}
	len = snprintf(V(buf), "10 %s\r\n0", fields[id].name) + 1;
	if (!(data = dyn_strcat(NULL, &length, buf, len)))
		return ERROR_MEMORY_FAILURE;
	*out = data;
	*length_out = length;
	return 0;
}

int about_config(char **out, size_t *length_out) {

	size_t length = 0;
	unsigned int i;
	char *data = NULL;
	const char title[] = "# Configuration\n\n";

	if (!(data = dyn_strcat(NULL, &length, V(header)))) goto fail;
	if (!(data = dyn_strcat(data, &length, V(title)))) goto fail;

	for (i = 0; i < LENGTH(fields); i++) {
		char buf[2048];
		int len = 0;
		char *info = fields[i].restart ? "(Restart required)" : "";
		switch (fields[i].type) {
		case VALUE_INT:
			len = snprintf(V(buf), "=>%d %s = %d %s\n", i,
					fields[i].name, *(int*)fields[i].ptr,
					info) + 1;
			break;
		case VALUE_STRING:
			len = snprintf(V(buf), "=>%d %s = %s %s\n", i,
					fields[i].name, (char*)fields[i].ptr,
					info) + 1;
			break;
		}
		if (!len) continue;
		if (!(data = dyn_strcat(data, &length, buf, len))) goto fail;
	}

	*out = data;
	*length_out = length;
	return 0;
fail:
	free(data);
	return ERROR_MEMORY_FAILURE;
}
