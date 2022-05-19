/* See LICENSE file for copyright and license details. */
#ifdef __linux__
#define _GNU_SOURCE
#endif
#include <netinet/in.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <tls.h>
#include <poll.h>
#include <fcntl.h>
#include <errno.h>
#include <termbox.h>
#include <ctype.h>
#include <time.h>
#include "gemini.h"
#ifdef __linux__
#include <bsd/string.h>
#endif
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#include <pthread.h>
#include <sys/socket.h>
#endif
#include "cert.h"
#include "wcwidth.h"
#include "display.h"

#define MAX_CACHE 10
#define TIMEOUT 3
struct timespec timeout = {0, 50000000};

struct tls_config* config;
struct tls_config* config_empty;
struct tls* ctx;
struct gmi_client client;

void tb_colorline(int x, int y, uintattr_t color);

void fatal() {
	tb_shutdown();
	printf("Failed to allocate memory, terminating\n");
	exit(0);
}

int fatalI() {
	fatal();
	return -1;
}

void* fatalP() {
	fatal();
	return NULL;
}

int getbookmark(char* path, size_t len) {
	int length = getcachefolder(path, len);
	const char bookmark[] = "/bookmarks.txt";
	if (length + sizeof(bookmark) >= len) return -1;
	return length + strlcpy(&path[length], bookmark, len - length);
}

int gmi_parseuri(const char* url, int len, char* buf, int llen) {
	int j = 0;
	int inquery = 0;
	for (int i=0; j < llen && i < len && url[i]; i++) {
		if (url[i] == '/') inquery = 0;
		if ((url[i] >= 'a' && url[i] <= 'z') ||
		    (url[i] >= 'A' && url[i] <= 'Z') ||
		    (url[i] >= '0' && url[i] <= '9') ||
		    (url[i] == '?' && !inquery) ||
		    url[i] == '.' || url[i] == '/' ||
		    url[i] == ':' || url[i] == '-') {
			if (url[i] == '?') inquery = 1;
			buf[j] = url[i];
			j++;
		} else {
			char format[8];
			snprintf(format, sizeof(format), "%%%x", (unsigned char)url[i]);
			buf[j] = '\0';
			j = strlcat(buf, format, llen);
		}
	}
	if (j >= llen) j = llen - 1;
	buf[j] = '\0';
	return j;
}

int gmi_goto(struct gmi_tab* tab, int id) {
	id--;
	struct gmi_page* page = &tab->page;
	if (id < 0 || id >= page->links_count) {
		snprintf(tab->error, sizeof(tab->error),
			 "Invalid link number, %d/%d", id, page->links_count);
		client.input.error = 1;
		return -1;
	}
	gmi_cleanforward(tab);
	int ret = gmi_nextlink(tab, tab->url, page->links[id]);
	if (ret < 1) return ret;
	tab->page.data_len = ret;
	gmi_load(page);
	tab->scroll = -1;
	return ret;
}

int gmi_goto_new(struct gmi_tab* tab, int id) {
	id--;
	struct gmi_page* page = &tab->page;
	if (id < 0 || id >= page->links_count) {
		snprintf(tab->error, sizeof(tab->error),
			 "Invalid link number, %d/%d", id, page->links_count);
		client.input.error = 1;
		return -1;
	}
	int old_tab = client.tab;
	struct gmi_tab* new_tab = gmi_newtab_url(NULL);
	client.tab = client.tabs_count - 1;
	page = &client.tabs[old_tab].page;
	int ret = gmi_nextlink(new_tab, client.tabs[old_tab].url, page->links[id]);
	if (ret < 1) return ret;
	new_tab->page.data_len = ret;
	gmi_load(&new_tab->page);
	return ret;
}

int gmi_nextlink(struct gmi_tab* tab, char* url, char* link) {
	int url_len = strnlen(url, MAX_URL);
	if (!strcmp(link, "about:home")) {
		gmi_gohome(tab);
		return 0;
	} else if (link[0] == '/' && link[1] == '/') {
		int ret = gmi_request(tab, &link[2]);
		if (ret < 1) return ret;
		return ret;
	} else if (link[0] == '/') {
		if (url_len > GMI && strstr(url, "gemini://")) {
			char* ptr = strchr(&url[GMI], '/');
			if (ptr) *ptr = '\0';
		}
		char urlbuf[MAX_URL];
		size_t l = strlcpy(urlbuf, url, sizeof(urlbuf));
		if (l >= sizeof(urlbuf))
			goto nextlink_overflow;
		if (strlcpy(urlbuf + l, link, sizeof(urlbuf) - l) >= sizeof(urlbuf) - l)
			goto nextlink_overflow;
		int ret = gmi_request(tab, urlbuf);
		return ret;
	} else if (strstr(link, "https://") ||
		   strstr(link, "http://") ||
		   strstr(link, "gopher://")) {
#ifndef DISABLE_XDG
		if (client.xdg) {
			char buf[1048];
			snprintf(buf, sizeof(buf), "xdg-open %s > /dev/null 2>&1", link);
#ifdef __OpenBSD__
			if (fork() == 0) {
				setsid();
				char* argv[] = {"/bin/sh", "-c", buf, NULL};
				execvp(argv[0], argv);
				exit(0);
			}
#else
			system(buf);
#endif
			return -1;
		}
#endif
		client.input.error = 1;
		snprintf(tab->error, sizeof(tab->error), 
			 "Can't open web link");
		return -1;
	} else if (strstr(link, "gemini://")) {
		int ret = gmi_request(tab, link);
		return ret;
	} else {
		char* ptr = strrchr(&url[GMI], '/');
		if (ptr) *ptr = '\0';
		char urlbuf[MAX_URL];
		size_t l = strlcpy(urlbuf, url, sizeof(urlbuf));
		if (l >= sizeof(urlbuf))
			goto nextlink_overflow;
		if (urlbuf[l-1] != '/') {
			size_t l2 = strlcpy(urlbuf + l, "/", sizeof(urlbuf) - l);
			if (l2 >= sizeof(urlbuf) - l)
				goto nextlink_overflow;
			l += l2;
		}
		size_t l2 = strlcpy(urlbuf + l, link, sizeof(urlbuf) - l);
		if (l2 >= sizeof(urlbuf) - l)
			goto nextlink_overflow;
		l += l2;
		if (urlbuf[l-1] == '/') urlbuf[l-1] = '\0';
		int ret = gmi_request(tab, urlbuf);
		return ret;
	}
nextlink_overflow:
	client.input.error = 1;
	snprintf(tab->error, sizeof(tab->error),
	 "Link too long, above 1024 characters");
	return -1;
}

