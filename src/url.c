/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "macro.h"
#include "url.h"
#include "page.h"
#include "request.h"
#include "error.h"
#include "strnstr.h"
#include "strlcpy.h"

int servername_from_url(const char *url, char* out, size_t len) {

	const char *start, *port, *end;

	start = strnstr(url, "://", len);
	if (!start) start = url;
	else start += sizeof("://") - 1;

	port = strchr(start, ':');
	end = strchr(start, '/');
	if (!end || (port && port < end)) end = port;
	if (!end) end = start + strlen(url);

	if ((size_t)(end - start) >= len) return ERROR_BUFFER_OVERFLOW;

	strlcpy(out, start, end - start + 1);
	return 0;
}

int protocol_from_url(const char *url) {
	if (!strnstr(url, "://", MAX_URL)) return PROTOCOL_NONE; /* default */
	if (!memcmp(url, V("gemini://") - 1)) return PROTOCOL_GEMINI;
	if (!memcmp(url, V("http://") - 1)) return PROTOCOL_HTTP;
	if (!memcmp(url, V("https://") - 1)) return PROTOCOL_HTTPS;
	if (!memcmp(url, V("gopher://") - 1)) return PROTOCOL_GOPHER;
	return PROTOCOL_UNKNOWN;
}

int port_from_url(const char *url) {

	const char *start, *end;
	char buf[MAX_URL];
	int port;

	start = strnstr(url, "://", MAX_URL);
	if (!start) start = url;
	end = strchr(start + sizeof("://"), '/');
	start = strchr(start + sizeof("://"), ':');
	if (!start || (end && end < start)) return 0;
	start++;
	end = strchr(start, '/') + 1;
	if (!end) end = start + strlen(start);
	strlcpy(buf, start, end - start);
	port = atoi(buf);
	if (!port) return ERROR_INVALID_PORT;
	return port;
}

int url_parse(struct request* request, const char *url) {

	int protocol, port, ret;
	char buf[MAX_URL];

	memset(request, 0, sizeof(*request));

	if ((ret = servername_from_url(url, V(request->name)))) return ret;

	protocol = protocol_from_url(url);
	if (protocol == PROTOCOL_UNKNOWN) return ERROR_UNKNOWN_PROTOCOL;
	if (protocol == PROTOCOL_NONE) {
		size_t length = STRLCPY(buf, "gemini://");
		int i;
		i = strlcpy(&buf[length], url, sizeof(buf) - length);
		i += length;
		buf[i] = '/';
		buf[i + 1] = '\0';
		protocol = PROTOCOL_GEMINI;
	} else STRLCPY(buf, url);

	port = port_from_url(url);
	if (port < 0) return port;
	if (!port) {
		switch (protocol) {
		case PROTOCOL_GEMINI: port = 1965; break;
		}
	}

	request->protocol = protocol;
	request->port = port;
	STRLCPY(request->url, buf);

	return 0;
}
