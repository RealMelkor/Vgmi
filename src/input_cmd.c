/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "macro.h"
#include "strlcpy.h"
#include "utf8.h"
#include "termbox.h"
#include "gemini.h"
#include "page.h"
#include "request.h"
#include "client.h"
#include "commands.h"
#include "tab.h"
#include "error.h"
#include "parser.h"
#include "input.h"

static void tabcompletion(struct client *client, char *in)  {
	size_t i, j, len;
	ASSERT(LENGTH(commands) < LENGTH(client->matches));
	client->tabcompletion = 0;
	client->tabcompletion_selected = client->matches[0] = -1;
	len = strlen(in);
	for (j = i = 0; i < LENGTH(commands); i++) {
		if (strncmp(in, commands[i].name, len)) continue;
		client->matches[j++] = i;
	}
	if (j) {
		client->tabcompletion = 1;
		client->matches[j] = -1;
	}
}

static void tabcomplete(struct client *client)  {
	const char *cmd = commands[client->matches[
		client->tabcompletion_selected]].name;
	strlcpy(&client->cmd[1], cmd, sizeof(client->cmd) - 1);
	client->cursor = strnlen(V(client->cmd));
}

static void insert_unicode(char *buf, size_t length, int *pos, uint32_t ch) {
	char cpy[MAX_CMDLINE];
	int end = !buf[*pos], len;
	{
		len = utf8_len(buf, length);
		if (len < *pos) *pos = len;
	}
	if (!end) STRLCPY(cpy, &buf[*pos]);
	len = utf8_unicode_to_char(&buf[*pos], ch);
	*pos += len;
	if (end) buf[*pos] = '\0';
	else strlcpy(&buf[*pos], cpy, length - *pos);
}

static void delete_unicode(char *buf, size_t length, int *pos) {
	int end = !buf[*pos];
	int next = *pos;
	*pos = utf8_previous(buf, *pos);
	if (end) {
		buf[*pos] = 0;
	} else {
		char cpy[MAX_CMDLINE];
		STRLCPY(cpy, &buf[next]);
		strlcpy(&buf[*pos], cpy, length - *pos);
	}
}

void client_enter_mode_cmdline(struct client *client) {
	client->count = 0;
	client->error = 0;
	memset(client->cmd, 0, sizeof(client->cmd));
	client->mode = MODE_CMDLINE;
	client->cursor = STRLCPY(client->cmd, ":");
	if (client->tab) client->tab->input[0] = 0;
}

static void refresh_search(struct client *client) {
	struct request *req;
	if (client->cmd[0] != '/') return;
	STRLCPY(client->search, &client->cmd[1]);
	if (!(req = tab_completed(client->tab))) return;
	page_search(&req->page, client->search);
	req->scroll = page_selection_line(req->page);
	req->scroll -= client_display_rect(client).h / 2;
	tab_scroll(client->tab, 0, client_display_rect(client));
}

int handle_cmd(struct client *client) {

	const char invalid[] = "Invalid command: %s";
	char name[MAX_CMD_NAME], cmd[MAX_CMDLINE - sizeof(invalid)];
	size_t i;
	int number;

	ASSERT(sizeof(cmd) > sizeof(name))

	STRLCPY(cmd, &client->cmd[1]);

	if (!*cmd) return 0;

	number = atoi(cmd);
	if (number || !STRCMP(cmd, "0")) {
		struct request *req;
		if (!client->tab) return 0;
		req = tab_completed(client->tab);
		if (!req) return 0;
		req->scroll = number;
		request_scroll(req, 0, client_display_rect(client));
		return 0;
	}

	for (i = 0; i < sizeof(name); ) {
		char c = cmd[i];
		size_t len;
		if (c == ' ' || c == '\t' || c == '\0') {
			name[i] = '\0';
			break;
		}
		len = i + utf8_char_length(cmd[i]);
		for (; i < len; i++)
			name[i] = cmd[i];
	}
	for (i = 0; i < LENGTH(commands); i++) {
		int j;
		if (STRCMP(commands[i].name, name)) continue;
		j = strnlen(V(commands[i].name));
		for (; cmd[j] && WHITESPACE(cmd[j]); j++) ;
		if (commands[i].command(client, &cmd[j], sizeof(cmd) - j))
			return 1;
		return 0;
	}
	client->error = 1;
	snprintf(V(client->cmd), invalid, cmd);
	return 0;
}

