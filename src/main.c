/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#include <stddef.h>
#include <stdio.h>
#include "macro.h"
#include "error.h"
#include "client.h"
#include "dns.h"
#include "page.h"
#include "request.h"
#include "secure.h"
#include "tab.h"

int main(int argc, char *argv[]) {
	
	struct client client = {0};
	int ret;

	if (argc) if (!argv) printf("\n");

	if ((ret = client_init(&client))) {
		char error[1024];
		error_string(ret, V(error));
		printf("Initialisation failure: %s\n", error);
		return -1;
	}

	client.tab = tab_new();
	tab_request(client.tab, "about:newtab");

	do {
		client_display(&client);
	} while (!client_input(&client));

	client_destroy(&client);

	return 0;
}
