/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include "macro.h"
#include "strlcpy.h"
#include "error.h"
#include "storage.h"
#define ABOUT_INTERNAL
#include "about.h"

int about_certificates_param(const char *param) {

	char path[PATH_MAX];
	size_t len;
	FILE *f;

	len = STRLCPY(path, param);
	if (len >= sizeof(path)) return ERROR_INVALID_ARGUMENT;
	strlcpy(&path[len], ".crt", sizeof(path) - len);
	if (!(f = storage_fopen(path, "r"))) return ERROR_INVALID_ARGUMENT;
	fclose(f);
	strlcpy(&path[len], ".key", sizeof(path) - len);
	if (!(f = storage_fopen(path, "r"))) return ERROR_INVALID_ARGUMENT;
	fclose(f);

	unlinkat(storage_fd, path, 0);
	strlcpy(&path[len], ".crt", sizeof(path) - len);
	unlinkat(storage_fd, path, 0);
	return 0;
}

int about_certificates(char **out, size_t *length_out) {

	char *data = NULL;
	size_t length = 0;
	const char title[] = "# Client certificates\n\n";
	DIR *dir;
	struct dirent *entry;

	if (!(dir = fdopendir(dup(storage_fd)))) return ERROR_ERRNO;
	rewinddir(dir);

	if (!(data = dyn_strcat(NULL, &length, V(header)))) goto fail;
	if (!(data = dyn_strcat(data, &length, V(title)))) goto fail;

	while ((entry = readdir(dir))) {

		int len;
		FILE *f;
		char buf[2048], host[1024];
		char *end = host + STRLCPY(host, entry->d_name) - 4;

		if (end <= host || !memcmp(end, V(".crt"))) continue;
		memcpy(end, V(".key"));
		f = storage_fopen(host, "r");
		if (!f) continue;
		fclose(f);
		*end = '\0';
		len = snprintf(V(buf), "* %s\n=>%s Delete\n\n", host, host);
		if (!(data = dyn_strcat(data, &length, buf, len))) goto fail;
        }

	closedir(dir);

	*out = data;
	*length_out = length;
	return 0;
fail:
	closedir(dir);
	free(data);
	return ERROR_MEMORY_FAILURE;
}
