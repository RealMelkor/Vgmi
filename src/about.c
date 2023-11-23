/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "macro.h"
#include "config.h"
#include "gemini.h"
#include "page.h"
#include "request.h"
#define ABOUT_INTERNAL
#include "about.h"
#include "error.h"
#include "strlcpy.h"
#include "memory.h"
#include "sandbox.h"
#include "parser.h"

char help_page[] =
HEADER \
"# Help\n\n" \
HELP_INFO;

char about_page[] =
HEADER \
"# List of \"about\" pages\n\n" \
"=>about:about\n" \
"=>about:blank\n" \
"=>about:bookmarks\n" \
"=>about:certificates\n" \
"=>about:config\n" \
"=>about:help\n" \
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

char sandbox_page[] =
HEADER \
"# Sandbox information\n\n" \
""SANDBOX_INFO"\n" \
"Filesystem access\t: "SANDBOX_FILESYSTEM"\n" \
"IPC access\t\t\t: "SANDBOX_IPC"\n" \
"Devices access\t\t: "SANDBOX_DEVICE"\n" \
"Parser isolation\t: "SANDBOX_PARSER"\n";

char sandbox_disabled_page[] =
HEADER \
"# Sandbox information\n\n" \
"The sandbox is disabled, enable it in about:config.\n" \
"Filesystem access\t: Unrestricted\n" \
"IPC access\t\t\t: Unrestricted\n" \
"Devices access\t\t: Unrestricted\n" \
"Parser isolation\t: Unrestricted\n";


void *dyn_strcat(char *dst, size_t *dst_length,
			const char *src, size_t src_len) {
	const size_t sum = (dst ? *dst_length : 0) + src_len + 2;
	void *ptr = realloc(dst, sum + 1);
	if (!ptr) return NULL;
	dst = ptr;
	strlcpy(&dst[*dst_length ? strnlen(dst, *dst_length) : 0], src, sum);
	*dst_length = strnlen(dst, sum);
	return dst;
}

static int parse_data_(struct request *request,
			char *data, size_t len, int status) {
	if (readonly(data, len, &request->data)) return ERROR_MEMORY_FAILURE;
	request->status = status;
	request->length = len;
	free(data);
	return parse_request(NULL, request);
}

static int parse_data(struct request *request, char *data, size_t len) {
	if (readonly(data, len, &request->data)) return ERROR_MEMORY_FAILURE;
	request->status = GMI_SUCCESS;
	request->length = len;
	free(data);
	return parse_request(NULL, request);
}

static int static_page(struct request *request, const char *data, size_t len) {
	if (readonly(data, len, &request->data)) return ERROR_MEMORY_FAILURE;
	request->status = GMI_SUCCESS;
	request->length = len - 1;
	return parse_request(NULL, request);
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
		if (ptr && (ret = about_bookmarks_param(param))) return ret;
		if ((ret = about_bookmarks(&data, &length))) return ret;
		return parse_data(request, data, length);
	}
	if (!strcmp(request->url, "about:certificates")) {
		if (ptr && (ret = about_certificates_param(param))) return ret;
		if ((ret = about_certificates(&data, &length))) return ret;
		return parse_data(request, data, length);
	}
	if (!strcmp(request->url, "about:help")) {
		return static_page(request, V(help_page));
	}
	if (!strcmp(request->url, "about:license")) {
		return static_page(request, V(license_page));
	}
	if (!strcmp(request->url, "about:newtab")) {
		if ((ret = about_newtab(&data, &length))) return ret;
		return parse_data(request, data, length);
	}
	if (!strcmp(request->url, "about:known-hosts")) {
		if (ptr && (ret = about_known_hosts_arg(param))) return ret;
		if ((ret = about_known_hosts(&data, &length))) return ret;
		return parse_data(request, data, length);
	}
	if (!strcmp(request->url, "about:config")) {
		int query = ptr ? (strchr(param, '?') != NULL) : 0;
		ret = ptr ? about_config_arg(param, &data, &length) :
				about_config(&data, &length);
		if (ret) return ret;
		if (ptr) {
			int len = strnlen(V(request->url));
			if (len + strnlen(V(param)) + 1 > sizeof(request->url))
				return ERROR_INVALID_ARGUMENT;
			request->url[len] = '/';
			strlcpy(&request->url[len + 1], param,
					sizeof(request->url) - len - 1);
		}
		/* reset url on succesful query */
		if (query) STRLCPY(request->url, "about:config");
		return parse_data_(request, data, length - 1,
					ptr ? GMI_INPUT : GMI_SUCCESS);
	}
	if (!strcmp(request->url, "about:history")) {
		if ((ret = about_history(&data, &length))) return ret;
		return parse_data(request, data, length - 1);
	}
	if (!strcmp(request->url, "about:sandbox")) {
		if (!config.enableSandbox) {
			return static_page(request, V(sandbox_disabled_page));
		}
		return static_page(request, V(sandbox_page));
	}
	return ERROR_INVALID_URL;
}
