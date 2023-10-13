#ifdef __OpenBSD__
#include <stdio.h>
#include <unistd.h>
#include "macro.h"
#include "sandbox.h"
#include "storage.h"
#include "error.h"

int sandbox_init() {

	char path[2048];
	int ret;

	if ((ret = storage_path(V(path)))) return ret;
	if (unveil("/etc/resolv.conf", "r")) return ERROR_SANDBOX_FAILURE;
	if (unveil("/etc/hosts", "r")) return ERROR_SANDBOX_FAILURE;
	if (unveil(path, "rwc")) return ERROR_SANDBOX_FAILURE;
	if (unveil(NULL, NULL)) return ERROR_SANDBOX_FAILURE;

	if (pledge("tty stdio inet dns rpath wpath cpath", ""))
		return ERROR_SANDBOX_FAILURE;

	return 0;
}
#else
typedef int hide_warning;
#endif
