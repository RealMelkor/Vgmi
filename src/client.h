/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#define MAX_CMDLINE 2048
#define MAX_PREFIX 1024
#define MAX_COUNT 10000000
#define MAX_CMD_NAME 64

#define ERROR_INFO 0xAA
#define HAS_TABS(X) (X->tab && (X->tab->prev || X->tab->next))
#define TAB_WIDTH 32

#ifndef size_t
#define size_t unsigned long
#endif

enum {
	MODE_NORMAL,
	MODE_CMDLINE
};

struct client;

struct client {
	int count;
	int mode;
	int width;
	int height;
	char cmd[MAX_CMDLINE];
	char search[MAX_CMDLINE];
	char prefix[MAX_PREFIX];
	int cursor;
	int error;
	int (*motion)(struct client*, int, int);
	struct tab *tab;
	/* tab completion */
	int matches[32];
	int tabcompletion;
	int tabcompletion_selected;
	/* internal */
	int g;
	int exit;
	void *last_request;
};

int client_init(struct client*);
int client_init_termbox(void);
int client_destroy(struct client*);
void client_draw(struct client*);
void client_display(struct client*);
struct rect client_display_rect(struct client*);
int client_input(struct client*);
int client_newtab(struct client *client, const char *url, int follow);
int client_closetab(struct client *client);
