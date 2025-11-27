/*
 * ISC License
 * Copyright (c) 2025 RMF <rawmonk@rmf-dev.com>
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
#include <dirent.h>
#ifdef __linux__
#include <linux/limits.h>
#endif
#include "macro.h"
#include "strscpy.h"
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

	i = strscpy(path_copy, path, sizeof(path_copy) - 1);
	path_copy[i] = '/';
	path_copy[i + 1] = 0;
	for (ptr = path_copy; ptr && *ptr; ptr = strchr(ptr, '/')) {
		char buf[PATH_MAX];
		if (ptr++ == path_copy) continue;
		if ((size_t)(ptr - path_copy) >= sizeof(buf)) return -1;
		strscpy(buf, path_copy, ptr - path_copy + 1);
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
                unsigned int i = strscpy(out, path, length);
                if (i + 1 >= length) return -1; /* path too long */
                if (out[i - 1] != '/') { /* check if path ends with a slash */
                        out[i++] = '/';
                        out[i] = '\0';
                }
                strscpy(&out[i], CONFIG_FOLDER, length - i);
                return 0;
        }

        return storage_from_home(out, length, CONFIG_PATH);
}

int storage_download_path(char *out, size_t length) {

	FILE *f;
	char path[CONFIG_STRING_LENGTH];

	if (length > PATH_MAX) length = PATH_MAX;

	config_get_str(config.downloadsPath, path);
	if (*path == '/') {
		strscpy(out, path, length);
		return 0;
	}

	f = popen("xdg-user-dir DOWNLOAD 2>&1", "r");
	if (f) {
		size_t bytes = fread(out, 1, length, f);
		pclose(f);
		if (bytes < 1) {
			out[0] = 0;
			return -1;
		}
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
	FILE *f;

	if (strchr(mode, 'w'))
		flags = O_WRONLY | O_TRUNC | O_CREAT | O_CLOEXEC;
	else if (strchr(mode, 'r'))
		flags = O_RDONLY | O_CLOEXEC;
	else if (strchr(mode, 'a'))
		flags = O_WRONLY | O_APPEND | O_CREAT | O_CLOEXEC;
	else return NULL;

	fd = openat(dir, name, flags, secure ? 0600 : 0644);
	if (fd < 0) return NULL;

	f = fdopen(fd, mode);
	if (!f) close(fd);

	return f;
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
	off_t file_length;

	fd = openat(storage_fd, name, O_RDONLY, 0);
	if (fd < 0) return ERROR_STORAGE_ACCESS;

	file_length = lseek(fd, 0, SEEK_END);
	if (file_length == -1 || lseek(fd, 0, SEEK_SET) == -1) {
		close(fd);
		return ERROR_ERRNO;
	}

	if (length < (size_t)file_length) {
		close(fd);
		return ERROR_BUFFER_OVERFLOW;
	}

	if (read(fd, out, file_length) != file_length) {
		close(fd);
		return ERROR_STORAGE_ACCESS;
	}
	*length_out = file_length;

	close(fd);
	return 0;
}

static int storage_init_path(int *fd, int download) {
	char path[PATH_MAX];
	int ret;
	if (download) {
		if (storage_path(V(path))) return ERROR_STORAGE_ACCESS;
	} else if (storage_download_path(V(path))) return ERROR_STORAGE_ACCESS;
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

int storage_find(const char* executable, char *path, size_t length) {
	struct stat st;
	const char *ptr;
	if (*executable == '/' && stat(executable, &st) == 0) {
		strscpy(path, executable, length);
		return 0;
	}
	for (ptr = getenv("PATH"); ptr && *ptr; ptr = strchr(ptr, ':')) {
		char buf[PATH_MAX], *buf_ptr;
		if (*ptr == ':') ptr++;
		STRSCPY(buf, ptr);
		buf_ptr = strchr(buf, ':');
		if (!buf_ptr) buf_ptr = buf + strnlen(buf, sizeof(buf) - 2);
		*(buf_ptr++) = '/';
		strscpy(buf_ptr, executable, sizeof(buf) - (buf_ptr - buf));
		if (stat(buf, &st) == 0) {
			strscpy(path, buf, length);
			return 0;
		}
	}
	return -1;
}

int storage_clear_tmp(void) {
	DIR *dir;
	int ret, fd;
	struct dirent *entry;

	if (download_fd < 0 && (ret = storage_init_download()))
		return ret;
	if ((fd = dup(download_fd)) == -1) return ERROR_ERRNO;
	if (!(dir = fdopendir(fd))) {
		close(fd);
		return ERROR_ERRNO;
	}
	rewinddir(dir);
	while ((entry = readdir(dir))) {
		if (!memcmp(entry->d_name, V(".tmp_vgmi_exec_") - 1))
			unlinkat(download_fd, entry->d_name, 0);
	}
	closedir(dir);
	return 0;
}
