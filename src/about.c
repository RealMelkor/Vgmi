#include <stdlib.h>
#include <string.h>
#include "macro.h"
#include "gemini.h"
#include "gemtext.h"
#include "request.h"
#include "about.h"
#include "error.h"

#include <stdint.h>
#include "utf8.h"
#include "termbox.h"

char newtab_page[] =
"20 text/gemini\r\n" \
"# Vgmi - " VERSION "\n\n" \
"A Gemini client written in C with vim-like keybindings\n\n" \
"=>gemini://geminispace.info Geminispace\n" \
"=>gemini://gemini.rmf-dev.com/repo/Vaati/Vgmi/readme Vgmi\n";

int about_parse(struct request *request) {
	if (!strcmp(request->url, "about:blank")) {
		request->status = GMI_SUCCESS;
		return 0;
	}
	if (!strcmp(request->url, "about:newtab")) {
		int ret;
		request->status = GMI_SUCCESS;
		request->length = sizeof(newtab_page);
		request->data = malloc(request->length);
		if (!request->data) return ERROR_MEMORY_FAILURE;
		memcpy(request->data, newtab_page, request->length);
		if ((ret = gemtext_links(request->data, request->length,
			&request->text.links, &request->text.links_count)))
			return ret;
		return gemini_status(request->data, request->length,
				V(request->meta), &request->status);
	}
	return ERROR_INVALID_URL;
}
