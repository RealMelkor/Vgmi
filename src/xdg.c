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

const char *allowed_protocols[] = {
	"http://",
	"https://",
	"gopher://",
	"mailto:",
};

int has_xdg = -1;
int xdg_available() {
	if (has_xdg == -1)
		has_xdg = !system("xdg-open --help >/dev/null 2>&1");
	return has_xdg;
}

int xdg_exec(char *line, size_t len) {
	char cmd[MAX_URL + 128];
	size_t i;
	for (i = 0; i < LENGTH(allowed_protocols); i++) {
		if (strnstr(line, allowed_protocols[i], len) == line)
			break;
	}
	if (i >= LENGTH(allowed_protocols)) return -1;
	snprintf(V(cmd), "xdg-open %s >/dev/null 2>&1", line);
	system(cmd);
	return 0;
}

int xdg_proc(int in, int out) {

	char buf[MAX_URL];
	size_t i;
	unsigned char byte;

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
	read(xdg_in, &byte, 1);
	if (byte) return ERROR_XDG;
	return 0;
}

int xdg_init() {
	if (!config.enableXdg) {
		has_xdg = 0;
		return 0;
	}
	return proc_fork("--xdg", &xdg_in, &xdg_out);
}
#else
typedef int hide_warning;
#endif
