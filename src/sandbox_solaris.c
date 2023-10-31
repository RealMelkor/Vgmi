/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#ifdef sun
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <priv.h>
#include "error.h"

int init_privs(const char **privs) {
	priv_set_t *pset;
	if ((pset = priv_allocset()) == NULL) return -1;
	priv_emptyset(pset);
	for (int i = 0; privs[i]; i++) {
		if (priv_addset(pset, privs[i])) return -1;
	}
	if (setppriv(PRIV_SET, PRIV_PERMITTED, pset) ||
			setppriv(PRIV_SET, PRIV_LIMIT, pset) ||
			setppriv(PRIV_SET, PRIV_INHERITABLE, pset)) {
		return -1;
	}
	priv_freeset(pset);
	return 0;
}

int sandbox_init() {
	const char* privs[] = {PRIV_NET_ACCESS, PRIV_FILE_READ,
				PRIV_FILE_WRITE, NULL};
	if (init_privs(privs)) return ERROR_SANDBOX_FAILURE;
	return 0;
}

int sandbox_isolate() {
	const char* privs[] = {NULL};
	if (init_privs(privs)) return ERROR_SANDBOX_FAILURE;
	return 0;
}

int sandbox_set_name(const char *name) {
	if (name) return !name;
	return 0;
}
#else
typedef int hide_warning;
#endif
