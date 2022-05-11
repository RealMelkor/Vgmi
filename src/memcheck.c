/* See LICENSE file for copyright and license details. */
#ifdef MEM_CHECK
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
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
struct __allocation {
	void* ptr;
	int line;
	size_t size;
	const char* file;
	const char* func;
};

struct __allocation* __allocation;
uint64_t __allocationCount;
FILE* __output = NULL;

void __init()
{
	allocation=NULL;
	allocationCount=0;
	output = fopen(PATH,"wb");
}

void __free(void* ptr, const char* file, int line, const char* func)
{
	uint32_t i=0;
	if(ptr) {
		while(i!=allocationCount && ptr!=allocation[i].ptr) i++;
		if(i==allocationCount) return;
		else {
			for(uint32_t j=i; j!=allocationCount-1; j++)
				allocation[j] = allocation[j+1];
			allocationCount--;
			allocation = realloc(allocation, 
					     sizeof(struct __allocation) * allocationCount);
		}
	}
	if(i>allocationCount)
		fprintf(output, "Error free : %p | %s, Line %d : %s\n",
			ptr, file, line, func);
	else
		fprintf(output, "Free : %p | %s, Line %d : %s\n",
			ptr, file, line, func);
	fflush(output);
	free(ptr);
}

void* __malloc(size_t size, const char* file, int line, const char* func)
{
	void* ptr = malloc(size);
	if(ptr)	{
		allocationCount++;
		allocation=realloc(allocation,sizeof(struct __allocation)*allocationCount);
		allocation[allocationCount-1].ptr=ptr;
		allocation[allocationCount-1].line=line;
		allocation[allocationCount-1].size=size;
		allocation[allocationCount-1].file = file;
		allocation[allocationCount-1].func = func;
	}
	fprintf(output, "malloc : %p, size : %ld | %s, Line %d : %s\n",
		ptr, size, file, line, func);
	fflush(output);
	return ptr;
}

void* __calloc(size_t num, size_t size, const char* file, int line, const char* func)
{
    void* ptr = calloc(num, size);
    if(ptr) {
        allocationCount++;
        allocation=realloc(allocation,sizeof(struct __allocation)*allocationCount);
        allocation[allocationCount-1].ptr=ptr;
        allocation[allocationCount-1].line=line;
        allocation[allocationCount-1].size=size;
        allocation[allocationCount-1].file = file;
        allocation[allocationCount-1].func = func;
    }
    fprintf(output, "calloc : %p, size : %ld | %s, Line %d : %s\n",
	    ptr, size, file, line, func);
    fflush(output);
    return ptr;
}

void* __realloc(void* ptr, size_t size, const char* file, int line, const char* func)
{
	if(ptr==NULL) {
		fprintf(output, "(WARNING NULL REALLOC) realloc : %p, " \
				"size : %ld | %s, Line %d : %s\n",
			ptr, size, file, line, func);
		fclose(output);
		exit(0);
	}
	void* _ptr = realloc(ptr, size);
	if(_ptr != ptr) {
		uint32_t i=0;
		for(i=0; i!=allocationCount && ptr != allocation[i].ptr; i++) ;
		if(allocationCount>i) {
			allocation[i].ptr=_ptr;
			allocation[i].line=line;
			allocation[i].size=size;
			allocation[i].file = file;
			allocation[i].func = func;
		}
	}
	fprintf(output, "realloc : %p, size : %ld | %s, Line %d : %s\n",
		_ptr, size, file, line, func);
	fflush(output);
	return _ptr;
}

void __check()
{
	fprintf(output, "-----------------------\n");
	if (allocationCount == 0)
		fprintf(output, "No memory leak detected\n");
	else {
		fprintf(output, "%ld memory leaks detected\n", allocationCount);
		printf("WARNING: Memory leaks detected (%ld)\n", allocationCount);
	}
	for(uint32_t i=0; i!=allocationCount; i++)
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
