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
#include "gemtext.h"
#include "request.h"
#include "tab.h"
#include "strnstr.h"
#include "error.h"

void tab_clean_requests(struct tab *tab);

void tab_display_loading(struct tab *tab, struct rect rect) {
	int off[][2] = {
		{0, 0}, {1, 0}, {2, 0}, {2, 1}, {2, 2}, {1, 2}, {0, 2}, {0, 1}
	};
	int x = rect.x + rect.w - 4, y = rect.y + rect.h - 4, i;
	struct timespec now;
	tab->loading = (tab->loading + 1) % (sizeof(off) / sizeof(*off));

	clock_gettime(CLOCK_MONOTONIC_RAW, &now);
	tab->loading = (now.tv_sec * 10 + now.tv_nsec / 100000000) %
			(sizeof(off) / sizeof(*off));

	for (i = 0; i < (int)(sizeof(off) / sizeof(*off)); i++)
		tb_set_cell(x + off[i][0], y + off[i][1], ' ', TB_WHITE,
			i != tab->loading ? TB_WHITE : TB_BLUE);
}

void tab_display_gemtext(struct request *req, struct rect rect) {

	char *data, *start;

	if (!req) return;

	data = req->data;
	start = strnstr(data, "\r\n", req->length);
	if (!start) return;

	if (req->text.width != rect.w - rect.x) {
		int error = gemtext_parse(req->data, req->length,
				rect.w - rect.x, &req->text);
		if (error) {
			req->error = error;
			return;
		}
		request_scroll(req, 0, rect); /* fix scroll */
	}
	gemtext_display(req->text, req->scroll,
			rect.h - rect.y, req->selected);
}

void tab_display_error(struct tab *tab, struct client *client) {
	if (!tab) return;
	if (tab->request->status > 0) {
		snprintf(V(client->cmd), "%d: %s",
				tab->request->status, tab->error);
	} else {
		snprintf(V(client->cmd), "%s: %s",
				tab->error, tab->request->url);
	}
	client->error = 1;
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

	switch (tab->request->state) {
	case STATE_COMPLETED:
		tab_display_request(tab->view ?
				tab->view : tab->request, rect);
		break;
	case STATE_FAILED:
		tab_display_error(tab, client);
		break;
	case STATE_ONGOING:
		tab_display_loading(tab, rect);
		break;
	}
}
