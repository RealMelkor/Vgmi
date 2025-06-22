/*
 * ISC License
 * Copyright (c) 2024 RMF <rawmonk@rmf-dev.com>
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include "termbox.h"
#include "macro.h"
#include "config.h"
#include "client.h"
#include "gemini.h"
#include "page.h"
#include "request.h"
#include "parser.h"
#include "secure.h"
#include "tab.h"
#include "strlcpy.h"
#include "url.h"
#include "error.h"

struct request_thread {
	pthread_t thread;
	struct tab *tab;
	struct request *request;
	char url[MAX_URL];
};

struct tab *tab_new(void) {
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
			if (found++ > config.maximumCachedPages)
				request->state = STATE_ENDED;
		}
	}
	prev = NULL;
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
	if (iterations > config.maximumRedirects) {
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
	if (req->state == STATE_ABANDONED) {
		request_free_ref(request);
		request_free(req);
		confirm = 0;
	} else if (req->state == STATE_CANCELED) {
		request_free_ref(request);
		confirm = 0;
	} else if (request.state == STATE_CANCELED) {
		confirm = 0;
		req->state = STATE_CANCELED;
	} else if (failure) {
		{
			char error[1024];
			error_string(request.error, V(error));
			snprintf(V(tab->error), "%s: %s", error, request.url);
		}
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
		{
			char status[1024] = {0};
			gemini_status_string(request.status, V(status));
			snprintf(V(tab->error), "%s (%d : %s)", request.meta,
					request.status, status);
		}
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
		int protocol, proxy;
		protocol = protocol_from_url(destination);
		proxy = *config.proxyHttp && (protocol == PROTOCOL_HTTPS ||
				protocol == PROTOCOL_HTTP);
		if (protocol != PROTOCOL_NONE && !proxy &&
					protocol != PROTOCOL_GEMINI) {
			request.error = ERROR_UNSUPPORTED_PROTOCOL;
			error_string(request.error, V(tab->error));
			tab->failure = 1;
			request.state = STATE_FAILED;
		} else {
			request_follow(&request, destination, V(args->url));
			iterations++;
			goto restart;
		}
	}
	free(args);
	tab_clean_requests(tab);
	return NULL;
}

int tab_request(struct tab* tab, const char *url) {

	struct request_thread *args;
	pthread_attr_t tattr = {0};

	/* clean up forward history */
	pthread_mutex_lock(tab->mutex);
	if (tab->view) {
		struct request *req, *next;
		for (req = tab->request; req; req = next) {
			if (req == tab->view) break;
			next = req->next;
			if (req->state == STATE_ONGOING ||
					req->state == STATE_CANCELED) {
				req->state = STATE_ABANDONED;
			} else request_free(req);
		}
		tab->request = tab->view;
		tab->view = NULL;
	}
	pthread_mutex_unlock(tab->mutex);

	args = malloc(sizeof(*args));
	if (!args) return ERROR_MEMORY_FAILURE;
	STRLCPY(args->url, url);
	args->tab = tab;
	args->request = tab_request_new(tab);
	if (!args->request) {
		free(args);
		return ERROR_MEMORY_FAILURE;
	}
	if (pthread_attr_init(&tattr)) {
		free(args);
		return ERROR_PTHREAD;
	}
	pthread_create(&args->thread, &tattr, tab_request_thread, args);
	args->request->thread = (void*)args->thread;
	return 0;
}

int tab_follow(struct tab* tab, const char *link) {

	struct request *req;
	char url[MAX_URL], buf[MAX_URL];
	int ret;

	if (!tab) return 0;
	format_link(link, MAX_URL, V(buf));
	if (!(req = tab_completed(tab))) return 0;
	if ((ret = request_follow(req, buf, V(url)))) return ret;
	tab_request(tab, url);

	return 0;
}

int tab_scroll(struct tab *tab, int scroll, struct rect rect) {
	struct request *req = tab_completed(tab);
	if (!req) return -1;
	return request_scroll(req, scroll, rect);
}

struct request *tab_input(struct tab *tab) {
	struct request *req;
	if (!tab) return NULL;
	for (req = tab->request; req; req = req->next) {
		if (req->state != STATE_COMPLETED) continue;
		if (req->status == GMI_INPUT || req->status == GMI_SECRET)
			return req;
	}
	return NULL;
}

struct request *tab_completed(struct tab *tab) {
	struct request *req;
	if (!tab) return NULL;
	if (tab->view) return tab->view;
	for (req = tab->request; req; req = req->next) {
		if (req->state != STATE_COMPLETED) continue;
		if (req->next && !STRCMP(req->url, req->next->url)) {
			req->next->state = STATE_FAILED;
		}
		if (req->status == GMI_SUCCESS) return req;
	}
	return NULL;
}

void *tab_free_thread(void *tab) {
	tab_free(tab);
	return NULL;
}

void tab_free(struct tab *tab) {
	struct request *req = tab->request;
	pthread_mutex_lock(tab->mutex);
	while (req) {
		struct request *next = req->next;
		while (req->state == STATE_ONGOING) {
			pthread_join((pthread_t)req->thread, NULL);
		}
		request_free(req);
		req = next;
	}
	tab->request = NULL;
	pthread_mutex_unlock(tab->mutex);
	pthread_mutex_destroy(tab->mutex);
	free(tab->mutex);
	free(tab);
}

struct tab *tab_close(struct tab *tab) {
	struct tab *next, *prev, *ret;
	if (!tab) return NULL;
	next = tab->next;
	prev = tab->prev;
	if (next) next->prev = prev;
	if (prev) prev->next = next;
	if (next) {
		ret = next;
	} else if (prev) {
		ret = prev;
	} else ret = NULL;
	pthread_create((pthread_t*)&tab->thread, NULL, tab_free_thread, tab);
	return ret;
}
