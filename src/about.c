/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#pragma GCC diagnostic ignored "-Woverlength-strings"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "macro.h"
#include "gemini.h"
#include "gemtext.h"
#include "request.h"
#include "about.h"
#include "error.h"
#include "strlcpy.h"
#define KNOWN_HOSTS_INTERNAL
#include "known_hosts.h"
#include "sandbox.h"
#include "parser.h"

#define HEADER "20 text/gemini\r\n"

static const char header[] = HEADER;

char about_page[] =
HEADER \
"# List of \"about\" pages\n\n" \
"=>about:about\n" \
"=>about:blank\n" \
"=>about:certificate\n" \
"=>about:downloads\n" \
"=>about:history\n" \
"=>about:known-hosts\n" \
"=>about:license\n" \
"=>about:newtab\n" \
"=>about:sandbox\n";

char license_page[] =
HEADER \
"# Vgmi license\n\n" \
"Copyright (c) 2023 RMF <rawmonk@firemail.cc>\n\n" \
"Permission to use, copy, modify, and distribute this software for any\n" \
"purpose with or without fee is hereby granted, provided that the above\n" \
"copyright notice and this permission notice appear in all copies.\n\n" \
"THE SOFTWARE IS PROVIDED \"AS IS\" AND THE AUTHOR DISCLAIMS ALL WARRANTIES\n" \
"WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF\n" \
"MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR\n" \
"ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES\n" \
"WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN\n" \
"ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF\n" \
"OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.\n\n";

char newtab_page[] =
HEADER \
"# Vgmi - " VERSION "\n\n" \
"A Gemini client written in C with vim-like keybindings\n\n" \
"=>gemini://geminispace.info Geminispace\n" \
"=>gemini://geminispace.info/search Search\n" \
"=> gemini://gemini.rmf-dev.com\n" \
"=>about:blank\n" \
"=>about:newtab\n" \
"=>about:history\n" \
"=>about:about\n" \
"=>gemini://gemini.rmf-dev.com/repo/Vaati/Vgmi/readme Vgmi\n" \
"=>gemini://gemini.rmf-dev.com/static/ static files\n";

char sandbox_page[] =
HEADER \
"# Sandbox information\n\n" \
""SANDBOX_INFO"\n" \
"Filesystem access\t: "SANDBOX_FILESYSTEM"\n" \
"IPC access\t\t\t: "SANDBOX_IPC"\n" \
"Devices access\t\t: "SANDBOX_DEVICE"\n" \
"Parser isolation\t: "SANDBOX_PARSER"\n";

char *show_history(struct request *request, size_t *length_out) {

	size_t length = 0;
	char *data = NULL;
	struct request *req;

	data = malloc(sizeof(header) + 1);
	if (!data) return NULL;
	length = sizeof(header) - 1;
	strlcpy(data, header, length + 1);

	for (req = request; req; req = req->next) {
		char buf[2048], *tmp;
		int len = snprintf(V(buf), "=>%s\n", request->url);
		tmp = realloc(data, length + len + 1);
		if (!tmp) {
			free(data);
			return NULL;
		}
		data = tmp;
		strlcpy(&data[length], buf, len);
		length += len;
	}
	*length_out = length;
	return data;
}

static int parse_data(struct request *request, char *data, size_t len) {
	request->status = GMI_SUCCESS;
	request->length = len;
	request->data = data;
	return parse_request(NULL, request);
}

static int static_page(struct request *request, const char *data, size_t len) {
	char *ptr = malloc(len);
	if (!ptr) return ERROR_MEMORY_FAILURE;
	memcpy(ptr, data, len);
	return parse_data(request, ptr, len - 1);
}

static int known_hosts_page(char **out, size_t *length_out) {

	char *data = NULL;
	size_t length = 0;
	size_t i;
	const char title[] = "# Known hosts\n\n";

	length = sizeof(header) + sizeof(title);
	data = malloc(length);
	if (!data) return ERROR_MEMORY_FAILURE;
	strlcpy(data, header, length);
	strlcpy(&data[sizeof(header) - 1], title, length - sizeof(header));
	length -= 2;

	for (i = 0; i < known_hosts_length; i++) {

		char buf[1024], from[64], to[64];
		int len;
		void *tmp;
		struct tm *tm;

		tm = localtime(&known_hosts[i].start);
		strftime(V(from), "%Y/%m/%d %H:%M:%S", tm);

		tm = localtime(&known_hosts[i].end);
		strftime(V(to), "%Y/%m/%d %H:%M:%S", tm);

		len = snprintf(V(buf),
			"* %s\n> Hash: %s\n"
			"> From: %s\n> Expiration: %s\n\n",
			known_hosts[i].host, known_hosts[i].hash,
			from, to) + 1;
		tmp = realloc(data, length + len);
		if (!tmp) {
			free(data);
			return ERROR_MEMORY_FAILURE;
		}
		data = tmp;
		strlcpy(&data[length], buf, len);
		length += len - 1;
	}

	*out = data;
	*length_out = length;
	return 0;
}

int about_parse(struct request *request) {
	if (!strcmp(request->url, "about:about")) {
		return static_page(request, V(about_page));
	}
	if (!strcmp(request->url, "about:blank")) {
		request->status = GMI_SUCCESS;
		return 0;
	}
	if (!strcmp(request->url, "about:license")) {
		return static_page(request, V(license_page));
	}
	if (!strcmp(request->url, "about:newtab")) {
		return static_page(request, V(newtab_page));
	}
	if (!strcmp(request->url, "about:known-hosts")) {
		char *data;
		size_t length;
		int ret = known_hosts_page(&data, &length);
		if (ret) return ret;
		return parse_data(request, data, length);
	}
	if (!strcmp(request->url, "about:history")) {
		size_t length;
		char *data = show_history(request, &length);
		if (!data) return ERROR_MEMORY_FAILURE;
		return parse_data(request, data, length - 1);
	}
	if (!strcmp(request->url, "about:sandbox")) {
		return static_page(request, V(sandbox_page));
	}
	return ERROR_INVALID_URL;
}
