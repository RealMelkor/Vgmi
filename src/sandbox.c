/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@rmf-dev.com>
 */
#include "sandbox.h"

#ifdef NO_SANDBOX
int sandbox_init(void) {
	return 0;
}

int sandbox_isolate(void) {
	return 0;
}

int sandbox_set_name(const char *ptr) {
	if (ptr) return !ptr;
	return 0;
}
#endif
