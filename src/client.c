/*
 * ISC License
 * Copyright (c) 2024 RMF <rawmonk@rmf-dev.com>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <errno.h>
#include "macro.h"
#include "error.h"
#include "config.h"
#include "gemini.h"
#include "page.h"
#include "request.h"
#include "tab.h"
#include "client.h"
#include "termbox.h"
#include "utf8.h"
#include "input.h"
#include "strlcpy.h"
#include "known_hosts.h"
#include "storage.h"
#include "sandbox.h"
#include "parser.h"
#include "bookmarks.h"
#include "image.h"
#include "history.h"
#include "xdg.h"
#include "url.h"
#include "proc.h"

int client_destroy(struct client *client) {
	struct tab *tab;
	if (!client) return ERROR_NULL_ARGUMENT;
	for (tab = client->tab; tab && tab->prev; tab = tab->prev) ;
	while (tab) {
		struct tab *next = tab->next;
		tab_free(tab);
		tab = next;
	}
	known_hosts_free();
	history_save();
	history_free();
	config_save();
	free(bookmarks);
	if (tb_shutdown()) return ERROR_TERMBOX_FAILURE;
	return 0;
}

int client_closetab(struct client *client) {
	if (!client->tab) return 1;
	client->tab = tab_close(client->tab);
	return client->tab == NULL;
}

int client_newtab(struct client *client, const char *url) {
	struct tab *tab;
	int proto;
	if (!url) url = "about:newtab";
	proto = protocol_from_url(url);
	switch (proto) {
	case PROTOCOL_GEMINI: case PROTOCOL_INTERNAL: case PROTOCOL_NONE:
		break;
	default:
		return tab_request(client->tab, url);
	}
	tab = tab_new();
	if (!tab) return ERROR_MEMORY_FAILURE;
	if (client->tab) {
		tab->next = client->tab->next;
		if (tab->next) tab->next->prev = tab;
		tab->prev = client->tab;
		client->tab->next = tab;
		if (client->tab->request && proto == PROTOCOL_NONE) {
			struct request req = {0};
			STRLCPY(req.url, client->tab->request->url);
			request_follow(&req, url, V(req.url));
			url = req.url;
		}
	}
	client->tab = tab;
	return tab_request(client->tab, url);
}

int client_input(struct client *client) {

	struct tb_event ev;
	struct request *req = NULL;
	int ret = 0;

	if (!client->tab || !client->tab->request ||
			client->tab->request->state != STATE_ONGOING) {
#ifdef FUZZING_MODE
		ret = tb_peek_event(&ev, 50);
		if (ret == TB_ERR_NO_EVENT) {
			ret = TB_OK;
			ev.ch = 'r';
		}
#else
		ret = tb_poll_event(&ev);
#endif
	} else {
		ret = tb_peek_event(&ev, 100);
	}

	if (ret == TB_ERR_NO_EVENT) return 0;
	if (ret == TB_ERR_POLL) ret = TB_OK;
	if (ret != TB_OK) return ERROR_TERMBOX_FAILURE;

	if (client->tab)
		req = tab_input(client->tab);

	if (req && gemini_isinput(req->status) && client->mode == MODE_NORMAL) {
		client_enter_mode_cmdline(client);
		client->cursor = snprintf(V(client->cmd), "%s: ", req->meta);
	}

	if (client_input_mouse(client, ev)) return -1;
	switch (client->mode) {
	case MODE_NORMAL:
		return client_input_normal(client, ev);
	case MODE_CMDLINE:
		{
			int ret = client_input_cmdline(client, ev);
			if (client->mode == MODE_NORMAL)
				tb_hide_cursor();
			return ret;
		}
	}
	
	return 0;
}

int client_init(struct client* client) {

	int ret;

	memset(client, 0, sizeof(*client));
	if ((ret = storage_init())) return ret;
	config_load();
	if ((ret = parser_request_create())) return ret;
	if ((ret = parser_page_create())) return ret;
#ifdef ENABLE_IMAGE
	if ((ret = image_init())) return ret;
#endif
#ifndef DISABLE_XDG
	if (xdg_available()) if ((ret = xdg_init())) return ret;
#endif
	if ((ret = known_hosts_load())) return ret;
	if ((ret = bookmark_load())) return ret;
	if (tb_init()) return ERROR_TERMBOX_FAILURE;
	if (tb_set_input_mode(TB_INPUT_ESC | TB_INPUT_MOUSE))
		return ERROR_TERMBOX_FAILURE;
#ifdef ENABLE_IMAGE
	if (tb_set_output_mode(TB_OUTPUT_256)) return ERROR_TERMBOX_FAILURE;
#endif
	if ((ret = sandbox_init())) {
		tb_shutdown();
#ifdef __linux
		if (ret == ERROR_LANDLOCK_FAILURE) {
			printf("Unable to initialize landlock: %s\n",
					strerror(errno));
			config.enableLandlock = 0;
			config_save();
			printf("Disabling landlock\n");
			proc_fork(NULL, NULL, NULL);
			exit(0);
		}
#endif
		return ret;
	}
	history_init();

	return 0;
}