int client_input_cmdline(struct client *client, struct tb_event ev) {

	int len, prefix;
	struct request *req;
	int search_mode = client->cmd[0] == '/';

	req = tab_input(client->tab);

	if (req) {
		prefix = strnlen(V(req->meta)) + 2;
		if (client->cursor < prefix) client->cursor = prefix;
	} else prefix = 1;

	switch (ev.key) {
	case TB_KEY_ESC:
		client_reset(client);
		client->mode = MODE_NORMAL;
		if (req) req->state = STATE_ENDED;
		return 0;
	case TB_KEY_ENTER:
		client->tabcompletion = 0;
		if (search_mode) ;
		else if (req) {
			char buf[2048];
			char url[1024];
			int error, i;
			req->state = STATE_ENDED;
			i = STRLCPY(url, req->url);
			for (; i > 0 && url[i] != '/'; i--) {
				if (url[i] == '?') {
					url[i] = '\0';
					break;
				}
			}
			len = snprintf(V(buf), "%s?%s", url,
					client->tab->input);
			error = len >= MAX_URL ?
				ERROR_URL_TOO_LONG :
				tab_request(client->tab, buf);
			if (error) {
				client->tab->failure = 1;
				error_string(error, V(client->tab->error));
			}
		} else if (handle_cmd(client)) return 1;
		client->mode = MODE_NORMAL;
		return 0;
	case TB_KEY_BACKSPACE:
	case TB_KEY_BACKSPACE2:
		if (client->cursor <= prefix) {
			if (!req && client->cmd[client->cursor]) return 0;
			if (req && client->tab->input[client->cursor])
				return 0;
			if (!req) client->mode = MODE_NORMAL;
			return 0;
		}
		if (!req) {
			delete_unicode(V(client->cmd), &client->cursor);
			refresh_search(client);
			return 0;
		}
		client->cursor -= prefix;
		delete_unicode(V(client->tab->input), &client->cursor);
		client->cursor += prefix;
		goto rewrite;
	case TB_KEY_ARROW_LEFT:
		if (client->cursor <= prefix) break;
		client->cursor = utf8_previous(client->cmd, client->cursor);
		break;
	case TB_KEY_ARROW_RIGHT:
		if (!client->cmd[client->cursor]) break;
		client->cursor +=
			utf8_char_length(client->cmd[client->cursor]);
		break;
	case TB_KEY_TAB:
		if (!client->tabcompletion) {
			tabcompletion(client, &client->cmd[1]);
		} else {
			size_t i = client->tabcompletion_selected;
			i = (i >= LENGTH(client->matches) ||
				client->matches[i + 1] < 0) ? 0 : i + 1;
			client->tabcompletion_selected = i;
			tabcomplete(client);
		}
		break;
	case TB_KEY_BACK_TAB:
		if (!client->tabcompletion) break;
		client->tabcompletion_selected--;
		if (client->tabcompletion_selected < 0) {
			size_t i;
			for (i = 0; i < LENGTH(client->matches); i++) {
				if (client->matches[i + 1] < 0) break;
			}
			client->tabcompletion_selected = i;
		}
		tabcomplete(client);
		break;
	}

	if (!ev.ch) return 0;

	client->tabcompletion = 0;

	len = utf8_unicode_length(ev.ch);
	if ((size_t)(client->cursor + len) >= sizeof(client->cmd)) return 0;

	if (!req) {
		insert_unicode(V(client->cmd), &client->cursor, ev.ch);
		refresh_search(client);
		return 0;
	}

	client->cursor -= prefix;
	insert_unicode(V(client->tab->input), &client->cursor, ev.ch);
	client->cursor += prefix;
rewrite:
	if (req->status == GMI_INPUT) {
		strlcpy(&client->cmd[prefix], client->tab->input,
				sizeof(client->cmd) - prefix);
	} else if (req->status == GMI_SECRET) {
		int i = 0;
		size_t j;
		for (j = prefix; j < sizeof(client->cmd) &&
				client->tab->input[i]; j++) {
			i += utf8_char_length(client->tab->input[i]);
			client->cmd[j] = '*';
		}
		client->cmd[j] = '\0';
	}

	return 0;
}
