/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@rmf-dev.com>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "strlcpy.h"
#include "macro.h"
#include "strnstr.h"
#include "config.h"
#include "page.h"
#include "client.h"
#include "tab.h"
#include "request.h"
#include "certificate.h"
#include "error.h"
#include "parser.h"
#include "xdg.h"

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

int command_reload(struct client *client, const char* ptr, size_t len) {
	struct request *req;
	if (no_argument(client, ptr, len)) return 0;
	req = tab_completed(client->tab);
	if (req) tab_request(client->tab, req->url);
	return 0;
}

int command_newtab(struct client *client, const char* ptr, size_t len) {

	int ret;
	const char *url = "about:newtab";

	if (!client->tab || !len) return 0;
	if (*ptr) url = ptr;

	if ((ret = client_newtab(client, url, 0))) {
		client->error = 1;
		error_string(ret, V(client->cmd));
	}
	return 0;
}

int command_tabnext(struct client *client, const char* ptr, size_t len) {
	if (no_argument(client, ptr, len)) return 0;
	if (client->tab->next) client->tab = client->tab->next;
	else while (client->tab->prev) client->tab = client->tab->prev;
	return 0;
}

int command_tabprev(struct client *client, const char* ptr, size_t len) {
	if (no_argument(client, ptr, len)) return 0;
	if (client->tab->prev) client->tab = client->tab->prev;
	else while (client->tab->next) client->tab = client->tab->next;
	return 0;
}

int command_search(struct client *client, const char* ptr, size_t len) {

	int ret;
	char url[MAX_URL];

	if (!client->tab || !len) return 0;
	if (need_argument(client, ptr, len, "Empty query")) return 0;

	snprintf(V(url), config.searchEngineURL, ptr);

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

	for (i = 0; buf[i]; i++) {
		if (buf[i] >= '0' && buf[i] <= '9') continue;
		tab_request(client->tab, buf);
		return 0;
	}

	req = tab_completed(client->tab);
	if (!req) return 0;
	link = atoi(buf) - 1;
	if (link >= req->page.links_count) {
		client->error = 1;
		STRLCPY(client->cmd, "Invalid link number");
		return 0;
	}
	request_follow(req, req->page.links[link], V(buf));
	tab_request(client->tab, buf);
	return 0;
}

#include <stdint.h>
#include "termbox.h"

int command_gencert(struct client *client, const char* args, size_t len) {

	struct request *req;

	if (no_argument(client, args, len)) return 0;

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

	req = tab_completed(client->tab);
	if (!req) {
		client->error = 1;
		STRLCPY(client->cmd, "Invalid page");
		return 0;
	}

	if (len && *args) {
		STRLCPY(name, args);
	} else if (!strnstr(req->url, "://", sizeof(req->url))) {
		STRLCPY(name, req->url);
	} else {
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
		char *dot;
		dot = strrchr(buf, '.');
		if (!dot || dot == buf) {
			snprintf(V(name), "%s (%d)", buf, i);
			continue;
		}
		*dot = '\0';
		snprintf(V(name), "%s (%d).%s", buf, i, dot + 1);
		*dot = '.';
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

int command_exec(struct client *client, const char* args, size_t len) {
	char buf[PATH_MAX], download_dir[PATH_MAX], name[256];
	int err;
	if (no_argument(client, args, len)) return 0;
	client->error = 0;
	snprintf(V(name), ".tmp_vgmi_exec_"TIME_T"%d", time(NULL), rand());
	if (command_download(client, name, 1)) return -1;
	if (client->error != ERROR_INFO) return 0;
	client->error = 0;
	if (config.enableSandbox || !config.launcherTerminal) {
		snprintf(V(buf), "download://%s", name);
		xdg_request(buf);
		return 0;
	}
	if ((err = storage_download_path(V(download_dir)))) {
		client->error = 1;
		error_string(err, V(client->cmd));
		return 0;
	}
	snprintf(V(buf), "\"%s\" %s/\"%s\"",
			config.launcher, download_dir, name);
	tb_shutdown();
	system(buf);
	unlinkat(download_fd, name, 0);
	client_init_termbox();
	return 0;
}

#include "bookmarks.h"
int command_add(struct client *client, const char* args, size_t len) {

	int ret;
	struct request *req;

	if (!client->tab || !len) return 0;
	if (!(req = tab_completed(client->tab))) return 0;
	if (!*args) args = req->page.title;
	if ((ret = bookmark_add(req->url, args)) ||
			(ret = bookmark_rewrite())) {
		error_string(ret, V(client->cmd));
		client->error = 1;
	}
	return 0;
}

int command_help(struct client *client, const char* args, size_t len) {
	int ret;
	if (no_argument(client, args, len)) return 0;
	if (!client->tab) return 0;
	if ((ret = client_newtab(client, "about:help", 0)) ||
			(ret = bookmark_rewrite())) {
		error_string(ret, V(client->cmd));
		client->error = 1;
	}
	return 0;
}
