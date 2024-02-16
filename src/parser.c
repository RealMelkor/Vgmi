/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#ifdef __linux__
#define _DEFAULT_SOURCE
#include <syscall.h>
#endif
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <poll.h>
#include "macro.h"
#include "error.h"
#include "sandbox.h"
#include "proc.h"
#include "url.h"
#include "page.h"
#include "gemini.h"
#include "request.h"
#include "strlcpy.h"
#include "strnstr.h"
#include "memory.h"
#define PARSER_INTERNAL
#include "parser.h"

struct parser {
	int in;
	int out;
	pthread_mutex_t mutex;
};
struct parser request_parser;
struct parser page_parser;

void parser_request(int in, int out);

int parse_request(struct parser *parser, struct request *request) {

	int ret, gemtext;
	size_t sent;
	if (!parser) parser = &request_parser;

	pthread_mutex_lock(&parser->mutex);

	write(parser->out, P(request->length));
	sent = request->length > sizeof(request->meta) ?
		sizeof(request->meta) : request->length;
	write(parser->out, request->data, sent);

	if ((ret = vread(parser->in, P(request->status)))) goto fail;
	if (request->status == -1) {
		int error;
		if ((ret = vread(parser->in, P(error)))) goto fail;
		ret = error;
		goto fail;
	}
	if ((ret = vread(parser->in, V(request->meta)))) goto fail;
	if ((ret = vread(parser->in, P(request->page.mime)))) goto fail;
	if ((ret = vread(parser->in, P(request->page.offset)))) goto fail;

	gemtext = request->status == GMI_SUCCESS &&
			is_gemtext(V(request->meta));
	request->page.links_count = 0;
	request->page.links = NULL;
	ret = 0;
	if (!gemtext && sent < request->length)
		write(parser->out, request->data, request->length - sent);
	while (gemtext) {

		size_t length = 0;
		char url[MAX_URL] = {0};
		void *tmp;
		char *ptr;
		int fds;
		struct pollfd pfd;

		pfd.fd = parser->in;
		pfd.events = POLLIN;
		fds = poll(&pfd, 1, 0);
		if (fds == -1) return ERROR_ERRNO;
		if (!fds) {
			int bytes;
			pfd.fd = parser->out;
			pfd.events = POLLOUT;
			fds = poll(&pfd, 1, 0);
			if (fds == -1) return ERROR_ERRNO;
			if (!fds) continue;
			bytes = PARSER_CHUNK;
			if (sent + bytes > request->length)
				bytes = request->length - sent;
			if (bytes < 1) continue;
			write(parser->out, &request->data[sent], bytes);
			sent += bytes;
			continue;
		}

		if (vread(parser->in, P(length))) {
			ret = -1;
			break;
		}
		if (length == (size_t)-1) break;
		if (!length || vread(parser->in, url, length)) {
			ret = -1;
			break;
		}

		length++;
		if ((ret = readonly(url, length, &ptr))) break;

		tmp = realloc(request->page.links,
			sizeof(char*) * (request->page.links_count + 1));
		if (!tmp) {
			ret = ERROR_MEMORY_FAILURE;
			break;
		}
		request->page.links = tmp;
		request->page.links[request->page.links_count] = ptr;
		request->page.links_count++;
	}

	if (gemtext && (ret = vread(parser->in, V(request->page.title))))
		goto fail;
	if (!gemtext || !request->page.title[0])
		STRLCPY(request->page.title, request->url);
fail:
	pthread_mutex_unlock(&parser->mutex);
	return ret;
}

void parser_page(int in, int out) {
	char *data = NULL;
	parser_sandbox(out, "vgmi [page]");
	while (1) { /* TODO: send error code */

		int ret;
		size_t length;
		int width;
		int mime;

		if (vread(in, &length, sizeof(length))) break;
		if (vread(in, &width, sizeof(width))) break;
		if (vread(in, &mime, sizeof(mime))) break;

		switch (mime) {
		case MIME_GEMTEXT:
			ret = parse_gemtext(in, length, width, out);
			break;
		case MIME_PLAIN:
			ret = parse_plain(in, length, width, out);
			break;
		default:
			ret = parse_binary(in, length, width, out);
			break;
		}

		if (ret) break;

		free(data);
		data = NULL;
	}
	free(data);
}

int parse_page(struct parser *parser, struct request *request, int width) {

	int ret;
	size_t length;
	if (!parser) parser = &page_parser;

	pthread_mutex_lock(&parser->mutex);

	length = request->length - request->page.offset;
	write(parser->out, P(length));
	write(parser->out, P(width));
	write(parser->out, P(request->page.mime));

	request->page.width = width;
	ret = page_update(parser->in, parser->out,
			&request->data[request->page.offset],
			length, &request->page);

	pthread_mutex_unlock(&parser->mutex);
	return ret;
}

int parser_create(struct parser *parser, int type) {
	switch (type) {
	case PARSER_GEMTEXT:
		return proc_fork("--page", &parser->in, &parser->out);
	case PARSER_REQUEST:
		return proc_fork("--request", &parser->in, &parser->out);
	}
	return 0;
}

int parser_request_create(void) {
	return parser_create(&request_parser, PARSER_REQUEST);
}

int parser_page_create(void) {
	return parser_create(&page_parser, PARSER_GEMTEXT);
}

int parser_sandbox(int out, const char *title) {
	uint8_t byte;
	sandbox_set_name(title);
	if (sandbox_isolate()) return -1;
	byte = 0;
	write(out, &byte, 1);
	return 0;
}
