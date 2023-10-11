/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "macro.h"
#include "strlcpy.h"
#include "storage.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#ifdef __linux__
#include <linux/limits.h>
#endif
#include <errno.h>

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

#define CONFIG_FOLDER "vgmi2"
#define CONFIG_PATH "%s/.config/"CONFIG_FOLDER

static int storage_mkdir(const char *path) {

	struct stat st;
	const char *ptr;
	char path_copy[PATH_MAX];
	int i;

	if (stat(path, &st) == 0) return 0;

	i = STRLCPY(path_copy, path);
	path_copy[i] = '/';
	path_copy[i + 1] = 0;
	for (ptr = path_copy; ptr && *ptr; ptr = strchr(ptr, '/')) {
		char buf[PATH_MAX];
		if (ptr++ == path_copy) continue;
		if ((size_t)(ptr - path_copy) >= sizeof(buf)) return -1;
		strlcpy(buf, path_copy, ptr - path_copy + 1);
		if (stat(buf, &st) == 0) continue;
		if (mkdir(buf, 0700)) return -1;
	}

	return 0;
}

char storage_path_cached[PATH_MAX] = {0};
static int storage_path(char *out, size_t length) {

	const char *path;
	struct passwd *pw;

	if (*storage_path_cached) {
		strlcpy(out, storage_path_cached, length);
		return 0;
	}

	if (length > PATH_MAX) length = PATH_MAX;

	path = getenv("XDG_CONFIG_HOME");
	if (path) {
		int i = strlcpy(out, path, length);
		strlcpy(&out[i], CONFIG_FOLDER, length - i);
		STRLCPY(storage_path_cached, out);
		return 0;
	}

	path = getenv("HOME");
	if (path) {
		snprintf(out, length, CONFIG_PATH, path);
		STRLCPY(storage_path_cached, out);
		return 0;
	}

	pw = getpwuid(getuid());
	if (pw) {
		snprintf(out, length, CONFIG_PATH, pw->pw_dir);
		STRLCPY(storage_path_cached, out);
		return 0;
	}

	return -1;
}

FILE* storage_open(const char *name, const char *mode) {

	char path[PATH_MAX];
	int i;

	if (storage_path(V(path))) return NULL;
	if (storage_mkdir(path)) return NULL;

	i = strnlen(V(path));
	path[i++] = '/';
	strlcpy(&path[i], name, sizeof(path) - i);

	return fopen(path, mode);
}

int storage_init() {
	char path[PATH_MAX];
	int ret;
	if ((ret = storage_path(V(path)))) return ret;
	return storage_mkdir(path);
}
