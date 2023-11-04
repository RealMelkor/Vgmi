/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
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
#include "strlcpy.h"
#include "dns.h"
#include "about.h"
#include "error.h"
#include "parser.h"

int request_process(struct request *request, struct secure *secure,
			const char *url) {

	char buf[MAX_URL + 8];
	int ret;
	size_t length;

	/* check if it's an about: page */
	if (!memcmp(url, V("about:") - 1)) {
		STRLCPY(request->url, url);
		if ((ret = about_parse(request))) goto failed;
		request->state = STATE_COMPLETED;
		return 0;
	}

	if ((ret = url_parse(request, url))) goto failed;
	if ((ret = dns_getip(request->name, &request->addr))) goto failed;
	if ((ret = secure_connect(secure, *request))) goto failed;

	length = strlcpy(buf, request->url, MAX_URL);
	length += strlcpy(&buf[length], "\r\n", sizeof(buf) - length);

	if ((ret = secure_send(secure, buf, length))) goto failed;
	if ((ret = secure_read(secure, &request->data, &request->length)))
		goto failed;
	if (parse_request(NULL, request)) goto failed;

	request->state = STATE_COMPLETED;
	return 0;
failed:
	request->state = STATE_FAILED;
	request->error = ret;
	return ret;
}

int request_cancel(struct request *request) {
	/* TODO: mutex lock */
	switch (request->state) {
	case STATE_ONGOING:
		request->state = STATE_CANCELED;
		break;
	case STATE_ENDED:
	case STATE_CANCELED:
		break;
	case STATE_COMPLETED:
		/*request->state = STATE_ENDED;*/
		break;
	default:
		return -1;
	}
	return 0;
}

int request_scroll(struct request* req, int scroll, struct rect rect) {

	if (!req) return -1;

	if (0 >= (ssize_t)req->text.length - rect.h + 2) {
		req->scroll = 0;
		return 0;
	}

	req->scroll += scroll;
	if (req->scroll < 0) req->scroll = 0;
	else if ((size_t)req->scroll > req->text.length - rect.h + 2)
		req->scroll = req->text.length - rect.h + 2;
	return 0;
}

int request_follow(struct request* req, const char *link,
			char *url, size_t length) {

	const char *end = url + length;
	char *ptr;

	if (!memcmp(link, V("about:") - 1)) {
		/* only allow "about:*" links on "about:*" pages */
		if (memcmp(req->url, V("about:") - 1))
			return ERROR_INVALID_URL;
		return strlcpy(url, link, length) > length ?
			ERROR_INVALID_URL : 0;
	}
	if (link[0] == '/' && link[1] == '/') {
		strlcpy(url, &link[2], length);
		return 0;
	}
	if (link[0] == '/') {
		int i;
		ptr = strstr(req->url, "://");
		if (!ptr) return -1;
		ptr += 4;
		ptr = strchr(ptr, '/');
		if (!ptr) ptr = req->url + strnlen(req->url, length);
		ptr++;
		strlcpy(url, req->url, ptr - req->url);
		i = ptr - req->url - 1;
		i += strlcpy(&url[i], link, length - i);
		return (size_t)i > length ? ERROR_INVALID_URL : 0;
	}
	if (strstr(link, "://")) {
		strlcpy(url, link, length);
		return 0;
	}
	strlcpy(url, req->url, length);
	ptr = strstr(url, "://");
	if (!ptr) return -1;
	ptr += 3;
	ptr = strrchr(ptr, '/');
	if (!ptr) {
		ptr = url + strnlen(url, length);
		*ptr = '/';
	}
	ptr++;
	ptr += strlcpy(ptr, link, end - ptr);
	return (size_t)(ptr - url) > length ? ERROR_INVALID_URL : 0;
}

int request_free_ref(struct request req) {
	free(req.data);
	free(req.addr);
	page_free(req.text);
	return 0;
}

int request_free(struct request *req) {
	request_free_ref(*req);
	free(req);
	return 0;
}
