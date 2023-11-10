/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "strlcpy.h"
#include "macro.h"
#include "strnstr.h"
#include "page.h"
#include "client.h"
#include "tab.h"
#include "request.h"
#include "certificate.h"
#include "error.h"
#include "parser.h"

static int need_argument(struct client * client,
			const char *field, size_t len, char *error) {
	if (!(!len || !*field)) return 0;
	client->error = 1;
	STRLCPY(client->cmd, error);
	return 1;
}

static int no_argument(struct client * client, const char *field, size_t len) {
	if (!len || !*field) return 0;
	client->error = 1;
	snprintf(V(client->cmd), "Trailing characters: %s", field);
	return 1;
}

int command_quit(struct client *client, const char* ptr, size_t len) {
	if (no_argument(client, ptr, len)) return 0;
	return 1;
}

int command_close(struct client *client, const char* ptr, size_t len) {
	if (no_argument(client, ptr, len)) return 0;
	client_closetab(client);
	return !client->tab;
}

int command_newtab(struct client *client, const char* ptr, size_t len) {

	int ret;
	const char *url = "about:newtab";

	if (!client->tab || !len) return 0;
	if (*ptr) url = ptr;

	if ((ret = client_newtab(client, url))) {
		client->error = 1;
		error_string(ret, V(client->cmd));
	}
	return 0;
}

int command_search(struct client *client, const char* ptr, size_t len) {

	int ret;
	char url[MAX_URL];

	if (!client->tab || !len) return 0;
	if (need_argument(client, ptr, len, "Empty query")) return 0;

	snprintf(V(url), "gemini://geminispace.info/search?%s", ptr);

	if ((ret = tab_request(client->tab, url))) {
		client->error = 1;
		error_string(ret, V(client->cmd));
	}
	return 0;
}

int command_open(struct client *client, const char* ptr, size_t len) {

	size_t link;
	struct request *req;
	char buf[MAX_URL];
	int i;

	if (need_argument(client, ptr, len, "No URL")) return 0;
	if (!client->tab) return 0;

	STRLCPY(buf, ptr);
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
	if (!req) return 0;
	link--;
	if (link >= req->page.links_count) {
		client->error = 1;
		STRLCPY(client->cmd, "Invalid link number");
		return 0;
	}
	request_follow(req, req->page.links[link], V(buf));
	tab_request(client->tab, buf);
	return 0;
}

int command_gencert(struct client *client, const char* ptr, size_t len) {

	struct request *req;

	if (no_argument(client, ptr, len)) return 0;

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

#include <time.h>
#include "known_hosts.h"
int command_forget(struct client *client, const char* ptr, size_t len) {

	const char *host;
	char buf[MAX_URL];
	int i, ret;

	if (need_argument(client, ptr, len, "No host")) return 0;
	if (!client->tab) return 0;

	host = ptr;
	STRLCPY(buf, host);
	i = strnlen(V(buf)) - 1;
	for (; i > 0; i--) {
		if (buf[i] <= ' ') buf[i] = '\0';
	}

	if ((ret = known_hosts_forget(buf))) {
		client->error = 1;
		error_string(ret, V(client->cmd));
		return 0;
	}

	client->error = ERROR_INFO;
	snprintf(V(client->cmd),
		"Certificate for %s removed from well-known hosts", buf);

	return 0;
}

#include "storage.h"
int command_download(struct client *client, const char* args, size_t len) {

	struct request *req;
	FILE *f;
	char name[MAX_URL], *ptr;
	char buf[MAX_URL];
	int i;

	if (no_argument(client, args, len)) return 0;

	req = tab_completed(client->tab);
	if (!req) {
		client->error = 1;
		STRLCPY(client->cmd, "Invalid page");
		return 0;
	}

	ptr = strnstr(req->url, "://", sizeof(req->url));
	if (!ptr) STRLCPY(name, req->url);
	else {
		char *start;
		size_t i = STRLCPY(buf, req->url);
		for (; i > 0; i--) {
			if (WHITESPACE(buf[i])) buf[i] = 0;
			else if (buf[i] == '/') buf[i] = 0;
			else break;
		}
		ptr = strrchr(buf, '/');
		if (!ptr) ptr = buf;
		else ptr++;
		start = ptr;
		for (i = 0; *ptr; i++) ptr++;
		if (i < 2) STRLCPY(name, req->url);
		else STRLCPY(name, start);
	}

	STRLCPY(buf, name);
	for (i = 1; i < 128 && storage_download_exist(name); i++) {
		snprintf(V(name), "%s (%d)", buf, i);
	}
	f = i < 128 ? storage_download_fopen(name, "wb") : NULL;
	if (!f) {
		client->error = 1;
		error_string(ERROR_ERRNO, V(client->cmd));
		return 0;
	}
	fwrite(&req->data[req->page.offset], 1,
			req->length - req->page.offset, f);
	fclose(f);

	client->error = ERROR_INFO;
	snprintf(V(client->cmd), "File downloaded : %s", name);

	return 0;
}

#include "bookmarks.h"
int command_add(struct client *client, const char* args, size_t len) {

	int ret;
	struct request *req;

	if (!client->tab || !len) return 0;
	req = tab_completed(client->tab);
	if (!(req = tab_completed(client->tab))) return 0;
	if ((ret = bookmark_add(req->url, args)) ||
			(ret = bookmark_rewrite())) {
		error_string(ret, V(client->cmd));
		client->error = 1;
	}
	return 0;
}
