/*
 * ISC License
 * Copyright (c) 2024 RMF <rawmonk@rmf-dev.com>
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "macro.h"
#include "termbox.h"
#include "page.h"
#include "request.h"
#include "client.h"
#include "tab.h"
#include "error.h"
#include "input.h"

void click_url(struct client *client, struct tb_event ev) {
	struct request *req = tab_completed(client->tab);
	if (!req) return;
	if ((size_t)ev.x > STRLEN(req->url)) return;
	client_enter_mode_cmdline(client);
	snprintf(V(client->cmd), ":o %s", req ? req->url : "");
	client->cursor = ev.x + 3;
}

int click_tab(struct client *client, struct tb_event ev, int close) {
	struct tab *start, *tab, *ret;
	int count, x, width;

	for (tab = client->tab; tab && tab->prev; tab = tab->prev) ;
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
	if (!tab) return 0;
	if (!close) {
		client_reset_mode(client);
		client->tab = tab;
		return 0;
	}
	ret = tab_close(tab);
	if (tab == client->tab) {
		client_reset(client);
		client->tab = ret;
	}
	return ret == NULL;
}

void click_link(struct client *client, struct tb_event ev, int newtab) {
	int ret;
	const char *link;
	struct request *req;
	if (!(client && client->tab && client->tab->request)) return;
	req = tab_completed(client->tab);
	if (!req) return;
	ret = page_link_line(req->page,
			req->scroll + ev.y - 1 - HAS_TABS(client), ev.x);
	if (!ret) return;
	if (!newtab && ret != req->selected) {
		req->selected = ret;
		return;
	}
	client_reset(client);
	client->mode = MODE_NORMAL;
	tb_hide_cursor();
	ret--;
	link = req->page.links[ret];
	ret = newtab ? client_newtab(client, link, 1) :
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
	case TB_KEY_MOUSE_MIDDLE:
	case TB_KEY_MOUSE_RELEASE:
		if (ev.mod & TB_MOD_MOTION) break;
		if (ev.y == client->height - 2) {
			click_url(client, ev);
			return 0;
		}
		if (ev.y == 0 && HAS_TABS(client)) {
			return click_tab(client, ev,
					ev.key == TB_KEY_MOUSE_MIDDLE);
		}
		click_link(client, ev, ev.key == TB_KEY_MOUSE_MIDDLE);
		break;
	}
	return 0;
}
