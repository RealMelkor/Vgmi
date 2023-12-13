/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#if __has_include(<stb_image.h>)
#include <stdlib.h>
#include <string.h>

#ifndef NO_SANDBOX
#ifdef __linux__
#define STATIC_ALLOC
#endif
#endif

#ifdef STATIC_ALLOC
size_t img_memory_length;
char *img_memory;
char *img_memory_ptr;
int allocations_count = 0;
struct allocation {
	void *ptr;
	size_t length;
	unsigned taken:1;
};
struct allocation allocations[2048] = {0};
#define LENGTH(X) (sizeof(X) / sizeof(*X))

void *img_malloc(size_t size) {
        void *ptr = img_memory_ptr;
	int i;
	for (i = 0; i < allocations_count; i++) {
		if (allocations[i].taken) continue;
		if (allocations[i].length > size) {
			return allocations[i].ptr;
		}
	}
	if (allocations_count >= LENGTH(allocations))
		return NULL;
        if (ptr + size > (void*)img_memory + img_memory_length)
                return NULL;
	allocations[allocations_count].ptr = ptr;
	allocations[allocations_count].length = size;
	allocations[allocations_count].taken = 1;
	allocations_count++;
        img_memory_ptr += size;
        return ptr;
}

void *img_realloc(void *ptr, size_t size) {
	char *prev, *new;
	int i;
	if (!ptr) return img_malloc(size);
	prev = img_memory;
	for (i = 0; i < LENGTH(allocations); i++) {
		if (allocations[i].ptr == ptr) break;
	}
	if (allocations[i].length >= size) {
		allocations[i].length = size;
		return prev;
	}
	new = img_malloc(size);
	memcpy(new, allocations[i].ptr, allocations[i].length);
        return new;
}

void img_free(void *ptr) {
	int i;
	if (allocations_count < 1) return;
	if (allocations[allocations_count - 1].ptr == ptr) {
		int found = 0;
		allocations_count--;
		allocations[allocations_count].taken = 0;
		for (i = allocations_count - 1; i >= 0; i--) {
			if (allocations[i].taken) {
				allocations_count = i;
				img_memory_ptr = allocations[i].ptr;
				found = 1;
				break;
			}
		}
		if (!found) img_memory_ptr = img_memory;
		return;
	}
	for (i = allocations_count - 1; i >= 0; i--) {
		if (allocations[i].ptr == ptr) {
			allocations[i].taken = 0;
			break;
		}
	}
}

#define STBI_MALLOC img_malloc
#define STBI_REALLOC img_realloc
#define STBI_FREE img_free
#endif
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

void image_memory_set(void *memory, size_t len) {
#ifdef STATIC_ALLOC
	img_memory = memory;
	img_memory_length = len;
#else
	free(memory);
#endif
}

void *image_load(void *data, int len, int *x, int *y) {
	int comp;
#ifdef STATIC_ALLOC
	img_memory_ptr = img_memory;
	memset(allocations, 0, sizeof(allocations));
	allocations_count = 0;
#endif
	return stbi_load_from_memory(data, len, x, y, &comp, 3);
}
#else
typedef int hide_warning;
#endif
