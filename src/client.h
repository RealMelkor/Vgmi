/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#define MAX_CMDLINE 2048
#define MAX_PREFIX 1024
#define MAX_COUNT 10000000
#define MAX_CMD_NAME 64

#define ERROR_INFO 0xAA

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
	/* internal */
	int g;
	void *last_request;
};

int client_init(struct client*);
int client_destroy(struct client*);
void client_draw(struct client*);
void client_display(struct client*);
struct rect client_display_rect(struct client*);
int client_input(struct client*);
int client_newtab(struct client *client, const char *url);
int client_closetab(struct client *client);
