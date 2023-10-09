/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
/* socket */
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
/* tls */
#include <tls.h>

#include <unistd.h>

#include "macro.h"
#include "error.h"
#include "dns.h"
#include "gemtext.h"
#include "client.h"
#include "request.h"
#include "secure.h"

struct secure {
	struct tls *ctx;
	struct tls_config *config;
};

int secure_initialized = 0;

int secure_init(struct secure *secure) {

	if (!secure_initialized) {
		if (tls_init()) return ERROR_TLS_FAILURE;
		secure_initialized = 1;
	}

	if (!secure->ctx) {
		secure->ctx = tls_client();
		if (!secure->ctx) {
			return ERROR_TLS_FAILURE;
		}
	}

	if (!secure->config) {
		secure->config = tls_config_new();
		if (!secure->config) {
			return ERROR_TLS_FAILURE;
		}

		tls_config_insecure_noverifycert(secure->config);
		if (tls_configure(secure->ctx, secure->config)) {
			return ERROR_TLS_FAILURE;
		}
	}

	return 0;
}

struct secure *secure_new() {
	struct secure *secure = calloc(1, sizeof(struct secure));
	return secure;
}

int secure_connect(struct secure *secure, struct request request) {

	int ret, sockfd;
	struct sockaddr *addr;

	if ((ret = secure_init(secure))) return ret;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) return ERROR_SOCKET_CREATION;

	addr = request.addr;
	ret = -1;
	switch (addr->sa_family) {
	case AF_INET:
		((struct sockaddr_in*)addr)->sin_port = htons(request.port);
		ret = connect(sockfd, addr, sizeof(struct sockaddr_in));
		break;
	case AF_INET6:
		((struct sockaddr_in6*)addr)->sin6_port = request.port;
		ret = connect(sockfd, addr, sizeof(struct sockaddr_in6));
		break;
	}
	if (ret) {
		ret = ERROR_SOCKET_CONNECTION;
		goto error;
	}

	if (tls_connect_socket(secure->ctx, sockfd, request.name)) {
		ret = ERROR_TLS_FAILURE;
		goto error;
	}

	if (tls_handshake(secure->ctx)) {
		ret = ERROR_TLS_FAILURE;
		goto error;
	}

	return 0;
error:
	close(sockfd);
	return ret;
}

int secure_send(struct secure *secure, const char *data, size_t len) {
	return tls_write(secure->ctx, data, len) == (ssize_t)len ?
		0 : ERROR_TLS_FAILURE;
}

#define MAXIUM_LENGTH 8388608
int secure_read(struct secure *secure, char **data, size_t *length) {

	const size_t pad = 16; /* pad the end with null-bytes in case the data
				  ends with an incomplete unicode character */
	char buf[1024], *ptr;
	size_t len, allocated;
	int i;

	ptr = NULL;
	i = len = allocated = 0;
	while (len < MAXIUM_LENGTH) {
		if (len + sizeof(buf) > allocated) {
			char *tmp;
			allocated += sizeof(buf);
			tmp = realloc(ptr, allocated + pad);
			if (!tmp) {
				free(ptr);
				return ERROR_MEMORY_FAILURE;
			}
			ptr = tmp;
		}
		i = tls_read(secure->ctx, buf, sizeof(buf));
		if (i == TLS_WANT_POLLIN) continue;
		if (i < 1) break;
		memcpy(&ptr[len], buf, i);
		len += i;
	}
	if (i < 0) {
		free(ptr);
		return ERROR_TLS_FAILURE;
	}
	memset(&ptr[len], 0, pad);
	*length = len;
	*data = ptr;
	return 0;
}

int secure_free(struct secure *secure) {
	tls_config_free(secure->config);
	tls_free(secure->ctx);
	free(secure);
	return 0;
}