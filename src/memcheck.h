/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#ifdef MEM_CHECK
#ifndef _MEMCHECK_H_
#define _MEMCHECK_H_

void __free(void*, const char*, int, const char*);
void* __malloc(size_t, const char*, int, const char*);
void* __calloc(size_t, size_t, const char*, int, const char*);
void* __realloc(void*, size_t, const char*, int, const char*);
void __init();
void __check();

#define free(x) __free(x, __FILE__, __LINE__, __func__)
#define malloc(x) __malloc(x, __FILE__, __LINE__, __func__)
#define calloc(x,y) __calloc(x, y, __FILE__, __LINE__, __func__)
#define realloc(x,y) __realloc(x, y, __FILE__, __LINE__, __func__)
#define main(x,y) __main(x, y)
#endif
#endif
