/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#ifndef DISABLE_XDG
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "macro.h"
#include "strnstr.h"
#include "proc.h"
#include "sandbox.h"
#include "error.h"
#include "config.h"
#include "storage.h"

char launcher_path[PATH_MAX];
const char download_prefix[] = "download://";

const char *allowed_protocols[] = {
	"http://",
	"https://",
	"gopher://",
	"mailto:",
	download_prefix	/* launched downloaded files */
};

int has_xdg = -1;
int xdg_available(void) {
	if (has_xdg == -1) {
		char buf[1024];
		has_xdg = !storage_find(config.launcher, V(buf));
	}
	return has_xdg;
}

static int open_download(const char *input) {

	char buf[PATH_MAX], cmd[PATH_MAX + 128], download_dir[PATH_MAX];
	const char *ptr;
	int ret;
	size_t i;

	if ((ret = storage_download_path(V(download_dir)))) return ret;

	ptr = input + sizeof(download_prefix) - 1;
	for (i = 0; i < sizeof(buf) && *ptr; i++) {
		buf[i] = *(ptr++);
		if (buf[i] == '/' || buf[i] == '\\')
			return ERROR_INVALID_ARGUMENT;
	}
	if (!STRCMP(buf, "..") || !STRCMP(buf, "."))
		return ERROR_INVALID_ARGUMENT;

	snprintf(V(cmd), "\"%s\" %s/\"%s\" >/dev/null 2>&1",
			config.launcher, download_dir, buf);
	system(cmd);
	return 0;
}

int xdg_exec(char *line, size_t len) {
	char cmd[MAX_URL + 128];
	size_t i;
	for (i = 0; i < LENGTH(allowed_protocols); i++) {
		if (strnstr(line, allowed_protocols[i], len) == line)
			break;
	}
	if (i >= LENGTH(allowed_protocols)) return -1;
	if (!strcmp(allowed_protocols[i], "download://")) {
		return open_download(line);
	}
	snprintf(V(cmd), "\"%s\" %s >/dev/null 2>&1", config.launcher, line);
	system(cmd);
	return 0;
}

int xdg_proc(int in, int out) {

	char buf[MAX_URL];
	size_t i;
	unsigned char byte;

	if (storage_init()) return -1;
	if (config_load()) return -1;

	sandbox_set_name("vgmi [xdg]");
	i = byte = 0;
	write(out, P(byte));
	while (1) {
		if (i >= sizeof(buf)) i = 0;
		if (read(in, P(byte)) != 1) break;
		if (byte != '\n') {
			buf[i++] = byte;
			continue;
		}
		buf[i] = 0;
		byte = xdg_exec(V(buf));
		write(out, P(byte));
		i = 0;
	}
	return 0;
}

int xdg_in = -1, xdg_out = -1;
int xdg_request(const char *request) {
	unsigned char byte;
	write(xdg_out, request, strlen(request));
	byte = '\n';
	write(xdg_out, P(byte));
	if (read(xdg_in, &byte, 1) != 1 || byte) return ERROR_XDG;
	return 0;
}

int xdg_init(void) {
	if (!config.enableXdg) {
		has_xdg = 0;
		return 0;
	}
	return proc_fork("--xdg", &xdg_in, &xdg_out);
}
#else
typedef int hide_warning;
#endif
