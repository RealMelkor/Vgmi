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
#include <fcntl.h>
#ifdef __linux__
#include <linux/limits.h>
#endif
#include <errno.h>
#include "error.h"

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

#define CONFIG_FOLDER "vgmi2"
#define CONFIG_PATH "%s/.config/"CONFIG_FOLDER

int storage_fd = -1;

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

int storage_path(char *out, size_t length) {

	const char *path;
	struct passwd *pw;

	if (length > PATH_MAX) length = PATH_MAX;

	path = getenv("XDG_CONFIG_HOME");
	if (path) {
		int i = strlcpy(out, path, length);
		strlcpy(&out[i], CONFIG_FOLDER, length - i);
		return 0;
	}

	path = getenv("HOME");
	if (path) {
		snprintf(out, length, CONFIG_PATH, path);
		return 0;
	}

	pw = getpwuid(getuid());
	if (pw) {
		snprintf(out, length, CONFIG_PATH, pw->pw_dir);
		return 0;
	}

	return -1;
}

FILE* storage_fopen(const char *name, const char *mode) {

	int fd;
	int flags;

	if (strchr(mode, 'w')) flags = O_WRONLY | O_TRUNC;
	else if (strchr(mode, 'r')) flags = O_RDONLY;
	else if (strchr(mode, 'a')) flags = O_WRONLY | O_APPEND;
	else return NULL;

	fd = openat(storage_fd, name, flags);
	if (fd < 0) return NULL;

	return fdopen(fd, mode);
}

int storage_open(const char *name, int flags, int mode) {
	return openat(storage_fd, name, flags, mode);
}

int storage_read(const char *name, char *out, size_t length,
			size_t *length_out) {
	int fd;
	size_t file_length;

	fd = openat(storage_fd, name, O_RDONLY, 0);
	if (fd < 0) return ERROR_STORAGE_ACCESS;

	file_length = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);

	if (length < file_length) return ERROR_BUFFER_OVERFLOW;

	if (read(fd, out, file_length) != (ssize_t)file_length)
		return ERROR_STORAGE_ACCESS;
	*length_out = file_length;

	close(fd);
	return 0;
}

int storage_init() {

	char path[PATH_MAX];
	int ret;

	if (storage_path(V(path))) return ERROR_STORAGE_ACCESS;
	if (storage_mkdir(path)) return ERROR_STORAGE_ACCESS;
	if ((ret = open(path, O_DIRECTORY)) < 0) return ERROR_STORAGE_ACCESS;
	storage_fd = ret;
	return 0;
}
