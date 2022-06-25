/* See LICENSE file for copyright and license details. */
#ifndef _STR_H_
#define _STR_H_

#include <stddef.h>

#ifdef __linux__
#define NO_STRLCPY
#define NO_STRLCAT
#define NO_STRNSTR
#endif

#ifdef __OpenBSD__
#define NO_STRNSTR
#endif

#ifdef NO_STRLCPY
size_t strlcpy(char *dst, const char *src, size_t dsize);
#endif

#ifdef NO_STRLCAT
size_t strlcat(char *dst, const char *src, size_t dsize);
#endif

#ifdef NO_STRNSTR
char* strnstr(const char *s, const char *find, size_t slen);
#endif

#endif
