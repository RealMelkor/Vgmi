#ifndef _GEMINI_H_
#define _GEMINI_H_

#include <unistd.h>

#define MAX_URL 1024
extern char gmi_url[1024];
extern char gmi_error[256];
extern char* gmi_data;
extern int gmi_code;
extern int gmi_lines;
extern char gmi_input[256];
int gmi_init();
int gmi_goto(int id);
int gmi_request(const char* url);
void gmi_load(char* data, size_t len);
int gmi_render(char* data, size_t len, int y);
int gmi_parseurl(const char* url, char* host, int host_len, char* urlbuf, int url_len);

#endif