void gmi_load(struct gmi_page* page) {
	for (int i=0; i<page->links_count; i++)
		free(page->links[i]);
	free(page->links);
	page->links = NULL;
	page->links_count = 0;
	page->lines = 0;
	int x = 0;
	for (int c = 0; c < page->data_len; c++) {
		if (x == 0 && page->data[c] == '=' && page->data[c+1] == '>') {
			c += 2;
			int nospace = c;
			for (; page->data[c]==' ' || page->data[c]=='\t'; c++) {
				if (page->data[c] == '\n' || page->data[c] == '\0') {
					c = nospace;
					break;
				}
			}
			char* url = (char*)&page->data[c];
			for (; page->data[c]!=' ' && page->data[c]!='\t'; c++) {
				if (page->data[c] == '\n' || page->data[c] == '\0') {
					break;
				}
			}
			char save = page->data[c];
			page->data[c] = '\0';
			if (page->links)
				page->links = realloc(page->links, sizeof(char*) 
						      * (page->links_count+1));
			else
				page->links = malloc(sizeof(char*));
			if (!page->links) {
				fatal();
				return;
			}
			if (url[0] == '\0') {
				page->links[page->links_count] = NULL;
				page->data[c] = save;
				continue;
			}
			int len = strnlen(url, MAX_URL);
			page->links[page->links_count] = malloc(len+2);
			if (!page->links[page->links_count]) {
				fatal();
				return;
			}
			memcpy(page->links[page->links_count], url, len+1);
			page->links_count++;
			page->data[c] = save;
		}
		if (page->data[c] == '\n') {
			page->lines++;
			x = 0;
			continue;
		}
		x++;
	}
}

int gmi_render(struct gmi_tab* tab) {
#include "img.h"
#ifdef TERMINAL_IMG_VIEWER
	if (strncmp(tab->page.meta, "image/", 6) == 0) {
		if (!tab->page.img.tried) {
			char* ptr = strchr(tab->page.data, '\n');
			if (!ptr) {
				tb_printf(2, -tab->scroll, TB_DEFAULT, TB_DEFAULT,
					  "Invalid data: new line not found");
				tab->page.img.tried = 1;
				return 1;
			}
			ptr++;
			if (tab->page.img.data) {
				stbi_image_free(tab->page.img.data);
				tab->page.img.data = NULL;
			}
			tab->page.img.data = 
			stbi_load_from_memory((unsigned char*)ptr, 
						tab->page.data_len-(int)(ptr-tab->page.data),
						&tab->page.img.w, &tab->page.img.h,
						&tab->page.img.channels, 3);
			if (!tab->page.img.data) {
				tb_printf(2, -tab->scroll, TB_DEFAULT, TB_DEFAULT,
					  "Failed to decode image: %s;", tab->page.meta);
				tab->page.img.tried = 1;
				return 1;
			}

			tab->page.img.tried = 1;
		}
		if (tab->page.img.data) {
			img_display(tab->page.img.data,
					tab->page.img.w, tab->page.img.h,
					client.tabs_count>1);
			return 1;
		}
	}
#endif

	int text = 0;
	if (strncmp(tab->page.meta, "text/gemini", 11)) {
		if (strncmp(tab->page.meta, "text/", 5)) {
			tb_printf(2, -tab->scroll, TB_DEFAULT, TB_DEFAULT,
				  "Unable to render format : %s", tab->page.meta);
			return 1;
		}
		text = 1;
	}
	int line = 0;
	int x = 0;
	int links = 0;
	uintattr_t color = TB_DEFAULT;
	int start = 1;
	int ignore = 0;
	int h = tb_height() - 2 - (client.tabs_count>1);
	for (int c = 0; c < tab->page.data_len; c++) {
		if (x == 0 && tab->page.data[c] == '\n') {
			if (c+1 != tab->page.data_len)
				line++;
			continue;
		}
		if (tab->page.data[c] == '\t') {
			x+=4;
			continue;
		}
		if (tab->page.data[c] == '\r') continue;
		if (!text && start && tab->page.data[c] == '`' &&
		    tab->page.data[c+1] == '`' && tab->page.data[c+2] == '`') 
			ignore = !ignore;	
		
		if (!ignore && !text) {
			for (int i=0; start && tab->page.data[c+i] == '#' && i<3; i++) {
				if (tab->page.data[c+i+1] == ' ') {
					color = TB_RED + i;
					break;
				}
			}
			if (start && tab->page.data[c] == '*' && tab->page.data[c+1] == ' ') {
				color = TB_ITALIC|TB_CYAN;
			}
			if (start && tab->page.data[c] == '>' && tab->page.data[c+1] == ' ') {
				color = TB_ITALIC|TB_MAGENTA;
			}
			if (start && tab->page.data[c] == '=' && tab->page.data[c+1] == '>') {
				if (line-1>=(tab->scroll>=0?tab->scroll:0) &&
				   line-tab->scroll <= tb_height()-2) {
					char buf[32];
					snprintf(buf, sizeof(buf), "[%d]", links+1);
					tb_print(x+2, line-1-tab->scroll,
						 links+1 == tab->selected?TB_RED:TB_BLUE,
						 TB_DEFAULT, buf);
					x += strnlen(buf, sizeof(buf));
				}
				c += 2;

				while (
				(tab->page.data[c]==' ' || tab->page.data[c]=='\t') && 
				tab->page.data[c]!='\n' &&
				tab->page.data[c]!='\0') c++;

				int initial = c;
				while (tab->page.data[c]!=' ' &&
				tab->page.data[c]!='\t' && 
				tab->page.data[c]!='\n' &&
				tab->page.data[c]!='\0') c++;

				while ((tab->page.data[c]==' ' || tab->page.data[c]=='\t') && 
				tab->page.data[c]!='\n' &&
				tab->page.data[c]!='\0') c++;

				if (tab->page.data[c]=='\n' || tab->page.data[c]=='\0')
					c = initial;
				x+=3;
				if ((links+1)/10) x--;
				if ((links+1)/100) x--;
				if ((links+1)/1000) x--;
				links++;
			}
		}

		if (tab->page.data[c] == '\n' || tab->page.data[c] == ' ' ||
		    x+4 >= tb_width()) {
			int end = 0;
			if (x+4>=tb_width()) {
				end = 1;
			}
			int newline = (tab->page.data[c] == '\n' || x+4 >= tb_width());
			for (int i=1; 1; i++) {
				if (x+i == tb_width() - 1) break;
				if (i > tb_width() - 4) {
					newline = 0;
					break;
				}
				if (tab->page.data[c+i] == ' ' || tab->page.data[c+i] == '\n' ||
				    tab->page.data[c+i] == '\0' || c+i >= tab->page.data_len)
					break;
				if (tb_width()-4<=x+i) newline = 1;
			}
			if (newline) {
				if (c+1 != tab->page.data_len)
					line++;
				if (tab->page.data[c+1] == ' ') c++;
				if (tab->page.data[c] == '\n') {
					color = TB_DEFAULT;
					start = 1;
				}
				x = 0;
				continue;
			} else if (end) {
				c++;
				x++;
			}
		}
		uint32_t ch = 0;
		int size = tb_utf8_char_to_unicode(&ch, &tab->page.data[c])-1;
		if (size > 0)
			c += tb_utf8_char_to_unicode(&ch, &tab->page.data[c])-1;
		
		int wc = mk_wcwidth(ch);
		if (wc < 0) wc = 0;
		if (line-1>=(tab->scroll>=0?tab->scroll:0) &&
		    (line-tab->scroll <= tb_height() - 2) && ch != '\t') {
			if (wc == 1)
				tb_set_cell(x+2, line-1-tab->scroll, ch, color, TB_DEFAULT);
			else
				tb_set_cell_ex(x+2, line-1-tab->scroll, &ch,
					       wc, color, TB_DEFAULT);
		}

		x += wc;
		start = 0;
	}
	line++;
	h += (client.tabs_count>1);
	if (h > line) return line;
	int size = (h==line?(h-1):(h/(line-h))); 
	if (size == h) size--;
	if (size < 1) size = 1;
	int H = (line-h+1+(client.tabs_count>1));
	int pos = (tab->scroll+(client.tabs_count<1))*h/(H?H:1) 
		  + (client.tabs_count>1) ;
	if (pos >= h) pos = h - size;
	if (pos < 0) pos = 0;
	if (tab->scroll+h == line) pos = h - size;
	int w = tb_width();
	for (int y = (client.tabs_count>1); y < h+(client.tabs_count>1); y++)
		if (y >= pos && y < pos + size)
			tb_set_cell(w-1, y, ' ', TB_DEFAULT, client.c256?246:TB_CYAN);
		else
			tb_set_cell(w-1, y, ' ', TB_DEFAULT, client.c256?233:TB_BLACK);
	return line;
}

