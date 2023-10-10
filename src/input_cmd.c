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
#include "client.h"

void client_enter_mode_cmdline(struct client *client) {
	client->count = 0;
	client->error = 0;
	memset(client->cmd, 0, sizeof(client->cmd));
	client->mode = MODE_CMDLINE;
	client->cursor = STRLCPY(client->cmd, ":");
}

int handle_cmd(struct client *client) {

	struct command *command;
	const char invalid[] = "Invalid command: %s";
	char name[MAX_CMD_NAME], cmd[MAX_CMDLINE - sizeof(invalid)];
	size_t i;

	ASSERT(sizeof(cmd) > sizeof(name))

	STRLCPY(cmd, &client->cmd[1]);

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
	for (command = client->commands; command; command = command->next) {
		if (STRCMP(command->name, name)) continue;
		if (command->command(client, cmd, sizeof(cmd)))
			return 1;
		return 0;
	}
	client->error = 1;
	snprintf(V(client->cmd), invalid, cmd);
	return 0;
}

int client_input_cmdline(struct client *client, struct tb_event ev) {

	int len;

	switch (ev.key) {
	case TB_KEY_ESC:
		client->mode = MODE_NORMAL;
		return 0;
	case TB_KEY_ENTER:
		if (handle_cmd(client)) return 1;
		client->mode = MODE_NORMAL;
		return 0;
	case TB_KEY_BACKSPACE:
	case TB_KEY_BACKSPACE2:
		if (client->cursor <= 1) {
			client->mode = MODE_NORMAL;
			return 0;
		}
		client->cursor = utf8_previous(client->cmd, client->cursor);
		client->cmd[client->cursor] = 0;
		return 0;
	}

	if (!ev.ch) return 0;

	len = utf8_unicode_length(ev.ch);
	if ((size_t)(client->cursor + len) >= sizeof(client->cmd)) return 0;

	len = utf8_unicode_to_char(&client->cmd[client->cursor], ev.ch);
	client->cursor += len;
	client->cmd[client->cursor] = '\0';

	return 0;
}
