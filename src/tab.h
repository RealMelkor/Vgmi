/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
struct tab {
	struct request *request;
	struct request *view;
	void *mutex;
	struct secure *secure;
	char error[1024];
	char input[1024];
	int failure;
	struct tab *next;
	struct tab *prev;
	/* internal */
	int loading;
};

struct tab *tab_new();
void tab_display(struct tab*, struct client*);
int tab_request(struct tab*, const char *url);
int tab_scroll(struct tab*, int, struct rect);
struct request *tab_completed(struct tab *tab);
void tab_free(struct tab*);
