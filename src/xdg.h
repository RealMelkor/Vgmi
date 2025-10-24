/* See LICENSE file for copyright and license details. */
#ifndef _XDG_H_
#define _XDG_H_

#include <unistd.h>

extern char download_path[1024];
int xdg_path(char* path, size_t len);
void xdg_close(void);
int xdg_init(void);
int xdg_request(const char* str);

#endif
