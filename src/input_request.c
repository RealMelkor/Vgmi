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
#include "gemtext.h"
#include "request.h"
#include "client.h"
#include "tab.h"

int client_input_request(struct client *client, struct tb_event ev) {

	int len, meta_len;
	struct request *req;

	if (!client) return 0;
	req = tab_input(client->tab);
	if (!req) return 0;

	meta_len = strnlen(V(req->meta)) + 2;
	if (client->cursor < meta_len) client->cursor = meta_len;

	switch (ev.key) {
	case TB_KEY_ESC:
		client->mode = MODE_NORMAL;
		req->state = STATE_ENDED;
		return 0;
	case TB_KEY_ENTER:
		client->mode = MODE_NORMAL;
		req->state = STATE_ENDED;
		{
			char url[1024];
			snprintf(V(url), "%s?%s", req->url,
					client->tab->input);
			tab_request(client->tab, url);
		}
		return 0;
	case TB_KEY_BACKSPACE:
	case TB_KEY_BACKSPACE2:
		if (client->cursor <= meta_len) return 0;
		client->cursor = utf8_previous(client->cmd, client->cursor);
		client->cmd[client->cursor] = 0;
		return 0;
	}

	if (!ev.ch) return 0;

	len = utf8_unicode_length(ev.ch);
	if ((size_t)(client->cursor + len) >= sizeof(client->cmd)) return 0;

	len = utf8_unicode_to_char(
			&client->tab->input[client->cursor - meta_len], ev.ch);
	client->cursor += len;
	client->tab->input[client->cursor - meta_len] = '\0';
	if (req->status == GMI_INPUT) {
		strlcpy(&client->cmd[meta_len], client->tab->input,
				sizeof(client->cmd) - meta_len);
	} else if (req->status == GMI_SECRET) {
		int i;
		for (i = meta_len; i < client->cursor; i++)
			client->cmd[i] = '*';
		client->cmd[client->cursor] = '\0';
	}

	return 0;
}
