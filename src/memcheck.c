/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#ifdef MEM_CHECK
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "termbox.h"
#define allocation (__allocation)
#define allocationCount (__allocationCount)
#define output (__output)
#define PATH "mem.log"

void __free(void*, const char*, int, const char*);
void* __malloc(size_t, const char*, int, const char*);
void* __calloc(size_t, size_t, const char*, int, const char*);
void* __realloc(void*, size_t, const char*, int, const char*);
void __init();
void __check();
int __main(int argc, char* argv[]);

struct __allocation {
	void* ptr;
	int line;
	size_t size;
	const char* file;
	const char* func;
};

int main(int argc, char* argv[]) {
	int ret;
        __init();
        ret = __main(argc, argv);
	__check();
	return ret;
}

struct __allocation* __allocation = NULL;
uint64_t __allocationCount = 0;
FILE* __output = NULL;
int __warnings = 0;

void __debug(const char* out) {
	fprintf(output, out);
	fflush(output);
}

void __init() {
	allocation = NULL;
	allocationCount = 0;
	output = fopen(PATH, "wb");
}

void __free(void* ptr, const char* file, int line, const char* func) {
	uint32_t i = 0, j;
	if (ptr) {
		while(i != allocationCount && ptr != allocation[i].ptr) i++;
		if (i == allocationCount) {
			fprintf(output,
				"(WARNING Freeing non-allocated memory) " \
				"free : %p, %s, Line %d : %s\n",
				ptr, file, line, func);
			fflush(output);
#ifndef EXIT_ON_ERROR
			__warnings++;
			return;
#else
			fclose(output);
			tb_shutdown();
			printf("mem_check detected a fatal error\n");
			return exit(0);
#endif
		}
		for(j = i; j != allocationCount - 1; j++)
			allocation[j] = allocation[j + 1];
		allocationCount--;
		allocation = realloc(allocation,
				     sizeof(struct __allocation) *
				     allocationCount);
	}
	if(i > allocationCount)
		fprintf(output, "Error free : %p | %s, Line %d : %s\n",
			ptr, file, line, func);
	else
		fprintf(output, "Free : %p | %s, Line %d : %s\n",
			ptr, file, line, func);
	fflush(output);
	free(ptr);
}

void* __malloc(size_t size, const char* file, int line, const char* func) {
	void* ptr = malloc(size);
	if(ptr)	{
		allocationCount++;
		allocation = realloc(allocation,
					sizeof(struct __allocation) *
					allocationCount);
		allocation[allocationCount - 1].ptr = ptr;
		allocation[allocationCount - 1].line = line;
		allocation[allocationCount - 1].size = size;
		allocation[allocationCount - 1].file = file;
		allocation[allocationCount - 1].func = func;
	}
	fprintf(output, "malloc : %p, size : %ld | %s, Line %d : %s\n",
		ptr, size, file, line, func);
	fflush(output);
	return ptr;
}

void* __calloc(size_t num, size_t size, const char* file,
	       int line, const char* func) {
	void* ptr = calloc(num, size);
	if(ptr) {
		allocationCount++;
		allocation = realloc(allocation,
				     sizeof(struct __allocation) *
				     allocationCount);
		allocation[allocationCount-1].ptr = ptr;
		allocation[allocationCount-1].line = line;
		allocation[allocationCount-1].size = size;
		allocation[allocationCount-1].file = file;
		allocation[allocationCount-1].func = func;
	}
	fprintf(output, "calloc : %p, size : %ld | %s, Line %d : %s\n",
	ptr, size, file, line, func);
	fflush(output);
	return ptr;
}

void* __realloc(void* ptr, size_t size, const char* file,
		int line, const char* func) {
	void* _ptr;
	if (ptr == NULL) return __malloc(size, file, line, func);
	_ptr = realloc(ptr, size);
	if(_ptr != ptr) {
		uint32_t i = 0;
		for(i = 0; i != allocationCount &&
				ptr != allocation[i].ptr; i++) ;
		if(allocationCount > i) {
			allocation[i].ptr = _ptr;
			allocation[i].line = line;
			allocation[i].size = size;
			allocation[i].file = file;
			allocation[i].func = func;
		}
	}
	fprintf(output, "realloc : %p, size : %ld | %s, Line %d : %s\n",
		_ptr, size, file, line, func);
	fflush(output);
	return _ptr;
}

void __check() {
	uint32_t i;
	fprintf(output, "-----------------------\n");
	if (allocationCount == 0)
		fprintf(output, "No memory leak detected\n");
	else {
		fprintf(output, "%ld memory leaks detected\n",
			allocationCount);
		printf("WARNING: Memory leaks detected (%ld)\n",
			allocationCount);
	}
	if (__warnings) {
		fprintf(output, "%d invalid memory operations detected\n",
			__warnings);
		printf("WARNING: %d invalid memory operations detected\n",
			__warnings);
	}
	for(i = 0; i != allocationCount; i++)
    		fprintf(output, "Leak : %p, size : %ld | %s, Line %d : %s\n",
			allocation[i].ptr,
			allocation[i].size,
			allocation[i].file,
			allocation[i].line,
			allocation[i].func);
	fclose(output);
}
#else
typedef int remove_iso_warning;
#endif
