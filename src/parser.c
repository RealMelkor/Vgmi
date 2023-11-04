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
#include "macro.h"
#include "error.h"
#include "sandbox.h"
#include "url.h"
#include "page.h"
#include "gemtext.h"
#include "gemini.h"
#include "request.h"
#include "strlcpy.h"
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

	int ret;
	if (!parser) parser = &request_parser;

	pthread_mutex_lock(&parser->mutex);

	write(parser->out, P(request->length));
	write(parser->out, request->data, request->length);

	if (vread(parser->in, P(request->status))) return -1;
	if (vread(parser->in, V(request->meta))) return -1;

	request->text.links_count = 0;
	request->text.links = NULL;
	ret = 0;
	while (1) {

		size_t length = 0;
		char url[MAX_URL] = {0};
		void *tmp;
		char *ptr;

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
		ptr = malloc(length);
		if (!ptr) {
			ret = ERROR_MEMORY_FAILURE;
			break;
		}
		strlcpy(ptr, url, length);

		tmp = realloc(request->text.links, sizeof(char*) * 
				(request->text.links_count + 1));
		if (!tmp) {
			ret = ERROR_MEMORY_FAILURE;
			break;
		}
		request->text.links = tmp;
		request->text.links[request->text.links_count] = ptr;
		request->text.links_count++;
	}
	pthread_mutex_unlock(&parser->mutex);
	return ret;
}

void parser_page(int in, int out) {
	char *data = NULL;
	while (1) { /* TODO: send error code */

		int ret;
		size_t length;
		int width;

		if (vread(in, &length, sizeof(length))) break;
		if (vread(in, &width, sizeof(width))) break;

		/* TODO: support other format than gemtext */
		ret = gemtext_prerender(in, length, width, out);
		if (ret) break;

		free(data);
		data = NULL;
	}
	free(data);
}

int parse_page(struct parser *parser, struct request *request, int width) {

	int ret;
	if (!parser) parser = &page_parser;

	pthread_mutex_lock(&parser->mutex);

	write(parser->out, P(request->length));
	write(parser->out, P(width));

	request->text.width = width;
	ret = page_update(parser->in, parser->out, request->data,
			request->length, &request->text);
	if (ret) return ret;

	pthread_mutex_unlock(&parser->mutex);
	return ret;
}

int parser_create(struct parser *parser, int type, char *name) {
	int in_pipe[2], out_pipe[2];
	unsigned char byte;
	if (pipe(in_pipe)) return -1;
	if (pipe(out_pipe)) return -1;
	if (fork()) {
		int len;
		close(in_pipe[1]);
		close(out_pipe[0]);
		len = read(in_pipe[0], &byte, 1);
		if (len != 1) return ERROR_SANDBOX_FAILURE;
		if (byte) return byte;
		parser->in = in_pipe[0];
		parser->out = out_pipe[1];
		pthread_mutex_init(&parser->mutex, NULL);
		return 0;
	}
	close(in_pipe[0]);
	close(out_pipe[1]);
	byte = 0;
	sandbox_set_name(name);
	if (sandbox_isolate()) byte = ERROR_SANDBOX_FAILURE;
	write(in_pipe[1], &byte, 1);
	switch (type) {
	case PARSER_GEMTEXT:
		parser_page(out_pipe[0], in_pipe[1]);
		break;
	case PARSER_REQUEST:
		parser_request(out_pipe[0], in_pipe[1]);
		break;
	}
#ifdef __linux__
	syscall(SYS_exit, EXIT_SUCCESS);
#else
	exit(0);
#endif
	return 0;
}

int parser_request_create() {
	return parser_create(&request_parser, PARSER_REQUEST,
			"vgmi [request]");
}

int parser_page_create() {
	return parser_create(&page_parser, PARSER_GEMTEXT,
			"vgmi [page]");
}
