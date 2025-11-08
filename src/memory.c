/*
 * ISC License
 * Copyright (c) 2025 RMF <rawmonk@rmf-dev.com>
 */
#include <sys/mman.h>
#include <string.h>
#include "memory.h"
#include "sandbox.h"
#include "error.h"

#if defined(__linux__) && defined(ENABLE_PKEY)
#define USE_PKEY
#endif

#ifdef USE_PKEY
int pkey = -1;
#endif

int readonly(const char *in, size_t length, char **out) {
	void *ptr = mmap(NULL, length, PROT_READ|PROT_WRITE,
			MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
	if (ptr == MAP_FAILED) return ERROR_MEMORY_FAILURE;
	memcpy(ptr, in, length);
#ifdef USE_PKEY
	if (pkey == -1) {
		pkey = pkey_alloc(0, PKEY_DISABLE_WRITE);
		if (pkey == -1) goto fail;
	}
	if (pkey_mprotect(ptr, length, PROT_READ, pkey)) goto fail;
#elif __OpenBSD__
	if (mimmutable(ptr, length)) goto fail;
#else
        if (mprotect(ptr, length, PROT_READ)) goto fail;
#endif
	*out = ptr;
	return 0;
fail:
	munmap(ptr, length);
	return ERROR_ERRNO;
}

int free_readonly(void *ptr, size_t length) {
	return munmap(ptr, length);
}
