/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@rmf-dev.com>
 */
#if defined (__OpenBSD__) && !defined (DISABLE_SANDBOX)
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "macro.h"
#include "sandbox.h"
#include "storage.h"
#include "error.h"

int sandbox_init(void) {

	char path[2048], download_path[2048];
	int ret;

	if ((ret = storage_path(V(path)))) return ret;
	if ((ret = storage_download_path(V(download_path)))) return ret;
	if (unveil("/etc/resolv.conf", "r")) return ERROR_SANDBOX_FAILURE;
	if (unveil("/etc/hosts", "r")) return ERROR_SANDBOX_FAILURE;
	if (unveil(path, "rwc")) return ERROR_SANDBOX_FAILURE;
	if (unveil(download_path, "rwc")) return ERROR_SANDBOX_FAILURE;
	if (unveil(NULL, NULL)) return ERROR_SANDBOX_FAILURE;

	if (pledge("tty stdio inet dns rpath wpath cpath", ""))
		return ERROR_SANDBOX_FAILURE;

	return 0;
}

int sandbox_isolate(void) {
	if (unveil(NULL, NULL)) return ERROR_SANDBOX_FAILURE;
	if (pledge("stdio", "")) return ERROR_SANDBOX_FAILURE;
	return 0;
}

int sandbox_set_name(const char *name) {
	setproctitle("%s", name);
	return 0;
}
#else
typedef int hide_warning;
#endif
