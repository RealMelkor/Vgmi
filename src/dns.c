/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <errno.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "macro.h"
#include "error.h"
#include "dns.h"

int dns_getip(const char *hostname, ip *out) {

	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_in *ipv4 = NULL;
	struct sockaddr_in6 *ipv6 = NULL;
	int error;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	/* can block */
	if ((error = getaddrinfo(hostname , NULL, &hints , &servinfo)))
		return ERROR_GETADDRINFO | ((-error) << 16);

	for (p = servinfo; p != NULL; p = p->ai_next) {
		switch (p->ai_family) {
		case AF_INET:
			ipv4 = (struct sockaddr_in*)p->ai_addr;
			p = NULL; /* prioritize IPv4 */
			break;
		case AF_INET6:
			ipv6 = (struct sockaddr_in6*)p->ai_addr;
			if (ipv4) p = NULL;
			break;
		}
		if (!p) break;
	}

	error = 0;
	if (ipv4) {
		*out = malloc(sizeof(struct sockaddr_in));
		if (!out) error = ERROR_MEMORY_FAILURE;
		else memcpy(*out, ipv4, sizeof(struct sockaddr_in));
	} else if (ipv6) {
		*out = malloc(sizeof(struct sockaddr_in6));
		if (!out) error = ERROR_MEMORY_FAILURE;
		else memcpy(*out, ipv6, sizeof(struct sockaddr_in6));
	} else error = ERROR_INVALID_ADDRESS;
	
	freeaddrinfo(servinfo);
	return error;
}