void gmi_addtohistory(struct gmi_tab* tab) {
	if (!(!tab->history || (tab->history && !tab->history->next))) return;
	gmi_cleanforward(tab);
	struct gmi_link* link = malloc(sizeof(struct gmi_link));
	if (!link) {
		fatal();
		return;
	}
	link->next = NULL;
	link->prev = tab->history;
	link->scroll = tab->scroll;
	strlcpy(link->url, tab->url, sizeof(link->url));
	tab->history = link;
	if (link->prev)
		link->prev->next = tab->history;
}

void gmi_cleanforward(struct gmi_tab* tab) {
	if (!tab->history)
		return;
	struct gmi_link* link = tab->history->next;
	while (link) {
		struct gmi_link* ptr = link->next;
		bzero(link->url, sizeof(link->url));
		if (link->cached) {
			gmi_freepage(&link->page);
			link->cached = 0;
		}
		free(link);
		link = ptr;
	}
	tab->history->next = NULL;
}

void gmi_freepage(struct gmi_page* page) {
	free(page->data);
	for (int i=0; i<page->links_count; i++)
		free(page->links[i]);
	free(page->links);
#ifdef TERMINAL_IMG_VIEWER
	stbi_image_free(page->img.data);
#endif
	bzero(page, sizeof(struct gmi_page));
}

void gmi_freetab(struct gmi_tab* tab) {
	if (tab->history) {
		struct gmi_link* link = tab->history->next;
		while (link) {
			struct gmi_link* ptr = link->next;
			bzero(link->url, sizeof(link->url));
			if (link->cached) {
				gmi_freepage(&link->page);
				link->cached = 0;
			}
			free(link);
			link = ptr;
		}
		link = tab->history->prev;
		while (link) {
			struct gmi_link* ptr = link->prev;
			bzero(link->url, sizeof(link->url));
			if (link->cached) {
				gmi_freepage(&link->page);
				link->cached = 0;
			}
			free(link);
			link = ptr;
		}
		bzero(tab->history->url, sizeof(tab->history->url));
		free(tab->history);
	}
	gmi_freepage(&tab->page);
	bzero(tab, sizeof(struct gmi_tab));
}


char home_page[] = 
"20 text/gemini\r\n# Vgmi\n\n" \
"A Gemini client written in C with vim-like keybindings\n\n" \
"## Bookmarks\n\n" \
"%s\n" \
"## Keybindings\n\n" \
"* k - Scroll up\n" \
"* j - Scroll down\n" \
"* h - Switch to the previous tab\n" \
"* l - Switch to the next tab\n" \
"* H - Go back in the history\n" \
"* L - Go forward in the history\n" \
"* gg - Go at the top of the page\n" \
"* G - Go at the bottom of the page\n" \
"* : - Open input mode\n" \
"* u - Open input mode with the current url\n" \
"* f - Show the history\n" \
"* r - Reload the page\n" \
"* [number]Tab - Select a link\n" \
"* Tab - Follow the selected link\n" \
"* Shift+Tab - Open the selected link in a new tab\n" \
"* Del - Delete the selected link from the bookmarks\n" \
"\nYou can prefix a movement key with a number to repeat it.\n\n" \
"## Commands\n\n" \
"* :q - Close the current tab\n" \
"* :qa - Close all tabs, exit the program\n" \
"* :o [url] - Open an url\n" \
"* :s [search] - Search the Geminispace using geminispace.info\n" \
"* :nt [url] - Open a new tab, the url is optional\n" \
"* :add [name] - Add the current url to the bookmarks, the name is optional\n" \
"* :[number] - Follow the link\n" \
"* :gencert - Generate a certificate for the current capsule"
;

