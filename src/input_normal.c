/*
 * ISC License
 * Copyright (c) 2024 RMF <rawmonk@rmf-dev.com>
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "macro.h"
#include "strlcpy.h"
#include "utf8.h"
#include "termbox.h"
#include "page.h"
#include "request.h"
#include "client.h"
#include "tab.h"
#include "input.h"
#include "config.h"
#include "error.h"

void client_reset(struct client *client) {
	if (!client) return;
	client->exit = client->g = client->count = 0;
}

int client_input_normal(struct client *client, struct tb_event ev) {
	switch (ev.key) {
	case TB_KEY_ESC:
		client->count = 0;
		break;
	case TB_KEY_MOUSE_WHEEL_DOWN:
		if (!config.enableMouse) break;
		/* fallthrough */
	case TB_KEY_ARROW_DOWN:
		goto down;
	case TB_KEY_MOUSE_WHEEL_UP:
		if (!config.enableMouse) break;
		/* fallthrough */
	case TB_KEY_ARROW_UP:
		goto up;
	case TB_KEY_PGUP:
		client->count = AZ(client->count);
		tab_scroll(client->tab, -client->count * client->height,
				client_display_rect(client));
		client_reset(client);
		break;
	case TB_KEY_PGDN:
		client->count = AZ(client->count);
		tab_scroll(client->tab, client->count * client->height,
				client_display_rect(client));
		client_reset(client);
		break;
	case TB_KEY_CTRL_Q:
		if (!HAS_TABS(client)) return 1;
		client->exit = 1;
		break;
	case TB_KEY_CTRL_W:
		return client_closetab(client);
	case TB_KEY_ENTER:
		ev.ch = 'j';
		break;
	case TB_KEY_BACKSPACE:
	case TB_KEY_BACKSPACE2:
		goto prev;
	case TB_KEY_BACK_TAB:
	case TB_KEY_TAB:
		{
			char url[MAX_URL];
			struct request *req;
			size_t i;
			int ret;

			req = tab_completed(client->tab);
			i = client->count;
			client->count = 0;
			if (!req) break;
			if (!i && !req->selected) break;
			if (i) {
				if (req->page.links_count < i) {
					/* error */
					client->error = 1;
					STRLCPY(client->cmd,
							"Invalid link number");
					break;
				}
				req->selected = i;
				break;
			}
			i = req->selected;
			req->selected = 0;
			if (req->page.links_count < i) break;
			ret = request_follow(req,
					req->page.links[i - 1], V(url));
			if (ret) {
				client->error = 1;
				error_string(ret, V(client->cmd));
				break;
			}
			if (ev.key == TB_KEY_BACK_TAB) {
				client_newtab(client, url);
				break;
			}
			ret = tab_request(client->tab, url);
			if (ret) {
				client->error = 1;
				error_string(ret, V(client->cmd));
				break;
			}
			break;
		}
	}
	if (ev.ch >= '0' && ev.ch <= '9') {
		client->count = client->count * 10 + ev.ch - '0';
		if (client->count > MAX_COUNT) client->count = MAX_COUNT;
		return 0;
	}
	switch (ev.ch) {
	case ':':
		client_enter_mode_cmdline(client);
		break;
	case '/':
		{
			struct request *req = tab_completed(client->tab);
			if (!req) break;
			client_enter_mode_cmdline(client);
			client->cmd[0] = '/';
			client->search[0] = '\0';
			req->page.selected = 1;
		}
		break;
	case 'n':
	case 'N':
		{
			struct request *req = tab_completed(client->tab);
			if (!req) break;
			if (ev.ch == 'n') {
				if (req->page.selected < req->page.occurrences)
					req->page.selected++;
			} else {
				if (req->page.selected > 1)
					req->page.selected--;
			}
			req->scroll = page_selection_line(req->page);
			req->scroll -= client_display_rect(client).h / 2;
		}
		tab_scroll(client->tab, 0, client_display_rect(client));
		break;
	case 'u':
		{
			struct request *req = tab_completed(client->tab);
			client_enter_mode_cmdline(client);
			client->cursor = snprintf(V(client->cmd),
					":o %s", req ? req->url : "");
		}
		break;
	case 'j':
down:
		client->count = AZ(client->count);
		tab_scroll(client->tab, client->count,
				client_display_rect(client));
		client_reset(client);
		break;
	case 'k':
up:
		client->count = AZ(client->count);
		tab_scroll(client->tab, -client->count,
				client_display_rect(client));
		client_reset(client);
		break;
	case 't':
		if (!client->g) break;
		if (client->tab->next)
			client->tab = client->tab->next;
		else while (client->tab->prev)
			client->tab = client->tab->prev;
		client_reset(client);
		break;
	case 'T':
		if (!client->g) break;
		if (client->tab->prev)
			client->tab = client->tab->prev;
		else while (client->tab->next)
			client->tab = client->tab->next;
		client_reset(client);
		break;
	case 'g':
		if (!client->g) {
			client->g = 1;
			break;
		}
		tab_scroll(client->tab, -0x0FFFFFFF,
				client_display_rect(client));
		client_reset(client);
		break;
	case 'G':
		client->count = 0;
		tab_scroll(client->tab, 0x0FFFFFFF,
				client_display_rect(client));
		client_reset(client);
		break;
	case 'h':
prev:
		if (!client || !client->tab || !client->tab->request) break;
		{
			struct request *req = client->tab->view ?
				client->tab->view->next :
				client->tab->request->next;
			while (req && req->state != STATE_COMPLETED) {
				req = req->next;
			}
			if (req) client->tab->view = req;
		}
		break;
	case 'l':
		if (!client || !client->tab || !client->tab->request) break;
		if (!client->tab->view) break;
		{
			struct request *req, *prev;
			prev = NULL;
			req = client->tab->request->next;
			while (req && req != client->tab->view) {
				if (req->state == STATE_COMPLETED)
					prev = req;
				req = req->next;
			}
			client->tab->view = prev;
		}
		break;
	case 'r':
		{
			struct request *req = tab_completed(client->tab);
			if (!req) break;
			tab_request(client->tab, req->url);
		}
		break;
	case 'b':
		client_newtab(client, "about:bookmarks");
		break;
	case 'f':
		client_newtab(client, "about:history");
		break;
	case 'y':
		if (client->exit) return 1;
		break;
	}
	return 0;
}
