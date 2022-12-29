/* See LICENSE file for copyright and license details. */
#ifndef _CERT_H_
#define _CERT_H_

#include <unistd.h>
#include <time.h>
#include "gemini.h"

int cert_create(char* url, char* error, int errlen);
int cert_getpath(char* host, char* crt, int crt_len, char* key, int key_len);
int cert_verify(char* host, const char* hash,
		unsigned long long start, unsigned long long end);
int cert_forget(char* host);
int cert_getcert(char* host, int reload);
int gethomefd();
int getconfigfd();
int getdownloadfd();
int getdownloadpath();
void cert_free();
int cert_load();
extern int config_fd;
extern int download_fd;
extern char download_path[1024];

#endif
