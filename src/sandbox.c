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

#include "cert.h"

cap_channel_t *_capnet;
cap_net_limit_t* limit;

int sandbox_connect(int s, const struct sockaddr *name, socklen_t namelen) {
        return cap_connect(_capnet, s, name, namelen);
}

int sandbox_getaddrinfo(const char *hostname, const char *servname, 
			const struct addrinfo *hints, struct addrinfo **res) {
        return cap_getaddrinfo(_capnet, hostname, servname, hints, res);
}

extern int config_folder;
int casper_init() {
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

#else
typedef int remove_iso_warning;
#endif


