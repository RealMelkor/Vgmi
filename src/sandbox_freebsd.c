#ifdef __FreeBSD__
#include <sys/capsicum.h>
#include <stdlib.h>
#include <unistd.h>
#include "sandbox.h"
#include "error.h"

int sandbox_init() {
	return 0;
}

int sandbox_isolate() {
	if (cap_enter()) return ERROR_MEMORY_FAILURE;
	return 0;
}

int sandbox_set_name(const char *name) {
	setproctitle("%s", name);
	return 0;
}
#else
typedef int hide_warning;
#endif
