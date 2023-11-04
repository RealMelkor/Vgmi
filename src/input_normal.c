/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
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
#include "error.h"

void client_reset(struct client *client) {
	if (!client) return;
	client->g = client->count = 0;
}

int client_input_normal(struct client *client, struct tb_event ev) {
	switch (ev.key) {
	case TB_KEY_ESC:
		client->count = 0;
		break;
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
	case TB_KEY_TAB:
		{
			struct request *req;
			size_t i;
			char buf[MAX_URL];
			req = tab_completed(client->tab);
			i = client->count;
			client->count = 0;
			if (!req) break;
			if (!i && !req->selected) break;
			if (i) {
				if (req->text.links_count < i) {
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
			if (req->text.links_count < i) break;
			i = request_follow(req,
					req->text.links[i - 1], V(buf));
			if (i) {
				client->error = 1;
				error_string(i, V(client->cmd));
				break;
			}
			tab_request(client->tab, buf);
			break;
		}
	}
	if (ev.ch >= '0' && ev.ch <= '9') {
		client->count = client->count * 10 + ev.ch - '0';
		if (client->count > MAX_COUNT) client->count = MAX_COUNT;
	}
	switch (ev.ch) {
	case ':':
		client_enter_mode_cmdline(client);
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
		client->count = AZ(client->count);
		tab_scroll(client->tab, client->count,
				client_display_rect(client));
		client_reset(client);
		break;
	case 'k':
		client->count = AZ(client->count);
		tab_scroll(client->tab, -client->count,
				client_display_rect(client));
		client_reset(client);
		break;
	case 'g':
		if (!client->g) {
			client->g = 1;
			break;
		}
		tab_scroll(client->tab, -100000,
				client_display_rect(client));
		client_reset(client);
		break;
	case 'G':
		client->count = 0;
		tab_scroll(client->tab, 100000,
				client_display_rect(client));
		client_reset(client);
		break;
	case 'h':
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
	}
	return 0;
}
