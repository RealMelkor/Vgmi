/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include "termbox.h"
#include "macro.h"
#include "strlcpy.h"
#include "page.h"
#include "request.h"
#include "client.h"
#include "tab.h"
#include "utf8.h"
#include "known_hosts.h"

#define MULTIPLE_TABS(X) (X->tab->next || X->tab->prev)

struct rect client_display_rect(struct client *client) {
	struct rect rect = {0};
	rect.w = client->width;
	rect.y = MULTIPLE_TABS(client);
	rect.h = client->height - 1 - rect.y;
	return rect;
}

void client_display(struct client* client) {
	if (!client) return;
	tb_clear();
	tab_display(client->tab, client);
	client_draw(client);
	tb_present();
}

#define TAB_WIDTH 32
#define TB_REV (TB_REVERSE | TB_DEFAULT), TB_DEFAULT

void client_draw(struct client* client) {

	int i;
	struct request *req, *req_input;

	if (!client) return;

	req = tab_completed(client->tab);
	/* reset counter and error when a new page is loaded */
	if (req != client->last_request) {
		client->count = 0;
		client->error = 0;
	}
	client->last_request = req;

	client->width = tb_width();
	client->height = tb_height();

	if (MULTIPLE_TABS(client)) {
		struct tab *tab, *start;
		int x, width;
		int count, current;
		for (i = 0; i < client->width; i++) {
			tb_set_cell(i, 0, ' ', TB_REV);
		}
		for (tab = client->tab; tab->prev; tab = tab->prev) ;
		current = count = 0;
		for (start = tab; start; start = start->next) {
			if (client->tab == start) current = count;
			count++;
		}
		x = 0;
		width = (client->width - count * 2) / count;
		if (width > TAB_WIDTH) width = TAB_WIDTH;
		if (width < 1) width = 1;
		for (; tab; tab = tab->next) {
			char buf[TAB_WIDTH];
			int fg = TB_REVERSE | TB_DEFAULT, bg = TB_DEFAULT;
			size_t length;
			struct request *req = tab_completed(tab);
			if (width == 1) {
				current--;
				if (client->width < current * 4) continue;
			}
			strlcpy(buf, req ? req->page.title : "about:blank",
					width);
			length = strnlen(V(buf));
			if (tab == client->tab) {
				fg = TB_DEFAULT;
				bg = TB_DEFAULT;
				for (i = x; i < x + width + 2; i++)
					tb_set_cell(i, 0, ' ', fg, bg);
			}
			tb_set_cell(x, 0, '[', fg, bg);
			tb_set_cell(x + width + 1, 0, ']', fg, bg);
			tb_printf(x + (width + 2 - length) / 2, 0,
					fg, bg, "%s", buf);
			x += width + 2;
		}
	}

	for (i = 0; i < client->width; i++) 
		tb_set_cell(i, client->height - 2, ' ', TB_REV);

	if (client->error) {
		int color = client->error == ERROR_INFO ? TB_GREEN : TB_RED;
		tb_print(0, client->height - 1, TB_WHITE, color, client->cmd);
	}

	if (client->mode == MODE_CMDLINE) {
		tb_print(0, client->height - 1, TB_DEFAULT, TB_DEFAULT,
				client->cmd);
		tb_set_cursor(utf8_width(client->cmd, sizeof(client->cmd)),
				client->height - 1);
	}

	if (client->count) {
		tb_printf(client->width - 10, client->height - 1,
				TB_DEFAULT, TB_DEFAULT, "%d", client->count);
	}

	req_input = tab_input(client->tab);
	if (req_input) {
		tb_printf(0, client->height - 2, TB_REV, "%s", req_input->url);
	} else {
		int expired = 0, fg = TB_REVERSE | TB_DEFAULT, bg = TB_DEFAULT;
		if (req && known_hosts_expired(req->name) > 0) {
			int i;
			expired = 1;
			fg = TB_WHITE;
			bg = TB_RED;
			for (i = 0; i < client->width; i++) {
				tb_set_cell(
					i, client->height - 2, ' ', fg, bg);
			}
		}
		tb_printf(0, client->height - 2, fg, bg, "%s%s (%s)",
				expired ? "[Certificate expired] " : "",
				req ? req->url : "about:blank",
				req ? req->meta : "");
	}

	if (req && req->selected &&
			(size_t)req->selected <= req->page.links_count) {
		const char *link = req->page.links[req->selected - 1];
		const char format[] = " => %s ";
		int len = utf8_width(link, MAX_URL) + sizeof(format) - 3;
		int x = client->width - len;
		if (x < 10) x = 10;
		tb_printf(x, client->height - 2, TB_WHITE, TB_BLUE,
				format, link);
	}
}
