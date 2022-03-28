/* See LICENSE file for copyright and license details. */
#ifndef _GEMINI_H_
#define _GEMINI_H_

#include <unistd.h>
#include <stddef.h>

#define GMI 9
#define MAX_URL 1024
extern char** gmi_links;
extern size_t gmi_links_count;
extern int gmi_selected;
extern char gmi_url[1024];
extern char gmi_error[256];
extern char* gmi_data;
extern int gmi_code;
extern int gmi_lines;
extern char gmi_input[256];
struct gmi_link {
	char url[MAX_URL];
	struct gmi_link* prev;
	struct gmi_link* next;
};
extern struct gmi_link* gmi_history;
int gmi_init();
int gmi_goto(int id);
int gmi_request(const char* url);
void gmi_load(char* data, size_t len);
int gmi_render(char* data, size_t len, int y);
int gmi_parseurl(const char* url, char* host, int host_len, char* urlbuf, int url_len);
void gmi_cleanforward();
int gmi_nextlink(char* url, size_t url_len, char* link, size_t link_len);

#endif
