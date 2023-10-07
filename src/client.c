#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
struct client;
struct rect;
#include "macro.h"
#include "error.h"
#include "gemtext.h"
#include "request.h"
#include "tab.h"
#include "client.h"
#include "commands.h"
#include "termbox.h"
#include "utf8.h"
#include "input.h"
#include "strlcpy.h"

int client_destroy(struct client* client) {
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
	if (tb_shutdown()) return ERROR_TERMBOX_FAILURE;
	return 0;
}

int client_input(struct client *client) {

	struct tb_event ev;
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

	switch (client->mode) {
	case MODE_NORMAL:
		return client_input_normal(client, ev);
	case MODE_CMDLINE:
		return client_input_cmdline(client, ev);
	}
	
	return 0;
}

int client_addcommand(struct client * client, const char * name,
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
	if ((ret = client_addcommand(client, "q", command_quit))) return ret;
	if ((ret = client_addcommand(client, "o", command_open))) return ret;
	if (tb_init()) return ERROR_TERMBOX_FAILURE;
	return 0;
}
