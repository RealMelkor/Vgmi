/*
 * ISC License
 * Copyright (c) 2025 RMF <rawmonk@rmf-dev.com>
 */
#ifndef DISABLE_XDG
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include "macro.h"
#include "strnstr.h"
#include "proc.h"
#include "sandbox.h"
#include "error.h"
#include "config.h"
#include "storage.h"
#define PARSER_INTERNAL
#include "parser.h"

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

static const char *redirect(void) {
	return config.launcherTerminal ? "" : ">/dev/null 2>&1";
}

static int open_download(const char *input) {

	char buf[PATH_MAX], cmd[PATH_MAX * 4], download_dir[PATH_MAX];
	const char *ptr;
	int ret;
	size_t i;

	if ((ret = storage_download_path(V(download_dir)))) return ret;

	ptr = input + sizeof(download_prefix) - 1;
	for (i = 0; i < sizeof(buf) - 1 && *ptr; i++) {
		buf[i] = *(ptr++);
		if (buf[i] == '/' || buf[i] == '\\')
			return ERROR_INVALID_ARGUMENT;
	}
	buf[i] = '\0';
	if (!STRCMP(buf, "..") || !STRCMP(buf, "."))
		return ERROR_INVALID_ARGUMENT;

	snprintf(V(cmd), "\"%s\" %s/\"%s\" %s",
			config.launcher, download_dir, buf, redirect());
	if (system(cmd) == -1) return ERROR_ERRNO;
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
	if (!strcmp(allowed_protocols[i], download_prefix)) {
		return open_download(line);
	}
	snprintf(V(cmd), "\"%s\" %s %s", config.launcher, line, redirect());
	if (system(cmd) == -1) return -1;
	return 0;
}

int xdg_proc(int in, int out) {

	char buf[MAX_URL];
	size_t i;
	unsigned char byte;

	if (storage_init()) return -1;
	if (storage_init_download()) return -1;
	config_load();

	sandbox_set_name("vgmi [xdg]");
	i = byte = 0;
	if (vwrite(out, P(byte))) return -1;
	while (1) {
		if (i >= sizeof(buf)) i = 0;
		if (read(in, P(byte)) != 1) break;
		if (byte != '\n') {
			buf[i++] = byte;
			continue;
		}
		buf[i] = 0;
		byte = xdg_exec(V(buf));
		if (vwrite(out, P(byte))) return -1;
		i = 0;
	}
	return 0;
}

int xdg_in = -1, xdg_out = -1;
int xdg_request(const char *request) {
	unsigned char byte;
	if (vwrite(xdg_out, (void*)request, strlen(request))) return ERROR_XDG;
	byte = '\n';
	if (vwrite(xdg_out, P(byte))) return ERROR_XDG;
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
