/* See LICENSE file for copyright and license details. */
#ifndef _CERT_H_
#define _CERT_H_

#include <unistd.h>

int cert_create(char* url);
int cert_getpath(char* host, char* crt, int crt_len, char* key, int key_len);
int gethomefolder(char* path, size_t len);
int getcachefolder(char* path, size_t len);
int getdownloadfolder(char* path, size_t len);

#endif
