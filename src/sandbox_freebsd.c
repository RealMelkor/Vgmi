/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#if defined (__FreeBSD__) && !defined (DISABLE_SANDBOX)
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
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

#include "sandbox.h"
#include "storage.h"
#include "error.h"

cap_channel_t *_capnet;

int sandbox_connect(int s, void *name, int namelen) {
	return cap_connect(_capnet, s, name, namelen);
}

int sandbox_getaddrinfo(const char *hostname, const char *servname,
			void *hints, void *res) {
	return cap_getaddrinfo(_capnet, hostname, servname, hints, res);
}

#include <sys/types.h>
#include <signal.h>
int sandbox_init(void) {

	int families[] = {AF_INET, AF_INET6};
	cap_net_limit_t* limit;
	cap_rights_t rights;
	cap_channel_t *capcas;

	if (chdir("/var/empty")) return ERROR_SANDBOX_FAILURE;

	cap_rights_init(&rights, CAP_WRITE, CAP_LOOKUP, CAP_READ,
			CAP_SEEK, CAP_CREATE, CAP_FCNTL, CAP_FTRUNCATE);
	if (cap_rights_limit(storage_fd, &rights))
		return ERROR_SANDBOX_FAILURE;

	capcas = cap_init();
	if (!capcas) return ERROR_SANDBOX_FAILURE;

	caph_cache_catpages();
	if (caph_enter()) return ERROR_SANDBOX_FAILURE;

	_capnet = cap_service_open(capcas, "system.net");
	cap_close(capcas);
	if (!_capnet) return ERROR_SANDBOX_FAILURE;

	limit = cap_net_limit_init(_capnet,
			CAPNET_NAME2ADDR |
			CAPNET_CONNECTDNS |
			CAPNET_CONNECT);
	if (!limit) return ERROR_SANDBOX_FAILURE;

	cap_net_limit_name2addr_family(limit, families, 2);
	if (cap_net_limit(limit) < 0) return ERROR_SANDBOX_FAILURE;

	return 0;
}

int sandbox_isolate(void) {
	if (cap_enter()) return ERROR_SANDBOX_FAILURE;
	return 0;
}

int sandbox_set_name(const char *name) {
	setproctitle("%s", name);
	return 0;
}
#else
typedef int hide_warning;
#endif
