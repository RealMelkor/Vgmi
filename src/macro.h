/*
 * ISC License
 * Copyright (c) 2024 RMF <rawmonk@rmf-dev.com>
 */
#ifdef __APPLE__
#undef strlcpy
#undef snprintf
#endif
#include "memcheck.h"
#define ASSERT(X) switch(0){case 0:case (X):;}
#define STRSCPY(X, Y) strscpy((X), (Y), sizeof(X))
#define STRCMP(X, Y) strncmp((X), (Y), sizeof(X))
#define STRLEN(X) strnlen((X), sizeof(X))
#define V(X) (X), sizeof(X)
#define P(X) (&X), sizeof(X)
#define AZ(X) ((X) ? (X) : 1)
#define MAX_URL 1024
#define MAX_HOST 1024
#define LENGTH(X) (sizeof(X) / sizeof(*X))
#ifndef PATH_MAX
#define PATH_MAX 1024
#endif
#ifndef NO_SANDBOX
#ifdef __linux__
#define STATIC_ALLOC
#endif
#endif
#ifdef __OpenBSD__
#define TIME_T "%lld"
#else
#define TIME_T "%ld"
#endif
