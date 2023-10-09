/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include "termbox.h"
#include "macro.h"
#include "client.h"
#include "gemtext.h"
#include "gemini.h"
#include "request.h"
#include "secure.h"
#include "tab.h"
#include "wcwidth.h"
#include "strnstr.h"
#include "strlcpy.h"
#include "error.h"

void tab_clean_requests(struct tab *tab);

void tab_display_loading(struct tab *tab, struct rect rect) {
	int off[][2] = {
		{0, 0}, {1, 0}, {2, 0}, {2, 1}, {2, 2}, {1, 2}, {0, 2}, {0, 1}
	};
	int x = rect.x + rect.w - 4, y = rect.y + rect.h - 4, i;
	struct timespec now;
	tab->loading = (tab->loading + 1) % (sizeof(off) / sizeof(*off));

	clock_gettime(CLOCK_MONOTONIC_RAW, &now);
	tab->loading = (now.tv_sec * 10 + now.tv_nsec / 100000000) %
			(sizeof(off) / sizeof(*off));

	for (i = 0; i < (int)(sizeof(off) / sizeof(*off)); i++)
		tb_set_cell(x + off[i][0], y + off[i][1], ' ', TB_WHITE,
			i != tab->loading ? TB_WHITE : TB_BLUE);
}

void tab_display_gemtext(struct request *req, struct rect rect) {

	char *data, *start;

	if (!req) return;

	data = req->data;
	start = strnstr(data, "\r\n", req->length);
	if (!start) return;

	if (req->text.width != rect.w - rect.x) {
		int error = gemtext_parse(req->data, req->length,
				rect.w - rect.x, &req->text);
		if (error) {
			req->error = error;
			return;
		}
		request_scroll(req, 0, rect); /* fix scroll */
	}
	gemtext_display(req->text, req->scroll,
			rect.h - rect.y, req->selected);
}

void tab_display_error(struct tab *tab, struct client *client) {
	if (!tab) return;
	if (tab->request->status > 0) {
		snprintf(V(client->cmd), "%d: %s",
				tab->request->status, tab->error);
	} else {
		snprintf(V(client->cmd), "%s: %s",
				tab->error, tab->request->url);
	}
	client->error = 1;
	tab->request->state = STATE_ENDED;
	tb_refresh();
	tab_clean_requests(tab);
}

void tab_display_request(struct request *req, struct rect rect) {
	if (!req) return;
	switch (req->status) {
	case GMI_SUCCESS:
		tab_display_gemtext(req, rect);
		break;
	default:
		tab_display_gemtext(req, rect);
		break;
	}
}

void tab_display(struct tab *tab, struct client *client) {

	struct rect rect;
	struct request *req;

	if (!tab || !tab->request) return;

	rect = client_display_rect(client);

	req = tab_completed(tab);
	if (req) tab_display_request(req, rect);

	switch (tab->request->state) {
	case STATE_COMPLETED:
		tab_display_request(tab->request, rect);
		break;
	case STATE_FAILED:
		tab_display_error(tab, client);
		break;
	case STATE_ONGOING:
		tab_display_loading(tab, rect);
		break;
	}
}

struct request_thread {
	pthread_t thread;
	struct tab *tab;
	struct request *request;
	char url[MAX_URL];
};

struct tab *tab_new() {
	struct tab *tab = calloc(1, sizeof(struct tab));
	if (!tab) return NULL;
	tab->mutex = malloc(sizeof(pthread_mutex_t));
	if (!tab->mutex) {
		free(tab);
		return NULL;
	}
	pthread_mutex_init(tab->mutex, NULL);
	return tab;
}

struct request *tab_request_new(struct tab *tab) {

	struct request *new, *req;

	new = calloc(1, sizeof(struct request));
	if (!new) return NULL;
	pthread_mutex_lock(tab->mutex);
	new->next = tab->request;
	for (req = new->next; req; req = req->next)
		request_cancel(req);
	tab->request = new;
	pthread_mutex_unlock(tab->mutex);
	return new;
}

