#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "macro.h"
#include "gemini.h"
#include "gemtext.h"
#include "request.h"
#include "about.h"
#include "error.h"
#include "strlcpy.h"

static const char header[] = "20 text/gemini\r\n";

char newtab_page[] =
"20 text/gemini\r\n" \
"# Vgmi - " VERSION "\n\n" \
"A Gemini client written in C with vim-like keybindings\n\n" \
"=>gemini://geminispace.info Geminispace\n" \
"=> gemini://geminispace.info\n" \
"=>about:blank\n" \
"=>about:newtab\n" \
"=>about:history\n" \
"=>gemini://gemini.rmf-dev.com/repo/Vaati/Vgmi/readme Vgmi\n";

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
	int ret;
	request->status = GMI_SUCCESS;
	request->length = len;
	request->data = data;
	if ((ret = gemtext_links(request->data, request->length,
		&request->text.links, &request->text.links_count)))
		return ret;
	return gemini_status(request->data, request->length,
			V(request->meta), &request->status);
}

int about_parse(struct request *request) {
	if (!strcmp(request->url, "about:blank")) {
		request->status = GMI_SUCCESS;
		return 0;
	}
	if (!strcmp(request->url, "about:newtab")) {
		const size_t length = sizeof(newtab_page);
		char *data = malloc(length);
		if (!data) return ERROR_MEMORY_FAILURE;
		memcpy(data, newtab_page, length);
		return parse_data(request, data, length - 1);
	}
	if (!strcmp(request->url, "about:history")) {
		size_t length;
		char *data = show_history(request, &length);
		if (!data) return ERROR_MEMORY_FAILURE;
		return parse_data(request, data, length - 1);
	}
	return ERROR_INVALID_URL;
}
