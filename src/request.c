/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@rmf-dev.com>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct request;
#include "macro.h"
#include "gemini.h"
#include "client.h"
#include "url.h"
#include "page.h"
#include "request.h"
#include "secure.h"
#include "memory.h"
#include "strscpy.h"
#include "strnstr.h"
#include "dns.h"
#include "about.h"
#include "error.h"
#include "parser.h"
#include "history.h"
#include "xdg.h"
#include "config.h"

int request_process(struct request *request, struct secure *secure,
			const char *url) {

	char buf[MAX_URL + 8], proxy_buf[CONFIG_STRING_LENGTH];
	int ret, proxy;
	size_t length;

	/* check if it's an about: page */
	if (!memcmp(url, V("about:") - 1)) {
		STRSCPY(request->url, url);
		if ((ret = about_parse(request))) goto failed;
		request->state = STATE_COMPLETED;
		return 0;
	}

	if ((ret = url_parse(request, url))) goto failed;
#ifndef DISABLE_XDG
	if (xdg_available() && (
			request->protocol == PROTOCOL_MAIL ||
			request->protocol == PROTOCOL_HTTP ||
			request->protocol == PROTOCOL_HTTPS ||
			request->protocol == PROTOCOL_GOPHER)) {
		if ((ret = xdg_request(url))) goto failed;
		request->state = STATE_CANCELED;
		return 0;
	}
#endif
	config_get_str(config.proxyHttp, proxy_buf);
	proxy = *proxy_buf && (request->protocol == PROTOCOL_HTTPS ||
				request->protocol == PROTOCOL_HTTP);
	if (request->protocol != PROTOCOL_GEMINI && !proxy) {
		ret = ERROR_UNSUPPORTED_PROTOCOL;
		goto failed;
	}
	if (request->protocol != PROTOCOL_GEMINI) {
		char domain[MAX_HOST] = {0};
		int port;
		url_domain_port(proxy_buf, domain, &port);
		STRSCPY(request->name, domain);
		request->port = port;
	}
	if ((ret = dns_getip(request->name, &request->addr))) goto failed;
	if ((ret = secure_connect(secure, *request))) goto failed;

	length = strscpy(buf, request->url, MAX_URL);
	length += strscpy(&buf[length], "\r\n", sizeof(buf) - length);

	if ((ret = secure_send(secure, buf, length))) goto failed;
	if ((ret = secure_read(secure, &request->data, &request->length)))
		goto failed;
	if (parse_request(NULL, request)) goto failed;

	request->state = STATE_COMPLETED;
	history_add(request->url, request->page.title);
	return 0;
failed:
	request->state = STATE_FAILED;
	request->error = ret;
	return ret;
}

int request_cancel(struct request *request) {
	switch (request->state) {
	case STATE_ONGOING:
		request->state = STATE_CANCELED;
		break;
	case STATE_ENDED:
	case STATE_CANCELED:
		break;
	case STATE_COMPLETED:
		break;
	default:
		return -1;
	}
	return 0;
}

int request_scroll(struct request* req, int scroll, struct rect rect) {

	const int height = rect.h - 2;
	if (!req) return -1;

	if (0 >= (ssize_t)req->page.length - height) {
		req->scroll = 0;
		return 0;
	}

	req->scroll += scroll;
	if (req->scroll < 0) req->scroll = 0;
	else if ((size_t)req->scroll > req->page.length - height)
		req->scroll = req->page.length - height;
	return 0;
}

int request_follow(struct request* req, const char *link,
			char *url, size_t length) {

	const char *end = url + length;
	char *ptr;

	if (!strncmp(link, ".", length)) {
		strscpy(url, req->url, length);
		ptr = strrchr(url, '/');
		if (ptr) *ptr = '\0';
		return 0;
	}
	if (!strncmp(link, "..", length)) {
		char *start;
		strscpy(url, req->url, length);

		start = strnstr(url, "://", length);
		if (!start) return 0;
		start += 3;
		start = strchr(start, '/');
		if (!start) return 0;

		ptr = strrchr(url, '/');
		if (ptr && ptr > start) {
			*ptr = '\0';
			ptr = strrchr(url, '/');
		}
		if (ptr) *(ptr + 1) = '\0';
		return 0;
	}
	if (!memcmp(link, V("about:") - 1)) {
		/* only allow "about:*" links on "about:*" pages */
		if (memcmp(req->url, V("about:") - 1))
			return ERROR_INVALID_URL;
		return strscpy(url, link, length) > length ?
			ERROR_INVALID_URL : 0;
	}
	if (link[0] == '/' && link[1] == '/') {
		strscpy(url, &link[2], length);
		return 0;
	}
	if (link[0] == '/') {
		int i;
		ptr = strnstr(req->url, "://", sizeof(req->url));
		if (!ptr) ptr = req->url;
		else ptr += 4;
		ptr = strchr(ptr, '/');
		if (!ptr) ptr = req->url + strnlen(req->url, length);
		ptr++;
		strscpy(url, req->url, ptr - req->url);
		i = ptr - req->url - 1;
		i += strscpy(&url[i], link, length - i);
		return (size_t)i > length ? ERROR_INVALID_URL : 0;
	}
	if (url_is_absolute(link)) {
		strscpy(url, link, length);
		return 0;
	}
	strscpy(url, req->url, length);
	ptr = strnstr(url, "://", length);
	if (!ptr) ptr = url;
	else ptr += 3;
	ptr = strrchr(ptr, '/');
	if (!ptr) {
		ptr = url + strnlen(url, length);
		*ptr = '/';
	}
	ptr++;
	ptr += strscpy(ptr, link, end - ptr);
	return (size_t)(ptr - url) > length ? ERROR_INVALID_URL : 0;
}

void request_free_ref(struct request req) {
	free_readonly(req.data, req.length);
	free(req.addr);
	page_free(req.page);
}

void request_free(struct request *req) {
	request_free_ref(*req);
	free(req);
}
