/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "macro.h"
#include "error.h"
#include "client.h"
#include "dns.h"
#include "page.h"
#include "request.h"
#include "parser.h"
#include "secure.h"
#include "image.h"
#include "tab.h"
#include "proc.h"
#include "storage.h"
#include "config.h"
#include "xdg.h"

int main(int argc, char *argv[]) {
	
	struct client client = {0};
	int ret;
	const char *url = "about:newtab";

	if (argc > 1) {
#ifdef ENABLE_IMAGE
		if (!strcmp(argv[1], "--image")) {
			if (storage_init()) return -1;
			config_load();
			storage_close();
			image_parser(STDIN_FILENO, STDOUT_FILENO);
			proc_exit();
			return 0;
		}
#endif
#ifndef DISABLE_XDG
		if (!strcmp(argv[1], "--xdg")) {
			xdg_proc(STDIN_FILENO, STDOUT_FILENO);
			return 0;
		}
#endif
		if (!strcmp(argv[1], "--page")) {
			parser_page(STDIN_FILENO, STDOUT_FILENO);
			proc_exit();
			return 0;
		}
		if (!strcmp(argv[1], "--request")) {
			parser_request(STDIN_FILENO, STDOUT_FILENO);
			proc_exit();
			return 0;
		}
		url = argv[1];
	}

	proc_argv(argv);

	if ((ret = client_init(&client))) {
		char error[1024];
		error_string(ret, V(error));
		printf("Initialisation failure: %s\n", error);
		return -1;
	}

	client.tab = tab_new();
	tab_request(client.tab, url);

	do {
		client_display(&client);
	} while (!client_input(&client));

	client_destroy(&client);

	return 0;
}
