/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <errno.h>
struct client;
struct rect;
#include "macro.h"
#include "error.h"
#include "gemini.h"
#include "page.h"
#include "request.h"
#include "tab.h"
#include "client.h"
#include "commands.h"
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

int client_destroy(struct client *client) {
	struct command *command;
	if (!client) return ERROR_NULL_ARGUMENT;
	if (client->tab) {
		tab_free(client->tab);
	}
	command = client->commands;
	while (command) {
		struct command *next = command->next;
		free(command);
		command = next;
	}
	known_hosts_free();
	if (tb_shutdown()) return ERROR_TERMBOX_FAILURE;
	return 0;
}

int client_closetab(struct client *client) {
	struct tab *tab, *next, *prev;
	if (!client->tab) return 1;
	tab = client->tab;
	next = tab->next;
	prev = tab->prev;
	if (next) next->prev = prev;
	if (prev) prev->next = next;
	if (next) {
		client->tab = next;
	} else if (prev) {
		client->tab = prev;
	} else client->tab = NULL;
	tab_free(tab);
	return 0;
}

int client_newtab(struct client *client, const char *url) {
	struct tab *tab;
	if (!url) url = "about:newtab";
	tab = tab_new();
	if (!tab) return ERROR_MEMORY_FAILURE;
	if (client->tab) {
		tab->next = client->tab->next;
		if (tab->next) tab->next->prev = tab;
		tab->prev = client->tab;
		client->tab->next = tab;
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
		ret = tb_poll_event(&ev);
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

int client_addcommand(struct client *client, const char *name,
			int (*cmd)(struct client*, const char*, size_t)) {

	struct command *command;

	if (strlen(name) > sizeof(command->name))
		return ERROR_INVALID_COMMAND_NAME;
        command	= malloc(sizeof(struct command));
	if (!command) return ERROR_MEMORY_FAILURE;
	command->command = cmd;
	STRLCPY(command->name, name);
	command->next = client->commands;
	client->commands = command;
	return 0;
}

int client_init(struct client* client) {

	int ret;

	memset(client, 0, sizeof(*client));
	if ((ret = parser_request_create())) return ret;
	if ((ret = parser_page_create())) return ret;
#ifdef ENABLE_IMAGE
	if ((ret = image_init())) return ret;
#endif
	if ((ret = client_addcommand(client, "qa", command_quit))) return ret;
	if ((ret = client_addcommand(client, "q", command_close))) return ret;
	if ((ret = client_addcommand(client, "o", command_open))) return ret;
	if ((ret = client_addcommand(client, "s", command_search))) return ret;
	if ((ret = client_addcommand(client, "add", command_add))) return ret;
	if ((ret = client_addcommand(client, "nt", command_newtab)))
		return ret;
	if ((ret = client_addcommand(client, "tabnew", command_newtab)))
		return ret;
	if ((ret = client_addcommand(client, "gencert", command_gencert)))
		return ret;
	if ((ret = client_addcommand(client, "forget", command_forget)))
		return ret;
	if ((ret = client_addcommand(client, "download", command_download)))
		return ret;
	if ((ret = client_addcommand(client, "help", command_help)))
		return ret;
	if ((ret = storage_init())) return ret;
	if ((ret = known_hosts_load())) return ret;
	if ((ret = bookmark_load())) return ret;
	if (tb_init()) return ERROR_TERMBOX_FAILURE;
#ifdef ENABLE_IMAGE
	if (tb_set_output_mode(TB_OUTPUT_256)) return ERROR_TERMBOX_FAILURE;
#endif
	if ((ret = sandbox_init())) {
		tb_shutdown();
		return ret;
	}

	return 0;
}
