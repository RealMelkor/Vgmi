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
#include "config.h"

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

#define CONFIG_FOLDER "vgmi"
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

	return ERROR_STORAGE_ACCESS;
}

int storage_path(char *out, size_t length) {

	const char *path;

        if (length > PATH_MAX) length = PATH_MAX;

        path = getenv("XDG_CONFIG_HOME");
        if (path && *path) {
                unsigned int i = strlcpy(out, path, length);
                if (i + 1 >= length) return -1; /* path too long */
                if (out[i - 1] != '/') { /* check if path ends with a slash */
                        out[i++] = '/';
                        out[i] = '\0';
                }
                strlcpy(&out[i], CONFIG_FOLDER, length - i);
                return 0;
        }

        return storage_from_home(out, length, CONFIG_PATH);
}

int storage_download_path(char *out, size_t length) {

	FILE *f;

	if (length > PATH_MAX) length = PATH_MAX;

	if (*config.downloadsPath == '/') {
		strlcpy(out, config.downloadsPath, length);
		return 0;
	} else *config.downloadsPath = '\0';

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

static FILE* _fopen(const char *name, const char *mode, int dir, int secure) {

	int fd;
	int flags;

	if (strchr(mode, 'w'))
		flags = O_WRONLY | O_TRUNC | O_CREAT | O_CLOEXEC;
	else if (strchr(mode, 'r'))
		flags = O_RDONLY | O_CLOEXEC;
	else if (strchr(mode, 'a'))
		flags = O_WRONLY | O_APPEND | O_CREAT | O_CLOEXEC;
	else return NULL;

	fd = openat(dir, name, flags, secure ? 0600 : 0644);
	if (fd < 0) return NULL;

	return fdopen(fd, mode);
}

FILE* storage_fopen(const char *name, const char *mode) {
	return _fopen(name, mode, storage_fd, 1);
}

FILE* storage_download_fopen(const char *name, const char *mode) {
	return _fopen(name, mode, download_fd, 0);
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

int storage_init_path(int *fd, int download) {
	char path[PATH_MAX];
	int ret;
	ret = download ? storage_path(V(path)) : storage_download_path(V(path));
	if ((ret = storage_mkdir(path))) return ret;
	if ((*fd = open(path, O_DIRECTORY | O_CLOEXEC)) < 0)
		return ERROR_STORAGE_ACCESS;
	return 0;
}

int storage_init(void) {
	return storage_init_path(&storage_fd, 1);
}

int storage_init_download(void) {
	return storage_init_path(&download_fd, 0);
}

int storage_close(void) {
	close(storage_fd);
	close(download_fd);
	return 0;
}
