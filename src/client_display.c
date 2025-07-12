/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@rmf-dev.com>
 */
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include "termbox.h"
#include "macro.h"
#include "strscpy.h"
#include "page.h"
#include "request.h"
#include "client.h"
#include "tab.h"
#include "utf8.h"
#include "wcwidth.h"
#include "url.h"
#include "known_hosts.h"
#include "commands.h"

#define MULTIPLE_TABS(X) (X->tab->next || X->tab->prev)

struct rect client_display_rect(struct client *client) {
	struct rect rect = {0};
	rect.w = client->width - 2;
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

#define TB_REV (TB_REVERSE | TB_DEFAULT), TB_DEFAULT

void client_draw_scrollbar(struct client* client, struct request *request) {
	int y, h, i;
	struct rect rect;
	if (!client || !request) return;
	rect = client_display_rect(client);
	rect.h -= 2;
	if ((signed)request->page.length - rect.h <= 0) return;
	y = rect.h * request->scroll / (request->page.length - rect.h);
	h = (rect.h - 1) / (request->page.length - rect.h);
	h = h ? h : 1;
	if (y + h > rect.h) y = rect.h - h + 1;
	for (i = 0; i <= rect.h; i++) {
		tb_set_cell(client->width - 1, rect.y + i, ' ', TB_DEFAULT,
				(i >= y && i < y + h) ? TB_WHITE : TB_BLACK);
	}
}

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
			char buf[sizeof(req->page.title)] = {0};
			int fg = TB_REVERSE | TB_DEFAULT, bg = TB_DEFAULT;
			int offset, j;
			size_t length;
			struct request *req = tab_completed(tab);
			if (width == 1) {
				current--;
				if (client->width < current * 4) continue;
			}
			strcpy(buf, req ? req->page.title : "about:blank");
			i = j = 0;
			while (buf[i] && (size_t)i < sizeof(buf)) {
				uint32_t c;
				i += utf8_char_to_unicode(&c, &buf[i]);
				j += mk_wcwidth(c);
				if (j >= width) {
					buf[i] = 0;
					break;
				}
			}
			length = j;
			if (tab == client->tab) {
				fg = TB_DEFAULT;
				bg = TB_DEFAULT;
				for (i = x; i < x + width + 2; i++)
					tb_set_cell(i, 0, ' ', fg, bg);
			}
			tb_set_cell(x, 0, '[', fg, bg);
			tb_set_cell(x + width + 1, 0, ']', fg, bg);
			offset = (width + 2 - length) / 2;
			if (offset < 1) offset = 1;
			i = j = 0;
			while ((size_t)i < sizeof(buf) && buf[i]) {
				uint32_t c;
				int w;
				i += utf8_char_to_unicode(&c, &buf[i]);
				w = mk_wcwidth(c);
				if (j + w > width) break;
				tb_set_cell(x + j + offset, 0, c, fg, bg);
				j += w;
			}
			x += width + 2;
		}
	}

	for (i = 0; i < client->width; i++) 
		tb_set_cell(i, client->height - 2, ' ', TB_REV);

	if (client->error || client->exit) {
		int bg = client->error == ERROR_INFO ? TB_GREEN : TB_RED;
		int fg = TB_WHITE;
		if (client->exit) fg = bg= TB_DEFAULT;
		tb_print(0, client->height - 1, fg, bg, client->cmd);
	}

	if (client->mode == MODE_CMDLINE) {
		tb_print(0, client->height - 1, TB_DEFAULT, TB_DEFAULT,
				client->cmd);
		tb_set_cursor(utf8_width(client->cmd, client->cursor),
				client->height - 1);
	}

	if (client->count) {
		tb_printf(client->width - 10, client->height - 1,
				TB_DEFAULT, TB_DEFAULT, "%d", client->count);
	}

	req_input = tab_input(client->tab);
	if (req_input) {
		tb_printf(0, client->height - 2, TB_REV, "%s", req_input->url);
	} else if (client->mode == MODE_CMDLINE && client->tabcompletion) {
		int i, x, y;
		x = 0;
		y = client->height - 2;
		for (i = 0; i < (signed)LENGTH(client->matches); i++) {
			int id = client->matches[i], bg;
			if (id < 0 || id >= (signed)LENGTH(commands)) break;
			bg = client->tabcompletion_selected == i ?
				TB_YELLOW : TB_DEFAULT | TB_REVERSE;
			tb_printf(x, y, TB_DEFAULT, bg, commands[id].name);
			x += strnlen(V(commands[id].name)) + 2;
		}
	} else {
		int expired = 0, fg = TB_REVERSE | TB_DEFAULT, bg = TB_DEFAULT;
		char url[1024];
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
		if (req) url_hide_query(req->url, V(url));
		else STRSCPY(url, "about:blank");
		tb_printf(0, client->height - 2, fg, bg, "%s%s (%s)",
				expired ? "[Certificate expired] " : "",
				url, req ? req->meta : "");
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
	client_draw_scrollbar(client, req);
}
