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
#define CONFIG_INTERNAL
#include "config.h"
#define ABOUT_INTERNAL
#include "about.h"

int about_config_arg(const char *param, char **out, size_t *length_out) {
	int id = atoi(param);
	size_t length = 0;
	char *data;
	if (!id && strcmp(param, "0")) return ERROR_INVALID_ARGUMENT;
	if (!(data = dyn_strcat(NULL, &length, V("10 test\r\n11\0"))))
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
		switch (fields[i].type) {
		case VALUE_INT:
	       		len = snprintf(V(buf), "=>%d %s = %d\n", i,
					fields[i].name, *(int*)fields[i].ptr);
			break;
		case VALUE_STRING:
	       		len = snprintf(V(buf), "=>%d %s = %s\n", i,
					fields[i].name, (char*)fields[i].ptr);
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
