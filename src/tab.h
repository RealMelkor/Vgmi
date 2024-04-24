/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
struct client;
struct tab {
	struct request *request;
	struct request *view;
	void *mutex;
	struct secure *secure;
	char error[3072];
	char input[1024];
	int failure;
	struct tab *next;
	struct tab *prev;
	/* internal */
	int loading;
};

struct tab *tab_new(void);
void tab_display(struct tab*, struct client*);
int tab_request(struct tab*, const char *url);
int tab_follow(struct tab* tab, const char *link);
int tab_scroll(struct tab*, int, struct rect);
struct request *tab_completed(struct tab *tab);
struct request *tab_input(struct tab *tab);
struct tab *tab_close(struct tab *tab);
void tab_free(struct tab*);
