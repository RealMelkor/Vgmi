/*
 * ISC License
 * Copyright (c) 2024 RMF <rawmonk@rmf-dev.com>
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include "macro.h"
#include "strscpy.h"
#include "utf8.h"
#include "termbox.h"
#include "page.h"
#include "request.h"
#include "client.h"
#include "tab.h"
#include "input.h"
#include "gemini.h"
#include "config.h"
#include "error.h"

void client_reset(struct client *client) {
	if (!client) return;
	memset(client->cmd, 0, sizeof(client->cmd));
	client->tabcompletion = client->exit = client->g = client->count = 0;
}

void client_reset_mode(struct client *client) {
	struct request *req = tab_input(client->tab);
	if (!req || (req->status != GMI_INPUT && req->status != GMI_SECRET))
		return;
	req->state = STATE_ENDED;
	client_reset(client);
	client->mode = MODE_NORMAL;
	tb_hide_cursor();
}

void move_tab_left(struct client *client) {
	struct tab *current, *prev;
	if (!client->tab || !HAS_TABS(client) || !client->tab->prev) return;

	current = client->tab;
	prev = client->tab->prev;

	current->prev = prev->prev;
	if (current->next) current->next->prev = prev;
	prev->next = current->next;
	current->next = prev;
	prev->prev = current;
	if (current->prev) current->prev->next = current;
}

void move_tab_right(struct client *client) {
	struct tab *current, *next;
	if (!client->tab || !HAS_TABS(client) || !client->tab->next) return;

	current = client->tab;
	next = client->tab->next;

	current->next = next->next;
	if (current->prev) current->prev->next = next;
	next->prev = current->prev;
	current->prev = next;
	next->next = current;
}

int client_input_normal(struct client *client, struct tb_event ev) {
	switch (ev.key) {
	case TB_KEY_ESC:
		client_reset(client);
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
	case TB_KEY_ARROW_LEFT:
		if (ev.mod & TB_MOD_ALT) goto prev;
		break;
	case TB_KEY_ARROW_RIGHT:
		if (ev.mod & TB_MOD_ALT) goto next;
		break;
	case TB_KEY_HOME:
		goto top;
	case TB_KEY_END:
		goto bottom;
	case TB_KEY_PGUP:
		if (ev.mod & TB_MOD_CTRL) {
			if (ev.mod & TB_MOD_SHIFT) {
				move_tab_left(client);
				break;
			}
			goto tabprev;
		}
		client->count = AZ(client->count);
		tab_scroll(client->tab, -client->count * client->height,
				client_display_rect(client));
		client_reset(client);
		break;
	case TB_KEY_PGDN:
		if (ev.mod & TB_MOD_CTRL) {
			if (ev.mod & TB_MOD_SHIFT) {
				move_tab_right(client);
				break;
			}
			goto tabnext;
		}
		client->count = AZ(client->count);
		tab_scroll(client->tab, client->count * client->height,
				client_display_rect(client));
		client_reset(client);
		break;
	case TB_KEY_CTRL_Q:
		if (!HAS_TABS(client)) return 1;
		STRSCPY(client->cmd, "Press 'y' to exit.");
		client->exit = 1;
		break;
	case TB_KEY_CTRL_B:
		client_newtab(client, "about:bookmarks", 0);
		break;
	case TB_KEY_CTRL_H:
		client_newtab(client, "about:history", 0);
		break;
	case TB_KEY_CTRL_W:
		return client_closetab(client);
	case TB_KEY_CTRL_T:
		return client_newtab(client, NULL, 0);
	case TB_KEY_CTRL_R:
	case TB_KEY_F5:
		{
			struct request *req = tab_completed(client->tab);
			if (!req) break;
			tab_request(client->tab, req->url);
		}
		break;
	case TB_KEY_ENTER:
		ev.ch = 'j';
		break;
	case TB_KEY_CTRL_F:
		goto search;
	case TB_KEY_CTRL_G:
		goto nextsearch;
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
					STRSCPY(client->cmd,
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
				client_newtab(client, url, 0);
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
search:
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
nextsearch:
	case 'n':
	case 'N':
		{
			struct request *req = tab_completed(client->tab);
			if (!req) break;
			if (ev.ch != 'N') {
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
tabnext:
		if (client->tab->next)
			client->tab = client->tab->next;
		else while (client->tab->prev)
			client->tab = client->tab->prev;
		client_reset(client);
		break;
	case 'T':
		if (!client->g) break;
tabprev:
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
top:
		tab_scroll(client->tab, -0x0FFFFFFF,
				client_display_rect(client));
		client_reset(client);
		break;
	case 'G':
bottom:
		client->count = 0;
		tab_scroll(client->tab, 0x0FFFFFFF,
				client_display_rect(client));
		client_reset(client);
		break;
	case 'h':
prev:
		if (!client || !client->tab || !client->tab->request) break;
		{
			struct request *req;
			pthread_mutex_lock(client->tab->mutex);
			req = client->tab->view ?
				client->tab->view->next :
				client->tab->request->next;
			while (req && req->state != STATE_COMPLETED) {
				req = req->next;
			}
			if (req) client->tab->view = req;
			pthread_mutex_unlock(client->tab->mutex);
		}
		break;
	case 'l':
next:
		if (!client || !client->tab || !client->tab->request) break;
		if (!client->tab->view) break;
		{
			struct request *req, *prev;
			pthread_mutex_lock(client->tab->mutex);
			prev = NULL;
			req = client->tab->request->next;
			while (req && req != client->tab->view) {
				if (req->state == STATE_COMPLETED)
					prev = req;
				req = req->next;
			}
			client->tab->view = prev;
			pthread_mutex_unlock(client->tab->mutex);
		}
		break;
	case 'y':
		if (client->exit) return 1;
		break;
	}
	return 0;
}
