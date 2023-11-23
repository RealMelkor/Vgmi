/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#define LTS 0
#define MAJOR "2"
#define MINOR "0"
#if LTS
#define VERSION MAJOR "." MINOR " LTS"
#else
#define VERSION MAJOR "." MINOR " (" __DATE__ " " __TIME__ ")"
#endif

#ifdef ABOUT_INTERNAL
#pragma GCC diagnostic ignored "-Woverlength-strings"
#define HELP_INFO \
"## Keybindings\n\n" \
"* k - Scroll up\n" \
"* j - Scroll up\n" \
"* gT - Switch to the previous tab\n" \
"* gt - Switch to the next tab\n" \
"* h - Go back to the previous page\n" \
"* l - Go forward to the next page\n" \
"* gg - Go at the top of the page\n" \
"* G - Go at the bottom of the page\n" \
"* / - Open search mode\n" \
"* : - Open input mode\n" \
"* u - Open input mode with the current url\n" \
"* b - Open about:bookmarks in a new tab\n" \
"* f - Open about:history in a new tab\n" \
"* r - Reload the page\n" \
"* [number]Tab - Select link\n" \
"* Tab - Follow the selected link\n" \
"* Shift+Tab - Open the selected link in a new tab\n\n" \
"You can prefix a movement key with a number to repeat it.\n\n" \
"## Commands\n\n" \
"* :q - Close the current tab\n" \
"* :qa - Close all tabs, exit the program\n" \
"* :o [url] - Open an url\n" \
"* :s [search] - Search the Geminispace using geminispace.info\n" \
"* :nt [url] - Open a new tab, the url is optional\n" \
"* :add [name] - Add the current url to the bookmarks, the name is optional\n"\
"* :[number] - Scroll to the line number\n" \
"* :gencert - Generate a client-certificate for the current capsule\n" \
"* :forget <host> - Forget the certificate for the host\n" \
"* :download [name] - Download the current page, the name is optional\n" \
"* :help - Open about:help in a new tab\n"
#define HEADER "20 text/gemini\r\n"
static const char header[] = HEADER;
int about_history(char **out, size_t *length_out);
int about_bookmarks(char **out, size_t *length_out);
int about_bookmarks_param(const char *param);
int about_known_hosts(char **out, size_t *length_out);
int about_known_hosts_arg(const char *param);
int about_newtab(char **out, size_t *length_out);
int about_certificates(char **out, size_t *length_out);
int about_certificates_param(const char *param);
int about_config(char **out, size_t *length_out);
int about_config_arg(char *param, char **out, size_t *length_out);
void *dyn_strcat(char *dst, size_t *dst_length,
			const char *src, size_t src_len);
#endif

struct request;
int about_parse(struct request *request);
