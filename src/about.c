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
#include "page.h"
#include "request.h"
#include "about.h"
#include "error.h"
#include "strlcpy.h"
#include "memory.h"
#define KNOWN_HOSTS_INTERNAL
#include "known_hosts.h"
#include "sandbox.h"
#include "parser.h"

#define HEADER "20 text/gemini\r\n"
#define HELP_INFO \
"## Keybindings\n\n" \
"* k - Scroll up\n" \
"* j - Scroll up\n" \
"* gT - Switch to the previous tab\n" \
"* gt - Switch to the next tab\n" \
"* h - Go back in history\n" \
"* l - Go forward in history\n" \
"* gg - Go at the top of the page\n" \
"* G - Go at the bottom of the page\n" \
"* / - Open search mode\n" \
"* : - Open input mode\n" \
"* u - Open input mode with the current url\n" \
"* b - Open about:bookmarks in a new tab\n" \
"* f - Open about:history in a new tab\n" \
"* r - Reload the page\n" \
"* [number]Tab - Scroll up\n" \
"* Tab - Follow the selected link\n" \
"* Shift+Tab - Open the selected link in a new tab\n\n" \
"You can prefix a movement key with a number to repeat it.\n\n" \
"## Commands\n\n" \
"* :q - Close the current tab\n" \
"* :qa - Close all tabs, exit the program\n" \
"* :o [url] - Open an url\n" \
"* :s [search] - Search the Geminispace using geminispace.info\n" \
"* :nt [url] - Open a new tab, the url is optional\n" \
"* :add [name] - Add the current url to the bookmarks, the name is optional\n"\
"* :[number] - Scroll to the line number\n" \
"* :gencert - Generate a certificate for the current capsule\n" \
"* :forget <host> - Forget the certificate for an host\n" \
"* :download [name] - Download the current page, the name is optional\n" \
"* :help - Open about:help in a new tab\n" \

static const char header[] = HEADER;

char about_page[] =
HEADER \
"# List of \"about\" pages\n\n" \
"=>about:about\n" \
"=>about:blank\n" \
"=>about:bookmarks\n" \
"=>about:certificate\n" \
"=>about:downloads\n" \
"=>about:history\n" \
"=>about:known-hosts\n" \
"=>about:license\n" \
"=>about:newtab\n" \
"=>about:sandbox\n";

char help_page[] =
HEADER \
"# Help\n\n" \
HELP_INFO;

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

char newtab_page_header[] =
HEADER \
"# Vgmi - " VERSION "\n\n" \
"A Gemini client written in C with vim-like keybindings\n\n## Bookmarks\n\n";

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
	char *ptr;
	if (readonly(data, len, &ptr)) return ERROR_MEMORY_FAILURE;
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
			"> From: %s\n> Expiration: %s\n"
			"=>/%ld Forget\n\n",
			known_hosts[i].host, known_hosts[i].hash,
			from, to, i) + 1;
		tmp = realloc(data, length + len);
		if (!tmp) {
			free(data);
			return ERROR_MEMORY_FAILURE;
		}
		data = tmp;
		strlcpy(&data[length], buf, len);
		length += len - 1;
	}

	readonly(data, length, out);
	free(data);
	*length_out = length;
	return 0;
}

#include "bookmarks.h"
static int bookmarks_page(char **out, size_t *length_out) {

	char *data = NULL;
	size_t length = 0;
	size_t i;
	const char title[] = "# Bookmarks\n\n";

	length = sizeof(header) + sizeof(title);
	data = malloc(length);
	if (!data) return ERROR_MEMORY_FAILURE;
	strlcpy(data, header, length);
	strlcpy(&data[sizeof(header) - 1], title, length - sizeof(header));
	length -= 2;

	for (i = 0; i < bookmark_length; i++) {

		char buf[1024];
		int len;
		void *tmp;

		len = snprintf(V(buf), "=>%s %s\n=>/%ld Delete\n\n",
				bookmarks[i].url, bookmarks[i].name, i) + 1;
		tmp = realloc(data, length + len);
		if (!tmp) {
			free(data);
			return ERROR_MEMORY_FAILURE;
		}
		data = tmp;
		strlcpy(&data[length], buf, len);
		length += len - 1;
	}

	readonly(data, length, out);
	free(data);
	*length_out = length;
	return 0;
}

int newtab_page(char **out, size_t *length_out) {
	void *ptr;
	size_t length = sizeof(newtab_page_header), i;
	char *data = malloc(length);
	if (!data) return ERROR_MEMORY_FAILURE;
	strlcpy(data, newtab_page_header, length);
	for (i = 0; i < bookmark_length; i++) {
		char line[sizeof(struct bookmark) + 8];
		size_t line_length;
		line_length = snprintf(V(line), "=>%s %s\n",
				bookmarks[i].url, bookmarks[i].name);
		ptr = realloc(data, length + line_length + 1);
		if (!ptr) {
			free(data);
			return ERROR_MEMORY_FAILURE;
		}
		data = ptr;
		strlcpy(&data[length - 1], line, line_length + 1);
		length += line_length;
	}
	ptr = realloc(data, length + sizeof(HELP_INFO));
	if (!ptr) {
		free(data);
		return ERROR_MEMORY_FAILURE;
	}
	data = ptr;
	data[length - 1] = '\n';
	strlcpy(&data[length], HELP_INFO, sizeof(HELP_INFO));
	length += sizeof(HELP_INFO);
	*out = data;
	*length_out = length;
	return 0;
}

int about_parse(struct request *request) {

	char param[MAX_URL];
	char *ptr = strchr(request->url, '/');
	char *data;
	size_t length;
	int ret;

	if (ptr) {
		*ptr = 0;
		ptr++;
		STRLCPY(param, ptr);
	} else param[0] = 0;
	if (!strcmp(request->url, "about:about")) {
		return static_page(request, V(about_page));
	}
	if (!strcmp(request->url, "about:blank")) {
		request->status = GMI_SUCCESS;
		STRLCPY(request->page.title, "about:blank");
		return 0;
	}
	if (!strcmp(request->url, "about:bookmarks")) {
		if (ptr) {
			int id = atoi(param);
			if (!id && STRCMP(param, "0"))
				return ERROR_INVALID_ARGUMENT;
			if ((ret = bookmark_remove(id))) return ret;
			if ((ret = bookmark_rewrite())) return ret;
		}
		if ((ret = bookmarks_page(&data, &length))) return ret;
		return parse_data(request, data, length);
	}
	if (!strcmp(request->url, "about:help")) {
		return static_page(request, V(help_page));
	}
	if (!strcmp(request->url, "about:license")) {
		return static_page(request, V(license_page));
	}
	if (!strcmp(request->url, "about:newtab")) {
		if ((ret = newtab_page(&data, &length))) return ret;
		return parse_data(request, data, length);
	}
	if (!strcmp(request->url, "about:known-hosts")) {
		if (ptr) {
			int id = atoi(param);
			if (!id && STRCMP(param, "0"))
				return ERROR_INVALID_ARGUMENT;
			if ((ret = known_hosts_forget_id(id))) return ret;
			if ((ret = known_hosts_rewrite())) return ret;
		}
		if ((ret = known_hosts_page(&data, &length))) return ret;
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
