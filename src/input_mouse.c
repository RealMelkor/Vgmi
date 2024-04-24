/*
 * ISC License
 * Copyright (c) 2024 RMF <rawmonk@rmf-dev.com>
 */
#include <stdio.h>
#include <stdint.h>
#include "macro.h"
#include "termbox.h"
#include "page.h"
#include "request.h"
#include "client.h"
#include "tab.h"
#include "error.h"

void click_tab(struct client *client, struct tb_event ev) {
	struct tab *start, *tab;
	int count, x, width;

	for (tab = client->tab; tab->prev; tab = tab->prev) ;
	count = 0;
	for (start = tab; start; start = start->next) count++;
	width = (client->width - count * 2) / count;
	if (width > TAB_WIDTH) width = TAB_WIDTH;
	if (width < 1) width = 1;

	x = 0;
	for (; tab; tab = tab->next) {
		if (x <= ev.x && x + width + 1 >= ev.x) break;
		x += width + 2;
	}
	if (tab) client->tab = tab;
}

void click_link(struct client *client, struct tb_event ev, int newtab) {
	int ret;
	const char *link;
	struct request *req;
	if (!(client && client->tab && client->tab->request)) return;
	req = client->tab->view;
	if (!req) req = client->tab->request;
	ret = page_link_line(req->page,
			req->scroll + ev.y - 1 - HAS_TABS(client), ev.x);
	if (!ret) return;
	ret--;
	link = req->page.links[ret];
	ret = newtab ? client_newtab(client, link) :
			tab_follow(client->tab, link);
	if (ret) {
		client->error = 1;
		error_string(ret, V(client->cmd));
	}
	return;
}

int client_input_mouse(struct client *client, struct tb_event ev) {
	if (ev.type != TB_EVENT_MOUSE) return 0;
	switch (ev.key) {
	case TB_KEY_MOUSE_LEFT:
	case TB_KEY_MOUSE_RIGHT:
		if (ev.y == 0 && HAS_TABS(client)) {
			click_tab(client, ev);
			break;
		}
		/* fallthrough */
	case TB_KEY_MOUSE_MIDDLE:
		click_link(client, ev, ev.key == TB_KEY_MOUSE_MIDDLE);
		break;
	}
	return 0;
}
