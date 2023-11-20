/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <fcntl.h>
#ifdef __linux__
#include <linux/limits.h>
#endif
#include <errno.h>
#include "macro.h"
#include "strlcpy.h"
#include "storage.h"
#include "error.h"
#include "utf8.h"

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

#define CONFIG_FOLDER "vgmi2"
#define DOWNLOAD_PATH "Downloads"
#define CONFIG_PATH ".config/"CONFIG_FOLDER

int storage_fd = -1, download_fd = -1;

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

static int storage_from_home(char *out, size_t length, const char *dir) {

	struct passwd *pw;
	const char *path = getenv("HOME");

	if (path) {
		snprintf(out, length, "%s/%s", path, dir);
		return 0;
	}

	pw = getpwuid(getuid());
	if (pw) {
		snprintf(out, length, "%s/%s", pw->pw_dir, dir);
		return 0;
	}

	return -1;
}

int storage_path(char *out, size_t length) {

	const char *path;

	if (length > PATH_MAX) length = PATH_MAX;

	path = getenv("XDG_CONFIG_HOME");
	if (path) {
		int i = strlcpy(out, path, length);
		strlcpy(&out[i], CONFIG_FOLDER, length - i);
		return 0;
	}

	return storage_from_home(out, length, CONFIG_PATH);
}

int storage_download_path(char *out, size_t length) {

	FILE *f;

	if (length > PATH_MAX) length = PATH_MAX;

	f = popen("xdg-user-dir DOWNLOAD 2>&1", "r");
	if (f) {
		fread(out, 1, length, f);
		pclose(f);
		if (out[0] == '/') { /* success if the output is a path */
			size_t i = 0;
			while (i < length) {
				uint32_t ch;
				int len = utf8_char_to_unicode(&ch, &out[i]);
				if (ch < ' ') {
					out[i] = 0;
					break;
				}
				i += len;
			}
			return 0;
		}
	}

	return storage_from_home(out, length, DOWNLOAD_PATH);
}

static FILE* _fopen(const char *name, const char *mode, int dir) {

	int fd;
	int flags;

	if (strchr(mode, 'w')) flags = O_WRONLY | O_TRUNC | O_CREAT;
	else if (strchr(mode, 'r')) flags = O_RDONLY;
	else if (strchr(mode, 'a')) flags = O_WRONLY | O_APPEND | O_CREAT;
	else return NULL;

	fd = openat(dir, name, flags, 0600);
	if (fd < 0) return NULL;

	return fdopen(fd, mode);
}

FILE* storage_fopen(const char *name, const char *mode) {
	return _fopen(name, mode, storage_fd);
}

FILE* storage_download_fopen(const char *name, const char *mode) {
	return _fopen(name, mode, download_fd);
}

int storage_open(const char *name, int flags, int mode) {
	return openat(storage_fd, name, flags, mode);
}

int storage_download_open(const char *name, int flags, int mode) {
	return openat(download_fd, name, flags, mode);
}

int storage_download_exist(const char *name) {
	struct stat stat;
	return !fstatat(download_fd, name, &stat, 0);
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

	char path[PATH_MAX], download[PATH_MAX];
	int ret;

	if (storage_path(V(path))) return ERROR_STORAGE_ACCESS;
	if (storage_download_path(V(download))) return ERROR_STORAGE_ACCESS;
	if (storage_mkdir(path)) return ERROR_STORAGE_ACCESS;
	if (storage_mkdir(download)) return ERROR_STORAGE_ACCESS;
	if ((ret = open(path, O_DIRECTORY)) < 0) return ERROR_STORAGE_ACCESS;
	storage_fd = ret;
	if ((ret = open(download, O_DIRECTORY)) < 0)
		return ERROR_STORAGE_ACCESS;
	download_fd = ret;
	return 0;
}
