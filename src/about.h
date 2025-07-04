/*
 * ISC License
 * Copyright (c) 2024 RMF <rawmonk@rmf-dev.com>
 */
#define LTS 0
#define MAJOR "2"
#define MINOR "1"
#if LTS
#define VERSION MAJOR "." MINOR " LTS"
#else
#define VERSION MAJOR "." MINOR " (" __DATE__ " " __TIME__ ")"
#endif

#ifdef ABOUT_INTERNAL
#pragma GCC diagnostic ignored "-Woverlength-strings"
#define HELP_INFO \
"## Keybindings\n\n" \
"### Standard\n\n" \
"* Arrow Up - Scroll up\n" \
"* Arrow Down - Scroll down\n" \
"* Ctrl+t - Open new tab\n" \
"* Ctrl+w - Close tab\n" \
"* Ctrl+PgUp - Switch to the previous tab\n" \
"* Ctrl+PgDn - Switch to the next tab\n" \
"* Alt+Arrow Left - Go back to the previous page\n" \
"* Alt+Arrow Right - Go forward to the next page\n" \
"* Home - Scroll to the top of the page\n" \
"* End - Scroll to the bottom of the page\n" \
"* Ctrl+f - Open search mode\n" \
"* Ctrl+b - Open about:bookmarks in a new tab\n" \
"* Ctrl+h - Open about:history in a new tab\n" \
"* Ctrl+r, F5 - Reload the page\n" \
"* Ctrl+g - Jump to next search occurence\n" \
"* Left click - Open link\n" \
"* Middle click - Open link in a new tab\n\n" \
"### Vi\n\n" \
"* k - Scroll up\n" \
"* j - Scroll down\n" \
"* gT - Switch to the previous tab\n" \
"* gt - Switch to the next tab\n" \
"* h - Go back to the previous page\n" \
"* l - Go forward to the next page\n" \
"* gg - Scroll to the top of the page\n" \
"* G - Scroll to the bottom of the page\n" \
"* / - Open search mode\n" \
"* : - Open input mode\n" \
"* u - Open input mode with the current url\n" \
"* b - Open about:bookmarks in a new tab\n" \
"* f - Open about:history in a new tab\n" \
"* r - Reload the page\n" \
"* n - Jump to next search occurence\n" \
"* N - Jump to previous search occurence\n" \
"* [number]Tab - Select link\n" \
"* Tab - Follow the selected link\n" \
"* Shift+Tab - Open the selected link in a new tab\n\n" \
"You can prefix a movement key with a number to repeat it.\n\n" \
"## Commands\n\n" \
"* :q - Close the current tab\n" \
"* :qa - Close all tabs, exit the program\n" \
"* :e - Refresh page\n" \
"* :open [url] - Open an url (alias: ':o')\n" \
"* :s [search] - Search the Geminispace using a search engine\n" \
"* :tabnew [url] - Open a new tab, the url is optional (alias: ':nt')\n" \
"* :tabprev - Go to the previous tab\n" \
"* :tabnext - Go to the next tab\n" \
"* :add [name] - Add the current url to the bookmarks, the name is optional\n"\
"* :[number] - Scroll to the line number\n" \
"* :gencert - Generate a client-certificate for the current capsule\n" \
"* :forget <host> - Forget the certificate for the host\n" \
"* :download [name] - Download the current page, the name is optional\n" \
"* :exec - Open the current page in an external program\n" \
"* :help - Open about:help in a new tab\n"
#define HEADER "20 text/gemini\r\n"
static const char header[] = HEADER;
int about_history(char **out, size_t *length_out);
int about_history_param(const char *param);
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
void *dyn_utf8_strcat(char *dst, size_t *dst_length,
			const char *src, size_t src_len);
#endif

struct request;
int about_parse(struct request *request);
