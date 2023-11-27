/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#include <sys/mman.h>
#include <string.h>
#include "memory.h"
#include "error.h"

int readonly(const char *in, size_t length, char **out) {
	void *ptr = mmap(NULL, length, PROT_READ|PROT_WRITE,
			MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
	if (!ptr) return ERROR_MEMORY_FAILURE;
	memcpy(ptr, in, length);
        if (mprotect(ptr, length, PROT_READ)) return ERROR_ERRNO;
	*out = ptr;
	return 0;
}

int free_readonly(void *ptr, size_t length) {
	munmap(ptr, length);
	return 0;
}
