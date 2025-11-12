/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include "macro.h"
#include "strlcpy.h"
#include "strnstr.h"
#include "error.h"
#include "image.h"
#include "storage.h"
#define CONFIG_INTERNAL
#include "config.h"

#define DEFAULT_SEARCH_URL "gemini://tlgs.one/search?%s"
#define CONFIG_FILE "config.conf"

struct config config = {0};

void config_default(void) {
	memset(&config, 0, sizeof(config));
	config.certificateLifespan = 3600 * 24 * 365 * 2; /* 2 years */
	config.certificateBits = 2048;
	config.enableHexViewer = 1;
	config.enableMouse = 1;
	config.enableSandbox = 1;
#ifdef __linux__
	config.enableLandlock = 1;
#endif
	config.enableImage = 1;
	config.enableHistory = 1;
	config.enableXdg = 1;
	config.maximumBodyLength = 8388608;
	config.maximumDisplayLength = 1048576;
	config.imageParserScratchPad = 10485760;
	config.maximumRedirects = 5;
	config.maximumCachedPages= 15;
	config.maximumHistorySize = 3000;
	config.maximumHistoryCache = 200;
	STRLCPY(config.searchEngineURL, DEFAULT_SEARCH_URL);
	STRLCPY(config.downloadsPath, "");
	STRLCPY(config.proxyHttp, "");
	STRLCPY(config.launcher, "xdg-open");
}

static int set_field(struct field field, int v, char *str) {
	unsigned int i = 0;
	for (i = 0; i < LENGTH(fields); i++) {
		if (STRCMP(fields[i].name, field.name)) continue;
		if (fields[i].type != field.type) break;
		switch (fields[i].type) {
		case VALUE_INT:
			{
				int *integer = fields[i].ptr;
				*integer = v;
			}
			break;
		case VALUE_STRING:
			strlcpy(fields[i].ptr, str, CONFIG_STRING_LENGTH);
			break;
		}
		break;
	}
	return 0;
}

int config_set_field(int id, const char *value) {
	if (id < 0 || (unsigned)id >= LENGTH(fields))
		return ERROR_INVALID_ARGUMENT;
	switch (fields[id].type) {
	case VALUE_INT:
		{
			int ivalue = atoi(value);
			if (!ivalue && strcmp(value, "0"))
				return ERROR_INVALID_ARGUMENT;
			*(int*)fields[id].ptr = ivalue;
		}
		break;
	case VALUE_STRING:
		strlcpy(fields[id].ptr, value, CONFIG_STRING_LENGTH);
		break;
	default:
		return ERROR_INVALID_ARGUMENT;
	}
	config_correction();
	return 0;
}

int config_load(void) {
	FILE *f;
	char buf[1024];
	int i, in_str, in_comment;
	struct field field = {0};
	srand(time(NULL));
	config_default();
	if (!(f = storage_fopen(CONFIG_FILE, "r")))
		return ERROR_STORAGE_ACCESS;

	in_comment = in_str = i = 0;
	for (;;) {
		int ch = fgetc(f);
		if (ch == EOF || ch == '\n') {
			int ivalue;
			buf[i] = 0;
			in_comment = in_str = i = 0;
			ivalue = atoi(buf);
			if (ivalue || !STRCMP(buf, "0")) {
				field.type = VALUE_INT;
				set_field(field, ivalue, NULL);
			} else {
				field.type = VALUE_STRING;
				set_field(field, 0, buf);
			}
			field.name[0] = 0;
			if (ch == EOF) break;
			continue;
		}
		if (in_comment) continue;
		if (!in_str && ch <= ' ') continue;
		if (ch == '"') {
			in_str = !in_str;
			continue;
		}
		if (!in_str && ch == '#') {
			in_comment = 1;
		}
		if (!in_str && ch == '=') {
			buf[i] = 0;
			STRLCPY(field.name, buf);
			i = 0;
			continue;
		}
		if ((unsigned)i >= sizeof(buf)) {
			i = 0;
			continue;
		}
		buf[i++] = ch;
	}
	
	fclose(f);
	config_correction();
	return 0;
}

int config_save(void) {

	FILE *f;
	unsigned int i;
	if (!(f = storage_fopen(CONFIG_FILE, "w")))
		return ERROR_STORAGE_ACCESS;
	for (i = 0; i < LENGTH(fields); i++) {
		switch (fields[i].type) {
		case VALUE_INT:
			fprintf(f, "%s = %d\n", fields[i].name,
						*(int*)fields[i].ptr);
			break;
		case VALUE_STRING:
			fprintf(f, "%s = \"%s\"\n", fields[i].name,
						(char*)fields[i].ptr);
			break;
		}
	}

	fclose(f);
	return 0;
}

int config_correction(void) {
	size_t i;
	for (i = 0; i < LENGTH(fields); i++) {
		if (!STRCMP(fields[i].name, "request.maxdisplay")) {
			unsigned int *value =  fields[i].ptr;
			if (*value < 4096) *value = 4096;
		}
		if (!STRCMP(fields[i].name, "search.url")) {
			char *value = fields[i].ptr;
			char *ptr = strnstr(value, "%s", CONFIG_STRING_LENGTH);
			char *second = NULL;
			if (ptr) {
				second = strnstr(ptr + 1, "%s",
					CONFIG_STRING_LENGTH - (ptr - value));
			}
			if (!ptr || second) {
				strlcpy(fields[i].ptr, DEFAULT_SEARCH_URL,
						CONFIG_STRING_LENGTH);
			}
		}
	}
	return 0;
}
