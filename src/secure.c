/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@rmf-dev.com>
 */
/* socket */
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#ifdef __FreeBSD__
#include <netinet/in.h>
#include "sandbox.h"
#define connect sandbox_connect
#endif
/* tls */
#include <tls.h>

#include <errno.h>
#include <unistd.h>

#include "macro.h"
#include "strscpy.h"
#include "error.h"
#include "client.h"
#include "page.h"
#include "request.h"
#include "known_hosts.h"
#include "secure.h"
#include "certificate.h"
#include "storage.h"
#include "memory.h"
#include "config.h"

struct secure {
	struct tls *ctx;
	struct tls_config *config;
	int socket;
};

int secure_initialized = 0;

static const char *_tls_error(struct tls *ctx) {
	const char *str = tls_error(ctx);
	return str ? str : "Success";
}

static const char *_tls_config_error(struct tls_config *config) {
	const char *str = tls_config_error(config);
	return str ? str : "Success";
}

int secure_init(struct secure *secure, const char *hostname) {

	if (!secure_initialized) {
		if (tls_init()) {
			STRSCPY(error_tls, strerror(errno));
			return ERROR_TLS_FAILURE;
		}
		secure_initialized = 1;
	}

	if (!secure->ctx) {
		secure->ctx = tls_client();
		if (!secure->ctx) {
			return ERROR_MEMORY_FAILURE;
		}
	}

	if (!secure->config) {
		secure->config = tls_config_new();
		if (!secure->config) {
			return ERROR_MEMORY_FAILURE;
		}

		tls_config_insecure_noverifycert(secure->config);
		if (tls_configure(secure->ctx, secure->config)) {
			STRSCPY(error_tls, _tls_config_error(secure->config));
			return ERROR_TLS_FAILURE;
		}

		if (hostname) {
			char cert_path[1024], key_path[1024];
			char cert[4096], key[4096];
			size_t cert_len, key_len;

			int ret = certificate_getpath(hostname,
					V(cert_path), V(key_path));
			/* TODO: handle errors properly */
			if (ret < 0) return 0;
			if (storage_read(cert_path, V(cert), &cert_len))
				return 0;
			if (storage_read(key_path, V(key), &key_len))
				return 0;
			tls_config_set_keypair_mem(secure->config,
					(unsigned char*)cert, cert_len,
					(unsigned char*)key, key_len);
		}
	}

	return 0;
}

struct secure *secure_new(void) {
	struct secure *secure = calloc(1, sizeof(struct secure));
	return secure;
}

int secure_connect(struct secure *secure, struct request request) {

	int ret, sockfd;
	struct sockaddr *addr;

	if ((ret = secure_init(secure, request.name))) return ret;

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
		((struct sockaddr_in6*)addr)->sin6_port = htons(request.port);
		ret = connect(sockfd, addr, sizeof(struct sockaddr_in6));
		break;
	}
	if (ret) {
		ret = ERROR_SOCKET_CONNECTION;
		goto error;
	}

	if (tls_connect_socket(secure->ctx, sockfd, request.name)) {
		ret = ERROR_TLS_FAILURE;
		STRSCPY(error_tls, _tls_error(secure->ctx));
		goto error;
	}

	if (tls_handshake(secure->ctx)) {
		ret = ERROR_TLS_FAILURE;
		STRSCPY(error_tls, _tls_error(secure->ctx));
		goto error;
	}

	{
		const char *hash = tls_peer_cert_hash(secure->ctx);
		time_t start = tls_peer_cert_notbefore(secure->ctx);
		time_t end = tls_peer_cert_notafter(secure->ctx);
		ret = known_hosts_verify(request.name, hash, start, end);
		if (ret) goto error;
	}

	secure->socket = sockfd;
	return 0;
error:
	close(sockfd);
	return ret;
}

int secure_send(struct secure *secure, const char *data, size_t len) {
	if (tls_write(secure->ctx, data, len) == (ssize_t)len) return 0;
	STRSCPY(error_tls, _tls_error(secure->ctx));
	return ERROR_TLS_FAILURE;
}

int secure_read(struct secure *secure, char **data, size_t *length) {

	char buf[1024], *ptr;
	size_t len, allocated;
	int i;

	ptr = NULL;
	i = len = allocated = 0;
	while (len < config.maximumBodyLength) {
		if (len + sizeof(buf) > allocated) {
			char *tmp;
			allocated += sizeof(buf);
			tmp = realloc(ptr, allocated);
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
	if (len >= config.maximumBodyLength) {
		free(ptr);
		return ERROR_RESPONSE_TOO_LARGE;
	}
	if (i < 0) {
		free(ptr);
		STRSCPY(error_tls, _tls_error(secure->ctx));
		return ERROR_TLS_FAILURE;
	}
	*length = len;
	if (readonly(ptr, *length, data)) {
		free(ptr);
		return ERROR_ERRNO;
	}
	free(ptr);
	return 0;
}

int secure_free(struct secure *secure) {
	if (secure->ctx) {
		close(secure->socket);
		tls_close(secure->ctx);
	}
	tls_config_free(secure->config);
	tls_free(secure->ctx);
	free(secure);
	return 0;
}
