/*
 * ISC License
 * Copyright (c) 2024 RMF <rawmonk@rmf-dev.com>
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
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
#include "utf8.h"

char help_page[] =
HEADER \
"# Help\n\n" \
HELP_INFO;

char about_page[] =
HEADER \
"# List of \"about\" pages\n\n" \
"=>about:about List of \"about\" pages\n" \
"=>about:blank Blank page\n" \
"=>about:bookmarks Bookmarks\n" \
"=>about:certificates Certificates\n" \
"=>about:config Configuration\n" \
"=>about:help Help\n" \
"=>about:history History\n" \
"=>about:known-hosts Known hosts\n" \
"=>about:license License\n" \
"=>about:newtab New tab page\n" \
"=>about:sandbox Sandbox information\n";

char license_page[] =
HEADER \
"# Vgmi license\n\n" \
"Copyright (c) 2024 RMF <rawmonk@rmf-dev.com>\n\n" \
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
"%s\n" \
"Filesystem access\t: %s\n" \
"IPC access\t\t\t: %s\n" \
"Devices access\t\t: %s\n" \
"Parser isolation\t: %s\n";

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
	size_t sum;
	void *ptr;
	if (src_len > SIZE_MAX - *dst_length) return NULL;
	sum = (*dst_length = strnlen(dst, *dst_length)) + src_len;
	ptr = realloc(dst, sum + 1);
	if (!ptr) return NULL;
	dst = ptr;
	strlcpy(&dst[*dst_length], src, src_len);
	*dst_length = strnlen(dst, sum);
	return dst;
}

static int parse_data_status(struct request *request,
			char *data, size_t len, int status) {
	if (readonly(data, len, &request->data)) return ERROR_MEMORY_FAILURE;
	request->status = status;
	request->length = len;
	free(data);
	return parse_request(NULL, request);
}

static int parse_data(struct request *request, char *data, size_t len) {
	return parse_data_status(request, data, len, GMI_SUCCESS);
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
		int query = ptr && strchr(param, '?');
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
		return parse_data_status(request, data, length - 1,
					ptr ? GMI_INPUT : GMI_SUCCESS);
	}
	if (!strcmp(request->url, "about:history")) {
		if (ptr && (ret = about_history_param(param))) return ret;
		if ((ret = about_history(&data, &length))) return ret;
		return parse_data(request, data, length - 1);
	}
	if (!strcmp(request->url, "about:sandbox")) {
		char buf[4096];
		size_t len;
		if (!config.enableSandbox) {
			return static_page(request, V(sandbox_disabled_page));
		}
		len = snprintf(V(buf), sandbox_page,
				SANDBOX_INFO,
#ifdef __linux__
				config.enableLandlock ?
					SANDBOX_FILESYSTEM : "Unrestricted",
#else
				SANDBOX_FILESYSTEM,
#endif
				SANDBOX_IPC,
				SANDBOX_DEVICE,
				SANDBOX_PARSER
			);
		return static_page(request, buf, len);
	}
	return ERROR_INVALID_URL;
}
