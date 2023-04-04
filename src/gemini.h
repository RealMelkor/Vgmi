/* See LICENSE file for copyright and license details. */
#ifndef _GEMINI_H_
#define _GEMINI_H_

#define VERSION "1.5 LTS"

#include <netdb.h>
#include <unistd.h>
#include <stddef.h>
#include <pthread.h>
#include <poll.h>
#include <netinet/in.h>
#include "memcheck.h"
#include "url.h"

#define GMI 9
#define P_FILE 7
#define MAX_META 1024

#define MAX_TEXT_SIZE 4194304 // 4 MB
#define MAX_IMAGE_SIZE 33554432 // 32 MB

struct gmi_page {
	char** links;
	char title[64];
	int title_cached;
	int links_count;
	char* data;
	int data_len;
	int code;
	int lines;
	char meta[MAX_META];
#ifdef TERMINAL_IMG_VIEWER
	struct img {
		int tried;
		unsigned char* data;
		int w;
		int h;
		int channels;
	} img;
#endif
	int no_header;
};

struct gmi_link {
	char url[MAX_URL];
	struct gmi_link* prev;
	struct gmi_link* next;
	int scroll;
	struct gmi_page page;
	int cached;
};

#define STATE_DONE 0
#define STATE_REQUESTED 1
#define STATE_DNS 2
#define STATE_CONNECTING 3
#define STATE_CONNECTED 4
#define STATE_RECV_HEADER 5
#define STATE_RECV_BODY 6
#define STATE_CANCEL 7
#define STATE_STARTED 8

struct gmi_tab {
	struct gmi_tab* next;
	struct gmi_tab* prev;
	struct gmi_link* history;
	struct gmi_page page;
	int scroll;
	int selected;
	char selected_url[MAX_URL];
	char url[MAX_URL];
	char error[256];
	int show_error;
	char info[256];
	struct search { 
		char entry[MAX_URL];
		int cursor;
		int count;
		int scroll;
		int pos[2];
	} search;
	int show_info;
	// async
	int pair[2];
	struct request { 
#ifdef __linux__
		struct gaicb* gaicb_ptr;
		int resolved;
#endif
		pthread_t thread;
		pthread_mutex_t mutex;
		int loaded;
		int socket;
		struct tls* tls;
		int state;
		int ask;
		char info[1024];
		char action[128];
		unsigned short port;
		char host[256];
		union {
			struct sockaddr_in addr4;
			struct sockaddr_in6 addr6;
		} _addr;
		struct sockaddr* addr;
		int family;
		char url[1024];
		char meta[1024];
		char* data;
		int recv;
		int download;
		char error[1024];
	} request;
	struct thread {
		pthread_t thread;
		int started;
		int pair[2];
		char url[1024];
		int add;
	} thread;
	pthread_mutex_t render_mutex;
};

struct gmi_client {
	struct gmi_tab* tab;
	struct pollfd* tabs_fds;
	int tabs_count;
	struct input {
		char download[MAX_URL];
		char label[MAX_URL];
		char field[MAX_URL * 2];
		int cursor;
		int mode;
	} input;
	struct vim {
		char counter[6];
		int g;
	} vim;
	struct cert_cache {
		char host[1024];
		char* crt;
		size_t crt_len;
		char* key;
		size_t key_len;
	} *certs;
	int certs_size;
	char** bookmarks;
#ifndef DISABLE_XDG
	int xdg;
#endif
	int c256;
	int shutdown;
};

extern struct gmi_client client;
int gmi_init();
int gmi_goto(struct gmi_tab* tab, int id);
int gmi_goto_new(struct gmi_tab* tab, int id);
int gmi_request(struct gmi_tab* tab, const char* url, int add);
void gmi_load(struct gmi_page* page);
int gmi_render(struct gmi_tab* tab);
void gmi_cleanforward(struct gmi_tab* tab);
int gmi_nextlink(struct gmi_tab* tab, char* url, char* link);
struct gmi_tab* gmi_newtab();
struct gmi_tab* gmi_newtab_url(const char* url);
int gmi_loadfile(struct gmi_tab* tab, char* path);
void gmi_addbookmark(struct gmi_tab* tab, char* url, char* title);
void gmi_newbookmarks();
int gmi_loadbookmarks();
int gmi_savebookmarks();
void gmi_addtohistory(struct gmi_tab* tab);
int gmi_removebookmark(int index);
void gmi_polling();
void gmi_gohome(struct gmi_tab* tab, int add);
void gmi_gettitle(struct gmi_page* page, const char* url);
void gmi_freepage(struct gmi_page* page);
void gmi_freetab(struct gmi_tab* tab);
void gmi_free();

void fatal();
int fatalI();
void* fatalP();

#include "img.h"

#define WHITE (client.c256?15:TB_WHITE)
#define BLACK (client.c256?16:TB_BLACK)
#define BLUE (client.c256?25:TB_BLUE)
#define RED (client.c256?9:TB_RED)
#define GREEN (client.c256?2:TB_GREEN)
#define CYAN (client.c256?14:TB_CYAN)
#define MAGENTA (client.c256?13:TB_MAGENTA)
#define YELLOW (client.c256?58:TB_YELLOW)
#define GREY (client.c256?252:TB_WHITE)
#define ORANGE (client.c256?172:ITALIC|TB_WHITE)
#define ITALIC (client.c256?0:TB_ITALIC)

#endif
