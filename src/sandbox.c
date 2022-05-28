/* See LICENSE file for copyright and license details. */
#ifdef __FreeBSD__
#include <stdio.h>
#include <sys/capsicum.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <strings.h>
#define WITH_CASPER
#include <sys/nv.h>
#include <libcasper.h>
#include <casper/cap_net.h>
#include <capsicum_helpers.h>

cap_channel_t *_capnet;
cap_net_limit_t* limit;

int sandbox_connect(int s, const struct sockaddr *name, socklen_t namelen) {
        return cap_connect(_capnet, s, name, namelen);
}

int sandbox_getaddrinfo(const char *hostname, const char *servname, 
			const struct addrinfo *hints, struct addrinfo **res) {
        return cap_getaddrinfo(_capnet, hostname, servname, hints, res);
}

int xdg_pipe[2];
int xdg_open(char*);

#include <string.h>
int xdg_request(char* str) {
	int len = strnlen(str, 1024)+1;
	return write(xdg_pipe[0], str, len) != len;
}

void xdg_listener() {
	char buf[4096];
	while (1) {
		int len = read(xdg_pipe[1], buf, sizeof(buf));
		if (len <= 0)
			break;
		xdg_open(buf);
	}
}

#include "cert.h"
extern int config_folder;
int sandbox_init() {
#ifndef XDG_DISABLE
	if (pipe(xdg_pipe)) {
                printf("pipe failed\n");
                return -1;
	}
	if (fork() == 0) {
		close(xdg_pipe[0]);
		xdg_listener();
		exit(0);
	}
	close(xdg_pipe[1]);
#endif
	char path[1024];
	getconfigfolder(path, sizeof(path));

	cap_rights_t rights;
	cap_rights_init(&rights, CAP_WRITE, CAP_LOOKUP, CAP_READ, CAP_SEEK, CAP_CREATE, CAP_FCNTL);
        if (cap_rights_limit(config_folder, &rights)) {
                printf("cap_rights_limit failed\n");
                return -1;
        }

        cap_channel_t *capcas;
        capcas = cap_init();
        if (capcas == NULL) {
                printf("cap_init failed\n");
                return -1;
        }
        caph_cache_catpages();
        if (caph_enter()) {
                printf("cap_enter failed\n");
                return -1;
        }
        _capnet = cap_service_open(capcas, "system.net");
        cap_close(capcas);
        if (_capnet == NULL) {
                printf("failed to open system.net service\n");
                return -1;
        }
        limit = cap_net_limit_init(_capnet,
                                   CAPNET_NAME2ADDR | CAPNET_CONNECTDNS | CAPNET_CONNECT);
        if (limit == NULL) {
                printf("Unable to create limits.\n");
                return -1;
        }
        int families[] = {AF_INET, AF_INET6};
        cap_net_limit_name2addr_family(limit, families, 2);
        if (cap_net_limit(limit) < 0) {
                printf("Unable to apply limits.\n");
                return -1;
        }
	return 0;
}

int sandbox_close() {
	close(xdg_pipe[0]);
	close(xdg_pipe[1]);
	return 0;
}

int makefd_readonly(int fd) {
	cap_rights_t rights;
        cap_rights_init(&rights, CAP_SEEK, CAP_READ);
        if (cap_rights_limit(fd, &rights))
                return -1;
	return 0;
}

int make_readonly(FILE* f) {
	return makefd_readonly(fileno(f));
}

int makefd_writeonly(int fd) {
	cap_rights_t rights;
        cap_rights_init(&rights, CAP_WRITE);
        if (cap_rights_limit(fd, &rights))
                return -1;
	return 0;
}

int makefd_writeseek(int fd) {
	cap_rights_t rights;
        cap_rights_init(&rights, CAP_WRITE, CAP_SEEK);
        if (cap_rights_limit(fd, &rights))
                return -1;
	return 0;
}

int make_writeonly(FILE* f) {
	return makefd_writeonly(fileno(f));
}
#elif __OpenBSD__
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "cert.h"

int sandbox_init() {
#ifndef HIDE_HOME
	char path[1024];
	if (gethomefolder(path, sizeof(path)) < 1) {
		printf("Failed to get home folder\n");
		return -1;
	}
#endif
	char certpath[1024];
	if (getconfigfolder(certpath, sizeof(certpath)) < 1) {
		printf("Failed to get cache folder\n");
		return -1;
	}
	char downloadpath[1024];
	if (getdownloadfolder(downloadpath, sizeof(downloadpath)) < 1) {
		printf("Failed to get download folder\n");
		return -1;
	}
	if (
#ifndef HIDE_HOME
		unveil(path, "r") ||
#endif
		unveil(certpath, "rwc") || 
		unveil(downloadpath, "rwc") || 
#ifndef DISABLE_XDG
		unveil("/bin/sh", "x") ||
		unveil("/usr/bin/which", "x") ||
		unveil("/usr/local/bin/xdg-open", "x") ||
#endif
		unveil("/etc/resolv.conf", "r") ||
		unveil(NULL, NULL)) {
		printf("Failed to unveil\n");
		return -1;
	}
#ifndef DISABLE_XDG
	if (pledge("stdio rpath wpath cpath inet dns tty exec proc", NULL)) {
#else
	if (pledge("stdio rpath wpath cpath inet dns tty", NULL)) {
#endif
		printf("Failed to pledge\n");
		return -1;
	}
	return 0;
}

int sandbox_close() {
	return 0;
}
#else
int sandbox_init() {
	return 0;
}

int sandbox_close() {
	return 0;
}
#endif


