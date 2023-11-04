/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "strlcpy.h"
#include "macro.h"
#include "page.h"
#include "client.h"
#include "tab.h"
#include "request.h"
#include "certificate.h"

int command_quit(struct client *client, const char* ptr, size_t len) {
	if (strncmp(ptr, "q", len)) {
		client->error = 1;
		snprintf(V(client->cmd), "Trailing characters: %s", &ptr[2]);
		return 0;
	}
	return 1;
}

int command_open(struct client *client, const char* ptr, size_t len) {

	const char start[] = "o ";
	const char *url;
	size_t link;
	struct request *req;
	char buf[MAX_URL];
	int i;

	if (!client->tab) return -1;

	if (memcmp((void*)ptr, start, sizeof(start) - 1) ||
			len <= sizeof(start) + 1) {
		client->error = 1;
		STRLCPY(client->cmd, "No URL");
		return 0;
	}

	url = &ptr[sizeof(start) - 1];
	STRLCPY(buf, url);
	i = strnlen(V(buf)) - 1;
	for (; i > 0; i--) {
		if (buf[i] <= ' ') buf[i] = '\0';
	}

	link = atoi(buf);
	if (!link) {
		tab_request(client->tab, buf);
		return 0;
	}

	req = tab_completed(client->tab);
	if (!req) return -1;
	link--;
	if (link >= req->text.links_count) {
		client->error = 1;
		STRLCPY(client->cmd, "Invalid link number");
		return 0;
	}
	request_follow(req, req->text.links[link], V(buf));
	tab_request(client->tab, buf);
	return 0;
}

int command_gencert(struct client *client, const char* ptr, size_t len) {

	const char start[] = "gencert";
	struct request *req;

	if (strncmp(ptr, start, len)) {
		client->error = 1;
		snprintf(V(client->cmd), "Trailing characters: %s",
				&ptr[sizeof(start)]);
		return 0;
	}

	req = tab_completed(client->tab);
	if (!req) {
		client->error = 1;
		STRLCPY(client->cmd, "Invalid page");
		return 0;
	}
	if (!strncmp(req->url, V("about:") - 1)) {
		client->error = 1;
		STRLCPY(client->cmd,
			"Cannot create certificate for \"about:\" pages");
		return 0;
	}
	if (certificate_create(req->name, V(client->cmd)))
		client->error = 1;
	return 0;
}