void tab_clean_requests(struct tab *tab) {

	struct request *request, *prev, *next;
	int found;

	if (!tab) return;

	pthread_mutex_lock(tab->mutex);
	found = 0;
	for (request = tab->request; request; request = request->next) {
		if (request->state == STATE_COMPLETED) {
			if (!found) {
				found = 1;
				continue;
			}
			request->state = STATE_ENDED;
		}
	}
	next = prev = NULL;
	for (request = tab->request; request; request = next) {
		next = request->next;
		if (request != tab->request &&
				request->state == STATE_FAILED)
			request->state = STATE_ENDED;
		if (request->state == STATE_ENDED) {
			if (prev)
				prev->next = request->next;
			if (request == tab->request) {
				if (prev) tab->request = prev;
				else tab->request = request->next;
			}
			request_free(request);
		} else prev = request;
	}
	pthread_mutex_unlock(tab->mutex);
}

#define MAX_REDIRECT 5

void* tab_request_thread(void *ptr) {

	struct request_thread *args = ptr;
	struct tab *tab = args->tab;
	struct request *req = args->request; /* copy next to the ref */
	struct request request = {0};
	struct secure *ctx;
	char destination[MAX_URL];
	int failure;
	int redirect;
	int iterations;
	int confirm;

	iterations = 0;
restart:
	if (iterations > MAX_REDIRECT) {
		request.error = ERROR_TOO_MANY_REDIRECT;
		error_string(request.error, V(tab->error));
		tab->failure = 1;
		request.next = req->next;
		request.status = -1;
		request.state = STATE_FAILED;
		*req = request;
		tb_refresh(); /* refresh screen */
		return 0;
	}
	redirect = 0;
	ctx = secure_new();
	failure = request_process(&request, ctx, args->url);
	pthread_mutex_lock(tab->mutex);
	confirm = 1;
	if (req->state == STATE_CANCELED) {
		request_free_ref(request);
		confirm = 0;
	} else if (failure) {
		error_string(request.error, V(tab->error));
		tab->failure = 1;
	} else if (gemini_isredirect(request.status)) {
		redirect = 1;
		STRLCPY(destination, request.meta);
		confirm = 0;
	} else if (request.status == -1) {
		request.error = ERROR_INVALID_DATA;
		error_string(request.error, V(tab->error));
		tab->failure = 1;
		request.state = STATE_FAILED;
	} else if (gemini_iserror(request.status)) {
		STRLCPY(tab->error, request.meta);
		tab->failure = 1;
		request.state = STATE_FAILED;
	}
	if (confirm) {
		request.next = req->next;
		*req = request;
		tb_refresh(); /* refresh screen */
	}
	pthread_mutex_unlock(tab->mutex);

	secure_free(ctx);
	if (redirect) {
		STRLCPY(args->url, destination);
		iterations++;
		goto restart;
	}
	free(args);
	tab_clean_requests(tab);
	return NULL;
}

int tab_request(struct tab* tab, const char *url) {

	struct request_thread *args;

	args = malloc(sizeof(*args));
	if (!args) return ERROR_MEMORY_FAILURE;
	STRLCPY(args->url, url);
	args->tab = tab;
	args->request = tab_request_new(tab);
	if (!args->request) {
		free(args);
		return ERROR_MEMORY_FAILURE;
	}
	pthread_create(&args->thread, NULL, tab_request_thread, args);
	pthread_detach(args->thread);
	return 0;
}

int tab_scroll(struct tab *tab, int scroll, struct rect rect) {
	struct request *req = tab_completed(tab);
	if (!req) return -1;
	return request_scroll(req, scroll, rect);
}

struct request *tab_completed(struct tab *tab) {
	struct request *req;
	if (!tab) return NULL;
	for (req = tab->request; req; req = req->next) {
		if (req->state == STATE_COMPLETED) {
			return req;
		}
	}
	return NULL;
}

void tab_free(struct tab *tab) {
	struct request *req = tab->request;
	while (req) {
		struct request *next = req->next;
		request_free(req);
		req = next;
	}
	tab->request = NULL;
	pthread_mutex_destroy(tab->mutex);
	free(tab->mutex);
	free(tab);
}
