#include "sandbox.h"

#ifdef NO_SANDBOX
int sandbox_init() {
	return 0;
}

int sandbox_isolate() {
	return 0;
}

int sandbox_set_name(const char *ptr) {
	if (ptr) return !ptr;
	return 0;
}
#endif
