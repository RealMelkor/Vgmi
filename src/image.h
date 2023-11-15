/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#if __has_include(<stb_image.h>)
#define ENABLE_IMAGE
#endif

#ifdef ENABLE_IMAGE
int image_init();
int image_display(unsigned char* data, int w, int h, int offsety);
void image_memory_set(void *memory, size_t len);
void *image_load(void *data, int len, int *x, int *y);
#endif
