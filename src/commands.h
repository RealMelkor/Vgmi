/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
int command_quit(struct client *client, const char* ptr, size_t len);
int command_close(struct client *client, const char* ptr, size_t len);
int command_open(struct client *client, const char* ptr, size_t len);
int command_search(struct client *client, const char* ptr, size_t len);
int command_gencert(struct client *client, const char* ptr, size_t len);
int command_forget(struct client *client, const char* ptr, size_t len);
int command_newtab(struct client *client, const char* ptr, size_t len);
int command_download(struct client *client, const char* ptr, size_t len);
int command_add(struct client *client, const char* args, size_t len);
int command_help(struct client *client, const char* args, size_t len);

struct command {
	char name[MAX_CMD_NAME];
	int (*command)(struct client*, const char*, size_t);
};

static struct command commands[] = {
	{"qa",		command_quit},
	{"q",		command_close},
	{"o",		command_open},
	{"s",		command_search},
	{"add",		command_add},
	{"nt",		command_newtab},
	{"tabnew",	command_newtab},
	{"gencert",	command_gencert},
	{"forget",	command_forget},
	{"download",	command_download},
	{"help",	command_help},
};