void gmi_newbookmarks() {
	int len;
	const char geminispace[] = "gemini://geminispace.info Geminispace";
	const char gemigit[] = "gemini://gemini.rmf-dev.com Gemigit";
	client.bookmarks = malloc(sizeof(char*) * 3);
	if (!client.bookmarks) goto fail_malloc;

	len = sizeof(geminispace);
	client.bookmarks[0] = malloc(len);
	if (!client.bookmarks[0]) goto fail_malloc;
	strlcpy(client.bookmarks[0], geminispace, len);

	len = sizeof(gemigit);
	client.bookmarks[1] = malloc(len);
	if (!client.bookmarks[1]) goto fail_malloc;
	strlcpy(client.bookmarks[1], gemigit, len);

	client.bookmarks[2] = NULL;
	return;
fail_malloc:
	fatal();
}

int gmi_loadbookmarks() {
	char path[1024];
	if (getbookmark(path, sizeof(path)) < 1) return -1;
	FILE* f = fopen(path, "rb");
	if (!f)
		return -1;
	fseek(f, 0, SEEK_END);
	size_t len = ftell(f);
	fseek(f, 0, SEEK_SET);
	char* data = malloc(len);
	if (len != fread(data, 1, len, f)) {
		fclose(f);
		return -1;
	}
	fclose(f);
	char* ptr = data;
	long n = 0;
	while (++ptr && ptr < data+len) if (*ptr == '\n') n++;
	client.bookmarks = malloc(sizeof(char*) * (n + 1));
	client.bookmarks[n] = NULL;
	n = 0;
	ptr = data;
	char* str = data;
	while (++ptr && ptr < data+len) {
		if (*ptr == '\n') {
			*ptr = '\0';
			client.bookmarks[n] = malloc(ptr-str+1);
			strlcpy(client.bookmarks[n], str, ptr-str+1);
			n++;
			str = ptr+1;
		}
	}
	free(data);
	return 0;
}

char* gmi_gettitle(struct gmi_page page, int* len) {
	int start = -1;
	int end = -1;
	for (int i=0; i < page.data_len; i++) {
		if (start == -1 && page.data[i] == '#') {
			for (int j = i+1; j < page.data_len; j++) {
				if (j && page.data[j-1] == '#' && page.data[j] == '#')
					break;
				if (page.data[j] != ' ') {
					start = j;
					break;
				}
			}
		}
		if (start != -1 && page.data[i] == '\n') {
			end = i;
			break;
		}
	}
	if (start == -1 || end == -1) return NULL;
	char* title = malloc(end - start + 1);
	strlcpy(title, &page.data[start], end - start + 1);
	title[end - start] = '\0'; 
	*len = end - start + 1;
	return title;
}

int gmi_removebookmark(int index) {
	index--;
	if (index < 0) return -1;
	int fail = -1;
	for (int i = 0; client.bookmarks[i]; i++) {
		if (i == index) {
			free(client.bookmarks[i]);
			fail = 0;
		}
		if (!fail)
			client.bookmarks[i] = client.bookmarks[i+1];
	}
	return fail;
}

void gmi_addbookmark(struct gmi_tab* tab, char* url, char* title) {
	if (!strcmp(url, "about:home")) {
		snprintf(tab->error, sizeof(tab->error),
			 "Cannot add the new tab page to the bookmarks");
		client.input.error = 1;
		return;
	}
	int title_len = 0;
	char* tmp_title = NULL;
	if (!title) tmp_title = title = gmi_gettitle(tab->page, &title_len);
	else title_len = strnlen(title, 128);
	long n = 0;
	while (client.bookmarks[n]) n++;
	client.bookmarks = realloc(client.bookmarks, sizeof(char*) * (n + 2));
	int len = strnlen(url, MAX_URL) + title_len + 2;
	client.bookmarks[n] = malloc(len);
	if (title)
		snprintf(client.bookmarks[n], len, "%s %s", url, title);
	else
		snprintf(client.bookmarks[n], len, "%s", url);
	client.bookmarks[n+1] = NULL;
	free(tmp_title);
}

int gmi_savebookmarks() {
	char path[1024];
	if (getbookmark(path, sizeof(path)) < 1) return -1;
	FILE* f = fopen(path, "wb");
	if (!f) {
		return -1;
	}
	for (int i = 0; client.bookmarks[i]; i++)
		fprintf(f, "%s\n", client.bookmarks[i]);
	fclose(f);
	return 0;

}

char* gmi_getbookmarks(int* len) {
	char* data = NULL;
	int n = 0;
	for (int i = 0; client.bookmarks[i]; i++) {
		char line[2048];
		long length = snprintf(line, sizeof(line), "=>%s\n ", client.bookmarks[i]);
		data = realloc(data, n+length+1);
		if (!data) return fatalP();
		strlcpy(&data[n], line, length);
		n += length-1;
	}
	*len = n;
	return data;
}

void gmi_gohome(struct gmi_tab* tab) {
	free(tab->page.data);
	tab->scroll = -1;
	strlcpy(tab->url, "about:home", sizeof(tab->url)); 
	tab->page.code = 20;
	int bm;
	char* data = gmi_getbookmarks(&bm);
	tab->page.data = malloc(sizeof(home_page) + bm);
	if (!tab->page.data) {
		fatal();
		return;
	}

	tab->page.data_len = 
	snprintf(tab->page.data, sizeof(home_page) + bm, home_page, 
		 data?data:"") + 1;
	free(data);

	strlcpy(tab->page.meta, "text/gemini",
		sizeof(tab->page.meta));

	gmi_load(&tab->page);
	gmi_addtohistory(tab);
}

struct gmi_tab* gmi_newtab() {
	return gmi_newtab_url("about:home");
}

struct gmi_tab* gmi_newtab_url(const char* url) {
	int tab = client.tabs_count;
	client.tabs_count++;
	if (client.tabs) 
		client.tabs = realloc(client.tabs, sizeof(struct gmi_tab) * client.tabs_count);
	else
		client.tabs = malloc(sizeof(struct gmi_tab));
	if (!client.tabs) return fatalP();
	bzero(&client.tabs[tab], sizeof(struct gmi_tab));
	if (url)
		gmi_request(&client.tabs[tab], url);
	client.tabs[tab].scroll = -1;
	return &client.tabs[tab];
}

#define PROTO_GEMINI 0
#define PROTO_HTTP 1
#define PROTO_HTTPS 2
#define PROTO_GOPHER 3
#define PROTO_FILE 4

