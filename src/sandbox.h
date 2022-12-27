/* See LICENSE file for copyright and license details. */
#ifndef _SANDBOX_H_
#define _SANDBOX_H_
int sandbox_init();
int sandbox_close();
#ifndef NO_SANDBOX

#ifndef DISABLE_XDG
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__linux__) || defined(sun)
int xdg_request(char*);
#define xdg_open(x) xdg_request(x)
#endif
#endif

#ifndef xdg_open
int xdg_open(char*);
#endif

#ifdef __FreeBSD__

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

#endif // freebsd

#ifdef sun
#define gmi_savebookmarks() sandbox_savebookmarks()
int sandbox_savebookmarks();
#endif // sun
#endif // sandbox

#include <signal.h>
void sigsys_handler(int signo, siginfo_t *info, void *unused);

#endif
