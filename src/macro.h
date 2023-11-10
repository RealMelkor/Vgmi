/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#include "memcheck.h"
#define ASSERT(X) switch(0){case 0:case (X):;}
#define STRLCPY(X, Y) strlcpy((X), (Y), sizeof(X))
#define STRCMP(X, Y) strncmp((X), (Y), sizeof(X))
#define V(X) (X), sizeof(X)
#define P(X) (&X), sizeof(X)
#define AZ(X) ((X) ? (X) : 1)
#define MAX_URL 1024
#define MAX_HOST 1024
#define LENGTH(X) (sizeof(X) / sizeof(*X))