int gmi_parseurl(const char* url, char* host, int host_len, char* urlbuf,
		 int url_len, unsigned short* port) {
	int proto = PROTO_GEMINI;
	char* proto_ptr = strstr(url, "://");
	char* ptr = (char*)url;
	if (!proto_ptr) {
		goto skip_proto;			
	}
	char proto_buf[16];
	for(; proto_ptr!=ptr; ptr++) {
		if (!((*ptr > 'a' && *ptr < 'z') || (*ptr > 'A' && *ptr < 'Z'))) goto skip_proto;
		if (ptr - url >= (signed)sizeof(proto_buf)) goto skip_proto;
		proto_buf[ptr-url] = tolower(*ptr);
	}
	proto_buf[ptr-url] = '\0';
	ptr+=3;
	proto_ptr+=3;
	if (!strcmp(proto_buf,"gemini")) goto skip_proto;
	else if (!strcmp(proto_buf,"http")) proto = PROTO_HTTP;
	else if (!strcmp(proto_buf,"https")) proto = PROTO_HTTPS;
	else if (!strcmp(proto_buf,"gopher")) proto = PROTO_GOPHER;
	else if (!strcmp(proto_buf,"file")) proto = PROTO_FILE;
	else {
		return -1; // unknown protocol
	}
skip_proto:;
	if (port && proto == PROTO_GEMINI) *port = 1965;
	if (!proto_ptr) proto_ptr = ptr;
	char* host_ptr = strchr(ptr, '/');
	if (!host_ptr) host_ptr = ptr+strnlen(ptr, MAX_URL);
	char* port_ptr = strchr(ptr, ':');
	if (port_ptr && port_ptr < host_ptr) {
		port_ptr++;	
		char c = *host_ptr;
		*host_ptr = '\0';
		if (port) {
			*port = atoi(port_ptr);
			if (*port < 1) {
				return -1; // invalid port
			}
		}
		*host_ptr = c;
		host_ptr = port_ptr - 1;
	}
	for(; host_ptr!=ptr; ptr++) {
		if (host_len <= host_ptr-ptr) {
			return -1;
		}
		if (!((*ptr >= 'a' && *ptr <= 'z') || (*ptr >= 'A' && *ptr <= 'Z')
		|| (*ptr >= '0' && *ptr <= '9') || *ptr == '.' || *ptr == '-')) {
			return -1;
		}
		host[ptr-proto_ptr] = *ptr;
	}
	host[ptr-proto_ptr] = '\0';
	if (!urlbuf) return proto;
	if (url_len < 16) return -1; // buffer too small
	int len = 0;
	switch (proto) {
	case PROTO_GEMINI:
		len = strlcpy(urlbuf, "gemini://", url_len);
		break;
	case PROTO_HTTP:
		len = strlcpy(urlbuf, "http://", url_len);
		break;
	case PROTO_HTTPS:
		len = strlcpy(urlbuf, "http://", url_len);
		break;
	case PROTO_GOPHER:
		len = strlcpy(urlbuf, "gopher://", url_len);
		break;
	case PROTO_FILE:
		len = strlcpy(urlbuf, "file://", url_len);
		break;
	default:
		return -1;
	}
	size_t l = strlcpy(urlbuf + len, host, sizeof(urlbuf) - len);
	if (l >= sizeof(urlbuf) - len) {
		goto parseurl_overflow;
	}
	len += l;
	if (host_ptr &&
	    strlcpy(urlbuf + len, host_ptr, sizeof(urlbuf) - len) >= 
	    sizeof(urlbuf) - len)
		goto parseurl_overflow;
	return proto;
parseurl_overflow:
	client.input.error = 1;
	struct gmi_tab* tab = &client.tabs[client.tab];
	snprintf(tab->error, sizeof(tab->error),
		 "Link too long, above 1024 characters");
	return -1;
}

#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
struct conn {
	int connected;
	int socket;
	struct sockaddr* addr;
	int family;
	struct sockaddr_in addr4;
	struct sockaddr_in6 addr6;
	pthread_t tid;
	pthread_cond_t cond;
	pthread_mutex_t mutex;
};
struct conn conn;

void conn_thread() {
	if (pthread_mutex_init(&conn.mutex, NULL)) {
		struct gmi_tab* tab = &client.tabs[client.tab];
		snprintf(tab->error, sizeof(tab->error), 
			"Failed to initialize connection mutex\n");
		return;
	}
	pthread_mutex_lock(&conn.mutex);
	while (1) {
		pthread_cond_wait(&conn.cond, &conn.mutex);
		if (connect(conn.socket, conn.addr, 
		(conn.family == AF_INET)?sizeof(conn.addr4):sizeof(conn.addr6)) != 0) {
			conn.connected = -1;
			continue;
		}
		conn.connected = 1;
	}
}

struct dnsquery {
	struct addrinfo* result;
	int resolved;
	char* host;
	pthread_t tid;
	pthread_cond_t cond;
	pthread_mutex_t mutex;
};
struct dnsquery dnsquery;

void dnsquery_thread(int signal) {
	if (signal != 0 && signal != SIGUSR1) return;
	if (pthread_mutex_init(&dnsquery.mutex, NULL)) {
		struct gmi_tab* tab = &client.tabs[client.tab];
		snprintf(tab->error, sizeof(tab->error), 
			"Failed to initialize connection mutex\n");
		return;
	}
	pthread_mutex_lock(&dnsquery.mutex);
	while (1) {
		pthread_cond_wait(&dnsquery.cond, &dnsquery.mutex);
		struct addrinfo hints;
		memset (&hints, 0, sizeof (hints));
		hints.ai_family = PF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags |= AI_CANONNAME;
		if (getaddrinfo(dnsquery.host, NULL, &hints, &dnsquery.result)) {
			dnsquery.resolved = -1;
			continue;
		}
		dnsquery.resolved = 1;
	}
}

void signal_cb() {
	pthread_t thread = pthread_self();
	if (thread == conn.tid)
		pthread_mutex_destroy(&conn.mutex);
	if (thread == dnsquery.tid)
		pthread_mutex_destroy(&dnsquery.mutex);
	pthread_exit(NULL);
}

#endif

