/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#include <sys/mman.h>
#include <string.h>

int readonly(const char *in, size_t length, char **out) {
	void *ptr = mmap(NULL, length, PROT_READ|PROT_WRITE,
			MAP_ANON|MAP_PRIVATE, -1, 0);
	memcpy(ptr, in, length);
        if (mprotect(ptr, length, PROT_READ)) return -1;
	*out = ptr;
	return 0;
}

int free_readonly(void *ptr, size_t length) {
	munmap(ptr, length);
	return 0;
}
