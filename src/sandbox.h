/* See LICENSE file for copyright and license details. */
#ifndef _SANDBOX_H_
#define _SANDBOX_H_

#include <stddef.h>

int sandbox_init();
int sandbox_close();

#ifndef NO_SANDBOX

#ifndef DISABLE_XDG
#if defined(__FreeBSD__) || defined(__OpenBSD__) || \
    defined(__linux__) || defined(sun)
int xdg_request(char*);
#ifndef SB_IGNORE
#define xdg_open(x) xdg_request(x)
#endif
#endif
#endif

#ifndef xdg_open
int xdg_open(char*);
#endif

#ifdef __FreeBSD__

#define SANDBOX_FREEBSD

extern int config_folder;
#include <netdb.h>
#include <fcntl.h>
#include <stdio.h>
int sandbox_getaddrinfo(const char *hostname, const char *servname, 
			const struct addrinfo *hints, struct addrinfo **res);
int sandbox_connect(int s, const struct sockaddr *name, socklen_t namelen);
int make_readonly(FILE* f);
int makefd_readonly(int fd);
int make_writeonly(FILE* f);
int makefd_writeonly(int fd);
int makefd_writeseek(int fd);
#ifndef SB_IGNORE
#define getaddrinfo(a, b, c, d) sandbox_getaddrinfo(a, b, c, d)
#define connect(a, b, c) sandbox_connect(a, b, c)
#endif

#endif // freebsd

#ifdef sun

#define SANDBOX_SUN

#define SBC unsigned int
extern SBC WR_BOOKMARKS;
extern SBC WR_KNOWNHOSTS;
extern SBC WR_KNOWNHOST_ADD;
extern SBC WR_DOWNLOAD;
extern SBC WR_CERTIFICATE;
extern SBC WR_END;
extern SBC RD_CERTIFICATE;

int sandbox_savebookmarks();
struct gmi_tab;
int sandbox_download(struct gmi_tab* tab, const char* path);
int sandbox_dl_length(size_t length);
int sandbox_cert_create(char* host, char* error, int errlen);
extern int wr_pair[2];
extern int rd_pair[2];

#ifndef SB_IGNORE
#define gmi_savebookmarks() sandbox_savebookmarks()
#define cert_create(X, Y, Z) sandbox_cert_create(X, Y, Z)
#endif

#endif // sun

#else // no sandbox
int xdg_open(char*);
#endif

#include <signal.h>
void sigsys_handler(int signo, siginfo_t *info, void *unused);

#endif
