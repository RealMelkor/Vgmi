/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#if __has_include(<stb_image.h>)
#include <stdlib.h>

#define STATIC_ALLOC

#ifdef STATIC_ALLOC
size_t img_memory_length;
char *img_memory;
char *img_memory_ptr;
char *img_memory_prev;

void *img_malloc(size_t size) {
        void *ptr = img_memory_ptr;
        if (ptr + size > (void*)img_memory + img_memory_length)
                return NULL;
        img_memory_prev = ptr;
        img_memory_ptr += size;
        return ptr;
}

void *img_realloc(void *ptr, size_t size) {
	char *_ptr = ptr;
	if (!ptr) return img_malloc(size);
	if (_ptr < img_memory || _ptr > img_memory + img_memory_length)
		return NULL;
        if (ptr == img_memory_prev) {
                img_memory_ptr = ptr + size;
                return ptr;
        }
        return img_malloc(size);
}

void img_free(void *ptr) {
        if (ptr == img_memory_prev) {
                img_memory_ptr = img_memory_prev;
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
	img_memory_prev = NULL;
#endif
	return stbi_load_from_memory(data, len, x, y, &comp, 3);
}
#else
typedef int hide_warning;
#endif
