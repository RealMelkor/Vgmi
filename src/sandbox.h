/* See LICENSE file for copyright and license details. */
#ifndef _CAPSICUM_H_
#define _CAPSICUM_H_
int sandbox_init();
int sandbox_close();
#ifdef __FreeBSD__

#ifndef XDG_DISABLE
#define xdg_open(x) xdg_request(x)
#endif
extern int config_folder;
#include <netdb.h>
#include <fcntl.h>
int sandbox_getaddrinfo(const char *hostname, const char *servname, 
			const struct addrinfo *hints, struct addrinfo **res);
int sandbox_connect(int s, const struct sockaddr *name, socklen_t namelen);
int make_readonly(FILE* f);
int makefd_readonly(int fd);
int make_writeonly(FILE* f);
int makefd_writeonly(int fd);
int makefd_writeseek(int fd);
#define getaddrinfo(a, b, c, d) sandbox_getaddrinfo(a, b, c, d)
#define connect(a, b, c) sandbox_connect(a, b, c)

#endif
#endif
