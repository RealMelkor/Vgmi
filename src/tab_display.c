/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include "termbox.h"
#include "macro.h"
#include "client.h"
#include "gemini.h"
#include "page.h"
#include "request.h"
#include "tab.h"
#include "strnstr.h"
#include "error.h"
#include "parser.h"
#include "known_hosts.h"

void tab_clean_requests(struct tab *tab);

void tab_display_loading(struct tab *tab, struct rect rect) {
	int off[][2] = {
		{0, 0}, {1, 0}, {2, 0}, {2, 1}, {2, 2}, {1, 2}, {0, 2}, {0, 1}
	};
	int x = rect.x + rect.w - 4, y = rect.y + rect.h - 4, i;
	struct timespec now;
	tab->loading = (tab->loading + 1) % (sizeof(off) / sizeof(*off));

	clock_gettime(CLOCK_MONOTONIC, &now);
	tab->loading = (now.tv_sec * 10 + now.tv_nsec / 100000000) %
			(sizeof(off) / sizeof(*off));

	for (i = 0; i < (int)(sizeof(off) / sizeof(*off)); i++)
		tb_set_cell(x + off[i][0], y + off[i][1], ' ', TB_WHITE,
			i != tab->loading ? TB_WHITE : TB_BLUE);
}

void tab_display_update(struct request *req, struct rect rect) {
	int error = parse_page(NULL, req, rect.w - rect.x);
	if (error) {
		req->error = error;
		return;
	}
	request_scroll(req, 0, rect); /* fix scroll */
	return;
}

void tab_display_gemtext(struct request *req, struct rect rect) {

	if (!req) return;

	if (req->page.width != rect.w - rect.x) {
		/* TODO: should be done in the background */
		tab_display_update(req, rect);
	}
	page_display(req->page, req->scroll, rect, req->selected);
}

void tab_display_error(struct tab *tab) {
	if (!tab) return;
	tab->request->state = STATE_ENDED;
	tb_refresh();
	tab_clean_requests(tab);
}

void tab_display_request(struct request *req, struct rect rect) {
	if (!req) return;
	switch (req->status) {
	case GMI_SUCCESS:
		tab_display_gemtext(req, rect);
		break;
	case GMI_INPUT:
		break;
	default:
		tab_display_gemtext(req, rect);
		break;
	}
}

void tab_display(struct tab *tab, struct client *client) {

	struct rect rect;
	struct request *req;

	if (!tab || !tab->request) return;

	rect = client_display_rect(client);

	req = tab_completed(tab);
	if (req) tab_display_request(req, rect);

	if (client->mode != MODE_CMDLINE && tab->failure) {
		tab->failure = 0;
		client->error = 1;
		snprintf(V(client->cmd), "%s", tab->error);
	}

	switch (tab->request->state) {
	case STATE_COMPLETED:
		tab_display_request(tab->view ?
				tab->view : tab->request, rect);
		break;
	case STATE_FAILED:
		tab_display_error(tab);
		break;
	case STATE_ONGOING:
		tab_display_loading(tab, rect);
		break;
	}

}
