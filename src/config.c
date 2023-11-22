/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "macro.h"
#include "strlcpy.h"
#include "error.h"
#include "config.h"
#include "image.h"
#include "storage.h"

#define DEFAULT_SEARCH_URL "gemini://geminispace.info/search?%s"
#define CONFIG_FILE "config.conf"

enum {
	VALUE_STRING,
	VALUE_INT
};

struct config config = {0};
struct field {
	char name[128];
	int type;
	void *ptr;
};

struct field fields[] = {
	{"hexviewer.enabled", VALUE_INT, &config.enableHexViewer},
	{"image.enabled", VALUE_INT, &config.enableImage},
	{"image.scratchpad", VALUE_INT, &config.imageParserScratchPad},
	{"sandbox.enabled", VALUE_INT, &config.enableSandbox},
	{"history.enabled", VALUE_INT, &config.enableHistory},
	{"certificate.bits", VALUE_INT, &config.certificateBits},
	{"certificate.expiration", VALUE_INT, &config.certificateLifespan},
	{"request.maxbody", VALUE_INT, &config.maximumBodyLength},
	{"request.maxdisplay", VALUE_INT, &config.maximumDisplayLength},
	{"request.maxredirects", VALUE_INT, &config.maximumRedirects},
	{"request.cachedpages", VALUE_INT, &config.maximumCachedPages},
	{"search.url", VALUE_STRING, &config.searchEngineURL},
};

void config_default() {
	memset(&config, 0, sizeof(config));
	config.certificateLifespan = 3600 * 24 * 365 * 2; /* 2 years */
	config.certificateBits = 2048;
	config.enableHexViewer = 1;
	config.enableSandbox = 1;
	config.enableImage = 1;
	config.enableHistory = 1;
	config.maximumBodyLength = 8388608;
	config.maximumDisplayLength = 1048576;
	config.imageParserScratchPad = 16777216;
	config.maximumRedirects = 5;
	config.maximumCachedPages= 15;
	STRLCPY(config.searchEngineURL, DEFAULT_SEARCH_URL);
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

int config_load() {
	FILE *f;
	struct field field;
	char buf[1024];
	int i, in_str, in_comment;
	config_default();
	if (!(f = storage_fopen(CONFIG_FILE, "r")))
		return ERROR_STORAGE_ACCESS;

	in_str = i = 0;
	while (1) {
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
	return 0;
}

int config_save() {

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
