/* See LICENSE file for copyright and license details. */
#ifndef _GEMINI_H_
#define _GEMINI_H_

#include <unistd.h>
#include <stddef.h>

#define GMI 9
#define P_FILE 7
#define MAX_URL 1024

struct gmi_link {
	char url[MAX_URL];
	struct gmi_link* prev;
	struct gmi_link* next;
};

struct gmi_page {
	char** links;
	int links_count;
	char* data;
	int data_len;
	int code;
	int lines;
};

struct gmi_tab {
	struct gmi_link* history;
	struct gmi_page page;
	int scroll;
	int selected;
	char selected_url[MAX_URL];
	char url[MAX_URL];
};

struct gmi_client {
	char error[256];
	struct gmi_tab* tabs;
	int tabs_count;
	struct input {
		char label[MAX_URL];
		char field[MAX_URL];
		int error;
		int cursor;
		int mode;
	} input;
	struct vim {
		char counter[6];
		int g;
	} vim;
	int tab;
};

extern struct gmi_client client;
int gmi_init();
int gmi_goto(int id);
int gmi_request(const char* url);
void gmi_load(struct gmi_page* page);
int gmi_render(struct gmi_tab* tab);
int gmi_parseurl(const char* url, char* host, int host_len, char* urlbuf, int url_len);
void gmi_cleanforward(struct gmi_tab* tab);
int gmi_nextlink(char* url, char* link);
void gmi_newtab();
void gmi_newtab_url(char* url);
int gmi_loadfile(char* path);

#endif
