/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#define MAX_CMDLINE 4096
#define MAX_COUNT 10000000
#define MAX_CMD_NAME 64

#ifndef size_t
#define size_t unsigned long
#endif

enum {
	MODE_NORMAL,
	MODE_CMDLINE
};

struct client;

struct command {
	char name[MAX_CMD_NAME];
	int (*command)(struct client*, const char*, size_t);
	struct command *next;
};

struct client {
	int count;
	int mode;
	int width;
	int height;
	char cmd[MAX_CMDLINE];
	int cursor;
	int error;
	struct command *commands;
	int (*motion)(struct client*, int, int);
	struct tab *tab;
	/* internal */
	int last_ch;
	int g;
	void *last_request;
};

struct rect {
	int x;
	int y;
	int w;
	int h;
};

int client_init(struct client*);
int client_destroy(struct client*);
void client_draw(struct client*);
void client_display(struct client*);
struct rect client_display_rect(struct client*);
int client_input(struct client*);
int client_addcommand(struct client*, const char*,
			int (*)(struct client *, const char*, size_t));
