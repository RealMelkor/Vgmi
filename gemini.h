#ifndef _GEMINI_H_
#define _GEMINI_H_

#include <unistd.h>

extern char gmi_url[1024];
extern char gmi_error[256];
extern char* gmi_data;
extern int gmi_lines;
int gmi_init();
int gmi_goto(int id);
int gmi_request(const char* url);
void gmi_load(char* data, size_t len);
void gmi_render(char* data, size_t len, int y);

#endif