int gmi_request(struct gmi_tab* tab, const char* url) {
	if (!strcmp(url, "about:home")) {
		gmi_gohome(tab);
		return tab->page.data_len;
	}
	client.input.error = 0;
	char* data_buf = NULL;
	char meta_buf[1024];
	char url_buf[1024];
	int download = 0;
	if (tab->history) tab->history->scroll = tab->scroll;
	tab->selected = 0;
	int recv = TLS_WANT_POLLIN;
	int sockfd = -1;
	char gmi_host[256];
	gmi_host[0] = '\0';
	unsigned short port;
	int proto = gmi_parseurl(url, gmi_host, sizeof(gmi_host),
				 url_buf, sizeof(url_buf), &port);
	if (proto == PROTO_FILE) {
		if (gmi_loadfile(tab, &url_buf[P_FILE]) > 0)
			gmi_load(&tab->page);
		return 0;
	}
	if (proto != PROTO_GEMINI) {
		snprintf(tab->error, sizeof(tab->error), "Invalid url: %s", url);
		client.input.error = 1;
		return -1;
	}
	if (ctx) {
		tls_close(ctx);
		ctx = NULL;
	}
	ctx = tls_client();
	if (!ctx) {
		snprintf(tab->error, sizeof(tab->error), "Failed to initialize TLS");
		client.input.error = 1;
		return -1;
	}

	int cert = 0;
	char crt[1024];
	char key[1024];
	if (cert_getpath(gmi_host, crt, sizeof(crt), key, sizeof(key)) == -1) {
		client.input.error = 1;
		cert = -1;
	}

	if (cert == 0) {
		FILE* f = fopen(crt, "rb");
		if (f) {
			cert = 1;
			fclose(f);
		}
	}

	if (cert == 1 && tls_config_set_keypair_file(config, crt, key)) {
		client.input.error = 1;
		snprintf(tab->error, sizeof(tab->error), "%s", 
			 tls_config_error(config));
		return -1;
	}

	if (tls_configure(ctx, cert?config:config_empty)) {
		snprintf(tab->error, sizeof(tab->error),
			 "Failed to configure TLS");
		client.input.error = 1;
		return -1;
	}

	// Manually create socket to set a timeout
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
	dnsquery.host = gmi_host;
	dnsquery.resolved = 0;
	pthread_cond_signal(&dnsquery.cond);
	for (int i=0; i < TIMEOUT * 20 && !dnsquery.resolved; i++)
		nanosleep(&timeout, NULL);
	
	if (dnsquery.resolved != 1 || dnsquery.result == NULL) {
		if (!dnsquery.resolved) {
			pthread_cancel(dnsquery.tid);
			pthread_kill(dnsquery.tid, SIGUSR1);
			pthread_join(dnsquery.tid, NULL);
			pthread_create(&dnsquery.tid, NULL,
				       (void *(*)(void *))dnsquery_thread, NULL);
		}
		snprintf(tab->error, sizeof(tab->error),
			 "Unknown domain name: %s", gmi_host);
		goto request_error;
	}
	struct addrinfo *result = dnsquery.result;
#else
	struct addrinfo hints, *result;
	memset (&hints, 0, sizeof (hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags |= AI_CANONNAME;
	if (getaddrinfo(gmi_host, NULL, &hints, &result)) {
		snprintf(tab->error, sizeof(tab->error),
			 "Unknown domain name: %s", gmi_host);
		goto request_error;
	}
#endif

	struct sockaddr_in addr4;
	struct sockaddr_in6 addr6;
	bzero(&addr4, sizeof(addr4));
	bzero(&addr6, sizeof(addr6));
	sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	struct timeval tv;
	tv.tv_sec = TIMEOUT;
	tv.tv_usec = 0;
	setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv);
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
	struct sockaddr* addr = NULL;
	if (result->ai_family == AF_INET) {
		addr4.sin_addr = ((struct sockaddr_in*) result->ai_addr)->sin_addr;
		addr4.sin_family = AF_INET;
		addr4.sin_port = htons(port);
		addr = (struct sockaddr*)&addr4;
	}
	else if (result->ai_family == AF_INET6) {
		addr6.sin6_addr = ((struct sockaddr_in6*) result->ai_addr)->sin6_addr;
		addr6.sin6_family = AF_INET6;
		addr6.sin6_port = htons(port);
		addr = (struct sockaddr*)&addr6;
	} else {
		snprintf(tab->error, sizeof(tab->error), 
			 "Unexpected error, invalid address family %s", gmi_host);
		goto request_error;
	}
	int family = result->ai_family;
	freeaddrinfo(result);
	
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
	conn.connected = 0;
	conn.addr = addr;
	conn.addr4 = addr4;
	conn.addr6 = addr6;
	conn.family = family;
	conn.socket = sockfd;
	pthread_cond_signal(&conn.cond);
	for (int i=0; i < TIMEOUT * 20 && !conn.connected; i++)
		nanosleep(&timeout, NULL);
	if (conn.connected != 1) {
		if (!conn.connected) {
			pthread_cancel(conn.tid);
			pthread_kill(conn.tid, SIGUSR1);
			pthread_join(conn.tid, NULL);
			pthread_create(&conn.tid, NULL, (void *(*)(void *))conn_thread, NULL);
		}
#else
	int addr_size = (family == AF_INET)?sizeof(addr4):sizeof(addr6);
	if (connect(sockfd, addr, addr_size) != 0) {
#endif
		snprintf(tab->error, sizeof(tab->error), 
			 "Connection to %s timed out", gmi_host);
		goto request_error;
	}

	if (tls_connect_socket(ctx, sockfd, gmi_host)) {
		snprintf(tab->error, sizeof(tab->error), 
			 "Unable to connect to: %s", gmi_host);
		goto request_error;
	}
	if (tls_handshake(ctx)) {
		snprintf(tab->error, sizeof(tab->error),
			 "Failed to handshake: %s", gmi_host);
		goto request_error;
	}
	if (cert_verify(gmi_host, tls_peer_cert_hash(ctx))) {
		snprintf(tab->error, sizeof(tab->error),
			 "Failed to verify server certificate for %s (Diffrent hash)",
			 gmi_host);
		goto request_error;
	}
	char buf[MAX_URL];
	ssize_t len = gmi_parseuri(url_buf, MAX_URL, buf, MAX_URL);
	if (len >= MAX_URL ||
	    (len += strlcpy(&buf[len], "\r\n", MAX_URL - len)) >= MAX_URL) {
		snprintf(tab->error, sizeof(tab->error),
			 "Url too long: %s", buf);
		goto request_error;
	}
	if (tls_write(ctx, buf, len) < len) {
		snprintf(tab->error, sizeof(tab->error),
			 "Failed to send data to: %s", gmi_host);
		goto request_error;
	}
	time_t now = time(0);
	while (recv==TLS_WANT_POLLIN || recv==TLS_WANT_POLLOUT) {
		if (time(0) - now >= TIMEOUT) {
			snprintf(tab->error, sizeof(tab->error),
				 "Connection to %s timed out (sent no data)", gmi_host);
			goto request_error;
		}
		recv = tls_read(ctx, buf, sizeof(buf));
	}
	
	if (recv <= 0) {
		snprintf(tab->error, sizeof(tab->error),
			 "[%d] Invalid data from: %s", recv, gmi_host);
		goto request_error;
	}
	if (!strstr(buf, "\r\n")) {
		snprintf(tab->error, sizeof(tab->error),
			 "Invalid data from: %s (no CRLF)", gmi_host);
		goto request_error;
	}
	char* ptr = strchr(buf, ' ');
	if (!ptr) {
		snprintf(tab->error, sizeof(tab->error),
			 "Invalid data from: %s", gmi_host);
		goto request_error;
	}
	int previous_code = tab->page.code;
	tab->page.code = atoi(buf);

	switch (tab->page.code) {
	case 10:
	case 11:
		ptr++;
		strlcpy(client.input.label, ptr, sizeof(client.input.label));
		ptr = strchr(client.input.label, '\n');
		if (ptr) *ptr = '\0';
		ptr = strchr(client.input.label, '\r');
		if (ptr) *ptr = '\0';
		goto exit;
	case 20:
	{
		tab->page.img.tried = 0;
		char* meta = strchr(++ptr, '\r');
		if (!meta) {
			snprintf(tab->error, sizeof(tab->error),
				 "Invalid data from: %s", gmi_host);
			goto request_error;
		}
		*meta = '\0';
		strlcpy(meta_buf, ptr, MAX_META);
		*meta = '\r';
		if ((strcasestr(meta_buf, "charset=") && 
		    !strcasestr(meta_buf, "charset=utf-8"))
		   || ((strncmp(meta_buf, "text/", 5))
#ifdef TERMINAL_IMG_VIEWER
		   && (strncmp(meta_buf, "image/", 6))
#endif
		   )) {
			if (tab->history) {
				strlcpy(tab->url, tab->history->url, sizeof(tab->url));
			} else {
				strlcpy(tab->url, "about:home", sizeof(tab->url));
			}
			char info[2048];
			snprintf(info, sizeof(info), 
				 "# Non-renderable meta-data %s : ",
				 meta_buf);
			download = display_ask(info);
			if (!download) {
				recv = -1;
				goto exit_download;
			}
		}
		char* ptr_meta = strchr(tab->page.meta, ';');
		if (ptr_meta) *ptr_meta = '\0';
	}
		break;
	case 30:
		snprintf(tab->error, sizeof(tab->error),
			 "Redirect temporary");
		break;
	case 31:
		snprintf(tab->error, sizeof(tab->error),
			 "Redirect permanent");
		break;
	case 40:
		snprintf(tab->error, sizeof(tab->error),
			 "Temporary failure");
		goto request_error_msg;
	case 41:
		snprintf(tab->error, sizeof(tab->error),
			 "Server unavailable");
		goto request_error_msg;
	case 42:
		snprintf(tab->error, sizeof(tab->error),
			 "CGI error");
		goto request_error_msg;
	case 43:
		snprintf(tab->error, sizeof(tab->error),
			 "Proxy error");
		goto request_error_msg;
	case 44:
		snprintf(tab->error, sizeof(tab->error),
			 "Slow down: server above rate limit");
		goto request_error_msg;
	case 50:
		snprintf(tab->error, sizeof(tab->error),
			 "Permanent failure");
		goto request_error_msg;
	case 51:
		snprintf(tab->error, sizeof(tab->error),
			 "Not found %s", url);
		goto request_error_msg;
	case 52:
		snprintf(tab->error, sizeof(tab->error),
			 "Resource gone");
		goto request_error_msg;
	case 53:
		snprintf(tab->error, sizeof(tab->error),
			 "Proxy request refused");
		goto request_error_msg;
	case 59:
		snprintf(tab->error, sizeof(tab->error),
			 "Bad request");
		goto request_error_msg;
	case 60:
		snprintf(tab->error, sizeof(tab->error),
			 "Client certificate required");
		goto request_error_msg;
	case 61:
		snprintf(tab->error, sizeof(tab->error),
			 "Client not authorised");
		goto request_error_msg;
	case 62:
		snprintf(tab->error, sizeof(tab->error),
			 "Client certificate not valid");
		goto request_error_msg;
	default:
		snprintf(tab->error, sizeof(tab->error),
			 "Unknown status code: %d", tab->page.code);
		tab->page.code = previous_code;
		goto request_error;
	}
	data_buf = malloc(recv+1);
	if (!data_buf) return fatalI();
	memcpy(data_buf, buf, recv);
	now = time(0);
	while (1) {
		if (time(0) - now >= TIMEOUT) {
			snprintf(tab->error, sizeof(tab->error),
				 "Server %s stopped responding", gmi_host);
			goto request_error;
		}
		int bytes = tls_read(ctx, buf, sizeof(buf));
		if (bytes == 0) break;
		if (bytes == TLS_WANT_POLLIN || bytes == TLS_WANT_POLLOUT) continue;
		if (bytes < 1) {
			snprintf(tab->error, sizeof(tab->error),
				 "Invalid data from %s: %s",
				 gmi_host, tls_error(ctx));
			goto request_error;
		}
		data_buf = realloc(data_buf, recv+bytes+1);
		memcpy(&data_buf[recv], buf, bytes);
		recv += bytes;
		now = time(0);
	}
exit:
	
	if (0) {
request_error_msg:;
		char* cr = strchr(ptr+1, '\r');
		if (cr) {
			*cr = '\0';
			char buf[256];
			snprintf(buf, sizeof(buf),
				 "%s (%d : %s)", ptr+1, tab->page.code, tab->error);
			strlcpy(tab->error, buf, sizeof(tab->error));
			*cr = '\r';
		}
request_error:
		free(data_buf);
		/*
		if (tab->history) {
			strlcpy(tab->url, tab->history->url, sizeof(tab->url));
		} else {
			strlcpy(tab->url, "about:home", sizeof(tab->url));
		}*/
		client.input.error = 1;
		recv = -1;
		tab->page.code = 20;
	}
exit_download:
	if (sockfd != -1)
		close(sockfd);
	if (ctx) {
		tls_close(ctx);
		tls_free(ctx);
		ctx = NULL;
	}
	if (recv > 0 && (tab->page.code == 11 || tab->page.code == 10)) {
		free(data_buf);
		strlcpy(tab->url, url_buf, sizeof(tab->url));
		return tab->page.data_len;
	}
	if (recv > 0 && (tab->page.code == 31 || tab->page.code == 30)) {
		char* ptr = strchr(data_buf, ' ');
		if (!ptr) {
			free(data_buf);
			return -1;
		}
		char* ln = strchr(ptr+1, ' ');
		if (ln) *ln = '\0';
		ln = strchr(ptr, '\n');
		if (ln) *ln = '\0';
		ln = strchr(ptr, '\r');
		if (ln) *ln = '\0';
		strlcpy(tab->url, url_buf, sizeof(tab->url));
		int r = gmi_nextlink(tab, tab->url, ptr+1);
		if (r < 1 || tab->page.code != 20) {
			free(data_buf);
			return tab->page.data_len;
		}
		tab->page.data_len = r;
		free(data_buf);
		data_buf = NULL;
		gmi_load(&tab->page);
		tab->scroll = -1;
		return r;
	}
	if (!download && recv > 0 && tab->page.code == 20) {
		struct gmi_link* link_ptr = tab->history?tab->history->prev:NULL;
		for (int i = 0; i < MAX_CACHE; i++) {
			if (!link_ptr) break;
			link_ptr = link_ptr->prev;
		}
		while (link_ptr) {
			if (link_ptr->cached) {
				gmi_freepage(&link_ptr->page);
				link_ptr->cached = 0;
			}
			link_ptr = link_ptr->prev;
		}
		tab->history->page = tab->page;
		tab->history->cached = 1;
		bzero(&tab->page, sizeof(struct gmi_page));
		tab->page.code = 20;
		strlcpy(tab->page.meta, meta_buf, sizeof(tab->page.meta));
		strlcpy(tab->url, url_buf, sizeof(tab->url));
		tab->page.data_len = recv;
		tab->page.data = data_buf;
		gmi_load(&tab->page);
		gmi_addtohistory(tab);
	}
	if (download && recv > 0 && tab->page.code == 20) {
		int len = strnlen(url_buf, sizeof(url_buf));
		if (url_buf[len-1] == '/')
			url_buf[len-1] = '\0';
		char* ptr = strrchr(url_buf, '/');
		FILE* f;
		char path[1024];
		int plen = getdownloadfolder(path, sizeof(path));	
		if (plen < 0) plen = 0;
		else {
			path[plen] = '/';
			plen++;
		}
		if (ptr)
			strlcpy(&path[plen], ptr+1, sizeof(path)-plen);
		else
			strlcpy(&path[plen], "output.dat", sizeof(path)-plen);
		f = fopen(path, "wb");
		if (!f) {
			snprintf(tab->error, sizeof(tab->error),
				 "Failed to write the downloaded file");
			client.input.error = 1;
		} else {
			char* ptr = strchr(data_buf, '\n')+1;
			if (ptr) {
				fwrite(ptr, 1, recv - (ptr-data_buf), f);
				fclose(f);
#ifndef DISABLE_XDG
				char info[2048];
				snprintf(info, sizeof(info), 
					 "# %s downloaded",
					 path);
				if (client.xdg && display_ask(info)) {
					char buf[1048];
					snprintf(buf, sizeof(buf), "xdg-open %s", path);
					system(buf);
				}
#endif
				client.input.info = 1;
				snprintf(tab->info, sizeof(tab->info),
					"File downloaded to %s", path);
					 
			} else {
				client.input.error = 1;
				snprintf(tab->error, sizeof(tab->error),
					 "Server sent invalid data");
			}
		}
		free(data_buf);
		recv = tab->page.data_len;
	}
	return recv;
}

int gmi_loadfile(struct gmi_tab* tab, char* path) {
	FILE* f = fopen(path, "rb");
	if (!f) {
		snprintf(tab->error, sizeof(tab->error),
			 "Failed to open %s", path);
		client.input.error = 1;
		return -1;
	}
	fseek(f, 0, SEEK_END);
	size_t len = ftell(f);
	fseek(f, 0, SEEK_SET);
	char* data = malloc(len+1);
	if (!data) return fatalI();
	data[0] = '\n';
	if (len != fread(&data[1], 1, len, f)) {
		fclose(f);
		free(data);
		return -1;
	}
	fclose(f);
	tab->page.code = 20;
	tab->page.data_len = len;
	if (tab->page.data) free(tab->page.data);
	tab->page.data = data;
	snprintf(tab->url, sizeof(tab->url), "file://%s/", path);
	return len;
}

int gmi_init() {
	if (tls_init()) {
		printf("Failed to initialize TLS\n");
		return -1;
	}
	
	config = tls_config_new();
	if (!config) {
		printf("Failed to initialize TLS config\n");
		return -1;
	}

	config_empty = tls_config_new();
	if (!config_empty) {
		printf("Failed to initialize TLS config\n");
		return -1;
	}

	tls_config_insecure_noverifycert(config);
	tls_config_insecure_noverifycert(config_empty);
	ctx = NULL;
	bzero(&client, sizeof(client));

#ifndef DISABLE_XDG
	if (!system("which xdg-open > /dev/null 2>&1"))
		client.xdg = 1;
#endif

	if (gmi_loadbookmarks()) {
		gmi_newbookmarks();
	}

	if (cert_load()) {
		printf("Failed to load known hosts\n");
		return -1;
	}

#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
	bzero(&conn, sizeof(conn));
	if (pthread_cond_init(&conn.cond, NULL)) {                                    
		printf("Failed to initialize pthread condition\n");
		return -1;
	}
	if (pthread_create(&conn.tid, NULL, (void *(*)(void *))conn_thread, NULL)) {
		printf("Failed to initialize connection thread\n");
		return -1;
	}
	bzero(&dnsquery, sizeof(dnsquery));
	if (pthread_cond_init(&dnsquery.cond, NULL)) {                                    
		printf("Failed to initialize thread condition\n");
		return -1;
	}
	if (pthread_create(&dnsquery.tid, NULL, (void *(*)(void *))dnsquery_thread, NULL)) {
		printf("Failed to initialize dns query thread\n");
		return -1;
	}
	signal(SIGUSR1, signal_cb);
#endif
		
	return 0;
}

void gmi_free() {
	for (int i=0; i < client.tabs_count; i++)
		gmi_freetab(&client.tabs[i]);
	gmi_savebookmarks();
	for (int i = 0; client.bookmarks[i]; i++)
		free(client.bookmarks[i]);
	free(client.bookmarks);
	free(client.tabs);
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
	pthread_kill(conn.tid, SIGUSR1);
	pthread_join(conn.tid, NULL);
	pthread_kill(dnsquery.tid, SIGUSR1);
	pthread_join(dnsquery.tid, NULL);
#endif
	cert_free();
}

