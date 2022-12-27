/* See LICENSE file for copyright and license details. */
#ifdef __linux__
#define _GNU_SOURCE
#endif
#ifdef sun
#include <port.h>
#endif
#include <pthread.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <unistd.h>
#include <sys/socket.h>
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
#include "cert.h"
#include "wcwidth.h"
#include "display.h"
#include "input.h"
#include "sandbox.h"
#include "str.h"
#include "url.h"

#define MAX_CACHE 10
#define TIMEOUT 8
struct timespec timeout = {0, 10000000};

struct tls_config* config;
struct tls_config* config_empty;
struct gmi_client client;

void tb_colorline(int x, int y, uintattr_t color);
void* gmi_request_thread(struct gmi_tab* tab);

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

int gmi_goto(struct gmi_tab* tab, int id) {
	id--;
	struct gmi_page* page = &tab->page;
	if (id < 0 || id >= page->links_count) {
		snprintf(tab->error, sizeof(tab->error),
			 "Invalid link number, %d/%d", id, page->links_count);
		tab->show_error = 1;
		client.input.mode = 0;
		return -1;
	}
	gmi_cleanforward(tab);
	int ret = gmi_nextlink(tab, tab->url, page->links[id]);
	return ret;
}

int gmi_goto_new(struct gmi_tab* tab, int id) {
	id--;
	struct gmi_page* page = &tab->page;
	if (id < 0 || id >= page->links_count) {
		snprintf(tab->error, sizeof(tab->error),
			 "Invalid link number, %d/%d", id, page->links_count);
		tab->show_error = 1;
		client.input.mode = 0;
		return -1;
	}
	struct gmi_tab* old_tab = client.tab;
	client.tab = gmi_newtab_url(NULL);
	int ret = gmi_nextlink(client.tab, old_tab->url,
			       old_tab->page.links[id]);
	return ret;
}

int gmi_nextlink(struct gmi_tab* tab, char* url, char* link) {
	int url_len = strnlen(url, MAX_URL);
	if (!strcmp(link, "about:home")) {
		gmi_gohome(tab, 1);
		return 0;
	} else if (link[0] == '/' && link[1] == '/') {
		char buf[1024];
		strlcpy(buf, &link[2], MAX_URL - 2);
		int len = strnlen(buf, sizeof(buf));
		if (len < MAX_URL - 1 && buf[len - 1] != '/') {
			buf[len] = '/';
			buf[len + 1] = '\0';
		}
		int ret = gmi_request(tab, buf, 1);
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
		if (strlcpy(urlbuf + l, link, sizeof(urlbuf) - l) >=
		    sizeof(urlbuf) - l)
			goto nextlink_overflow;
		int ret = gmi_request(tab, urlbuf, 1);
		return ret;
	} else if (strstr(link, "https://") == link ||
		   strstr(link, "http://") == link ||
		   strstr(link, "gopher://") == link) {
#ifndef DISABLE_XDG
		if (client.xdg && !xdg_open(link)) return -1;
#endif
		tab->show_error = 1;
		snprintf(tab->error, sizeof(tab->error), 
			 "Unable to open the link");
		return -1;
	} else if (strstr(link, "gemini://") == link) {
		int ret = gmi_request(tab, link, 1);
		return ret;
	} else {
		char* ptr = strrchr(&url[GMI], '/');
		if (ptr) *ptr = '\0';
		char urlbuf[MAX_URL];
		size_t l = strlcpy(urlbuf, url, sizeof(urlbuf));
		if (l >= sizeof(urlbuf))
			goto nextlink_overflow;
		if (urlbuf[l-1] != '/') {
			size_t l2 = strlcpy(urlbuf + l, "/",
					    sizeof(urlbuf) - l);
			if (l2 >= sizeof(urlbuf) - l)
				goto nextlink_overflow;
			l += l2;
		}
		size_t l2 = strlcpy(urlbuf + l, link, sizeof(urlbuf) - l);
		if (l2 >= sizeof(urlbuf) - l)
			goto nextlink_overflow;
		int ret = gmi_request(tab, urlbuf, 1);
		return ret;
	}
nextlink_overflow:
	tab->show_error = 1;
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
	if (strncmp(page->meta, "text/gemini", sizeof("text/gemini") - 1)) {
		page->links = NULL;
		page->links_count = 0;
		page->lines = 0;
		return;
	}
	int x = 0;
	for (int c = 0; c < page->data_len; c++) {
		if (x == 0 && page->data[c] == '=' &&
		    page->data[c + 1] == '>') {
			c += 2;
			for (; c < page->data_len &&
			       (page->data[c] == ' ' || page->data[c] == '\t');
			     c++) ;
			char* url = (char*)&page->data[c];
			c += parse_link(&page->data[c], page->data_len - c);
			if (page->data[c - 1] == 127)
				c--;
			char save = page->data[c];
			page->data[c] = '\0';
			if (page->links)
				page->links = realloc(page->links,
						      sizeof(char*) *
						      (page->links_count+1));
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
	pthread_mutex_lock(&tab->render_mutex);
#include "img.h"
#ifdef TERMINAL_IMG_VIEWER
	if (strncmp(tab->page.meta, "image/", 6) == 0) {
		if (!tab->page.img.tried) {
			char* ptr = strchr(tab->page.data, '\n');
			if (!ptr) {
				tb_printf(2, -tab->scroll,
					  TB_DEFAULT, TB_DEFAULT,
					  "Invalid data: new line not found");
				tab->page.img.tried = 1;
				pthread_mutex_unlock(&tab->render_mutex);
				return 1;
			}
			ptr++;
			if (tab->page.img.data) {
				stbi_image_free(tab->page.img.data);
				tab->page.img.data = NULL;
			}
			tab->page.img.data = 
			stbi_load_from_memory((unsigned char*)ptr, 
					      tab->page.data_len -
					      (int)(ptr-tab->page.data),
					      &tab->page.img.w,
					      &tab->page.img.h,
					      &tab->page.img.channels, 3);
			if (!tab->page.img.data) {
				tb_printf(2, -tab->scroll,
					  TB_DEFAULT, TB_DEFAULT,
					  "Failed to decode image: %s;",
					  tab->page.meta);
				tab->page.img.tried = 1;
				pthread_mutex_unlock(&tab->render_mutex);
				return 1;
			}

			tab->page.img.tried = 1;
		}
		if (tab->page.img.data) {
			img_display(tab->page.img.data,
					tab->page.img.w, tab->page.img.h,
					client.tabs_count>1);
			pthread_mutex_unlock(&tab->render_mutex);
			return 1;
		}
	}
#endif
	int text = 0;
	if (strncmp(tab->page.meta, "text/gemini", 11)) {
		if (strncmp(tab->page.meta, "text/", 5)) {
			tb_printf(2, -tab->scroll, TB_DEFAULT, TB_DEFAULT,
				  "Unable to render format : %s",
				  tab->page.meta);
			pthread_mutex_unlock(&tab->render_mutex);
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
	char* search = client.input.field[0] == '/'?
			&client.input.field[1]:
			tab->search.entry;
	int search_len = search?
			 strnlen(search, sizeof(client.input.field) - 1):0;
	if (!search_len) search = NULL;
	int highlight = 0;
	int hlcolor = YELLOW;
	if (tab->search.cursor >= tab->search.count)
		tab->search.cursor = 0;
	else if (tab->search.cursor < 0)
		tab->search.cursor = tab->search.count - 1;
	int previous_count = tab->search.count;
	tab->search.count = 0;
	char* ptr = tab->page.no_header?NULL:strstr(tab->page.data, "\r\n");
	if (ptr && ptr > strstr(tab->page.data, "\n")) ptr = NULL;
	line++;
	int w = tb_width();
	for (int c = ptr?ptr-tab->page.data+2:0; c < tab->page.data_len; c++) {
		if (x == 0 && tab->page.data[c] == '\n') {
			if (c+1 != tab->page.data_len)
				line += 1;
			continue;
		}
		if (tab->page.data[c] == '\t') {
			x+=4;
			continue;
		}
		if (tab->page.data[c] == '\r') continue;
		if (!text && start &&
		    tab->page.data[c] == '`' &&
		    tab->page.data[c + 1] == '`' &&
		    tab->page.data[c + 2] == '`') {
			ignore = !ignore;
			c += 3;
			continue;
		}
		if (ignore)
			color = ORANGE;
		
		if (!ignore && !text) {
			for (int i=0;
			     start && tab->page.data[c+i] == '#' && i<3;
			     i++) {
				if (tab->page.data[c+i+1] != '#') {
					color = RED + i;
					break;
				}
			}
			if (start && tab->page.data[c] == '*' &&
				     tab->page.data[c + 1] == ' ') {
				color = ITALIC|CYAN;
			}
			if (start && tab->page.data[c] == '>' &&
				     tab->page.data[c + 1] == ' ') {
				color = ITALIC|MAGENTA;
			}
			if (start && tab->page.data[c] == '=' &&
				     tab->page.data[c + 1] == '>') {
				char buf[32];
				int len = snprintf(buf, sizeof(buf),
						   "[%d]", links+1);
				if (line-1>=(tab->scroll>=0?tab->scroll:0) &&
				    line-tab->scroll <= tb_height()-2) {
					tb_print(x+2, line-1-tab->scroll,
						 links+1 ==
						 tab->selected?RED:BLUE,
						 TB_DEFAULT, buf);
				}
				x += len;
				c += 2;

				while (
				(tab->page.data[c]==' ' ||
				 tab->page.data[c]=='\t') && 
				tab->page.data[c]!='\n' &&
				tab->page.data[c]!='\0') c++;

				int initial = c;
				while (tab->page.data[c]!=' ' &&
				tab->page.data[c]!='\t' && 
				tab->page.data[c]!='\n' &&
				tab->page.data[c]!='\0') c++;

				while (
				(tab->page.data[c]==' ' ||
				 tab->page.data[c]=='\t') && 
				tab->page.data[c]!='\n' &&
				tab->page.data[c]!='\0') c++;

				if (tab->page.data[c]=='\n' ||
				    tab->page.data[c]=='\0')
					c = initial;
				x+=3;
				if ((links+1)/10) x--;
				if ((links+1)/100) x--;
				if ((links+1)/1000) x--;
				links++;
			}
		}

		if (search &&
		    !strncasecmp(&tab->page.data[c], search, search_len)) {
			if (tab->search.count == (tab->search.cursor?
						 (tab->search.cursor - 1):
						 (previous_count - 1)))
				tab->search.pos[0] = line;
			if (tab->search.count == tab->search.cursor)
				hlcolor++;
			if (tab->search.cursor == previous_count - 1 &&
			    tab->search.count == 0)
				tab->search.pos[1] = line;
			if (tab->search.count == tab->search.cursor + 1) {
				hlcolor--;
				tab->search.pos[1] = line;
			}
			highlight += search_len;
			tab->search.count++;
		}
		if (tab->page.data[c] == '\n' || tab->page.data[c] == ' ' ||
		    x+4 >= w) {
			int end = 0;
			if (x+4 >= w)
				end = 1;
			int newline = (tab->page.data[c] == '\n' || x+4 >= w);
			for (int i = 1; ; i++) {
				if (x + i == w - 1) break;
				if (i > w - 4) {
					newline = 0;
					break;
				}
				if (c + i >= tab->page.data_len ||
				    tab->page.data[c+i] == ' ' ||
				    tab->page.data[c+i] == '\n' ||
				    tab->page.data[c+i] == '\0')
					break;
				if (w - 4 <= x + i) newline = 1;
			}
			if (newline) {
				if (c != tab->page.data_len)
					line += 1 + (x + 1)/w;
				if (tab->page.data[c] == '\n') {
					color = TB_DEFAULT;
					start = 1;
				} else c--;
				if (c + 1 >= tab->page.data_len) continue;
				if (tab->page.data[c + 1] == ' ') c++;
				x = 0;
				continue;
			} else if (end) {
				c++;
				x++;
			}
		}
		uint32_t ch = 0;
		int size = tb_utf8_char_to_unicode(&ch,
						   &tab->page.data[c]) - 1;
		if (size > 0)
			c += tb_utf8_char_to_unicode(&ch,
						     &tab->page.data[c]) - 1;
		else if (ch < 32) ch = '?';
		
		int wc = mk_wcwidth(ch);
		if (wc < 0) wc = 0;
		if (line - 1 >= tab->scroll &&
		    (line - tab->scroll <= tb_height() - 2) && ch != '\t') {
			if (wc == 1)
				tb_set_cell(x + 2, line - 1 - tab->scroll,
					    ch, color,
					    highlight?hlcolor:TB_DEFAULT);
			else
				tb_set_cell_ex(x + 2, line - 1 - tab->scroll,
					       &ch, wc, color,
					       highlight?hlcolor:TB_DEFAULT);
		}
		if (highlight > 0)
			highlight--;

		x += wc;
		start = 0;
	}
	line++;
	h += (client.tabs_count > 1);
	if (h > line) {
		pthread_mutex_unlock(&tab->render_mutex);
		return line;
	}
	int size = (h == line?(h - 1):(h / (line - h)));
	if (size == h) size--;
	if (size < 1) size = 1;
	int H = (line - h + 1 + (client.tabs_count > 1));
	int pos = (tab->scroll + (client.tabs_count < 1)) * h / (H?H:1)
		  + (client.tabs_count>1);
	if (pos >= h) pos = h - size;
	if (pos < 0) pos = 0;
	if (tab->scroll+h == line) pos = h - size;
	for (int y = (client.tabs_count > 1);
	     y < h + (client.tabs_count > 1); y++) {
		if (y >= pos && y < pos + size)
			tb_set_cell(w - 1, y, ' ', TB_DEFAULT, CYAN);
		else
			tb_set_cell(w - 1, y, ' ', TB_DEFAULT, BLACK);
	}
	pthread_mutex_unlock(&tab->render_mutex);
	return line;
}

void gmi_addtohistory(struct gmi_tab* tab) {
	if (!(!tab->history || (tab->history && !tab->history->next)))
		return;
	gmi_cleanforward(tab);
	struct gmi_link* link = malloc(sizeof(struct gmi_link));
	if (!link) {
		fatal();
		return;
	}
	link->next = NULL;
	link->prev = tab->history;
	if (link->prev)
		link->prev->scroll = tab->scroll;
	link->page = tab->page;
	link->cached = 1;
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
	if (!page) return;
#ifdef TERMINAL_IMG_VIEWER
	if (page->img.data)
		stbi_image_free(page->img.data);
#endif
	free(page->data);
	for (int i=0; i < page->links_count; i++)
		free(page->links[i]);
	free(page->links);
	bzero(page, sizeof(struct gmi_page));
}

void gmi_freetab(struct gmi_tab* tab) {
	if (!tab) return;
	tab->request.state = STATE_CANCEL;
	int signal = 0xFFFFFFFF;
	send(tab->thread.pair[1], &signal, sizeof(signal), 0);
	close(tab->thread.pair[0]);
	close(tab->thread.pair[1]);
	if (tab->history) {
		struct gmi_link* link = tab->history->next;
		while (link) {
			struct gmi_link* ptr = link->next;
			if (link->cached) {
				gmi_freepage(&link->page);
				link->cached = 0;
			}
			bzero(link->url, sizeof(link->url));
			free(link);
			link = ptr;
		}
		link = tab->history;
		while (link) {
			struct gmi_link* ptr = link->prev;
			if (link->cached) {
				gmi_freepage(&link->page);
				link->cached = 0;
			}
			bzero(link->url, sizeof(link->url));
			free(link);
			link = ptr;
		}
	}
	if (tab->prev) tab->prev->next = tab->next;
	if (tab->next) tab->next->prev = tab->prev;
	struct gmi_tab* prev = tab->prev;
	if (!prev) prev = tab->next;
	pthread_mutex_destroy(&tab->render_mutex);
	if ((signed)tab->thread.started)
		pthread_join(tab->thread.thread, NULL);
#ifdef TERMINAL_IMG_VIEWER
	if (tab->page.img.data)
		free(tab->page.img.data);
#endif
	free(tab);
	client.tab = prev;
	client.tabs_count--;
}

char home_page[] = 
"20 text/gemini\r\n# Vgmi - " VERSION "\n\n" \
"A Gemini client written in C with vim-like keybindings\n\n" \
"## Bookmarks\n\n" \
"%s\n" \
"## Keybindings\n\n" \
"* k - Scroll up\n" \
"* j - Scroll down\n" \
"* gT - Switch to the previous tab\n" \
"* gt - Switch to the next tab\n" \
"* H - Go back in the history\n" \
"* L - Go forward in the history\n" \
"* gg - Go at the top of the page\n" \
"* G - Go at the bottom of the page\n" \
"* / - Open search mode\n" \
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
"* :add [name] - Add the current url to the bookmarks, the name is optional\n"\
"* :[number] - Scroll to the line number\n" \
"* :gencert - Generate a certificate for the current capsule\n" \
"* :forget <host> - Forget the certificate for an host\n" \
"* :download [name] - Download the current page, the name is optional\n"
"* :exec - Open the last downloaded file";

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
	int fd = openat(config_fd, "bookmarks.txt", O_RDONLY);
	if (fd < 0)
		return -1;
	FILE* f = fdopen(fd, "rb");
	if (!f)
		return -1;
#if defined(__FreeBSD__) && !defined(NO_SANDBOX)
	if (makefd_readonly(fd)) {
		fclose(f);
		return -1;
	}
#endif
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
		if (*ptr != '\n') continue;
		*ptr = '\0';
		client.bookmarks[n] = malloc(ptr-str+1);
		strlcpy(client.bookmarks[n], str, ptr-str+1);
		n++;
		str = ptr+1;
	}
	free(data);
	return 0;
}

void sanitize(char* str, size_t len) {
	int n = 0;
	for (size_t i = 0; i < len; i += n) {
		n = tb_utf8_char_length(str[i]);
		if (n > 1 || str[i] >= 32) continue;
		str[i] = '\0';
		break;
	}
}

void gmi_gettitle(struct gmi_page* page, const char* url) {
	if (page->title_cached) return;
	page->title[0] = '\0';
	page->title_cached = 1;
	if (strncmp(page->meta, "text/gemini", sizeof("text/gemini") - 1)) {
		goto use_url;
	}
	int start = -1;
	int end = -1;
	int line_start = 1;
	for (int i = 0; i < page->data_len; i++) {
		if (line_start && start == -1 && page->data[i] == '#') {
			for (int j = i+1; j < page->data_len; j++) {
				if (j && page->data[j-1] == '#' &&
				    page->data[j] == '#')
					break;
				if (page->data[j] != ' ') {
					start = j;
					break;
				}
			}
		}
		line_start = 0;
		if (page->data[i] == '\n')
			line_start = 1;
		if (start != -1 && page->data[i] == '\n') {
			end = i;
			break;
		}
	}
	if (start == -1 || end == -1)
		goto use_url;
	size_t len = end - start + 1;
	len = strlcpy(page->title, &page->data[start],
		      len < sizeof(page->title)?
		      len:sizeof(page->title));
	sanitize(page->title, len);
	return;
use_url:
	if (!url) {
		size_t len = strlcpy(page->title, page->meta,
				     sizeof(page->title));
		sanitize(page->title, len);
		return;
	}
	char* str = strrchr(url, '/');
	if (!str) {
		len = strlcpy(page->title, url, sizeof(page->title));
		goto sanitize;
	}
	if (str[1] != '\0')
		str++;
	len = strlcpy(page->title, str, sizeof(page->title));
sanitize:
	for (size_t i = 0; i < len; i++)
		if (page->title[i] == '?')
			page->title[i] = 0;
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
		tab->show_error = 1;
		return;
	}
	int title_len = 0;
	if (!title) {
		gmi_gettitle(&tab->page, tab->url);
		title = tab->page.title;
		title_len = strnlen(tab->page.title, sizeof(tab->page.title));
		if (!title_len) {
			title_len = sizeof("no title");
			title = "no title";
		}
	} else title_len = strnlen(title, 128);
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
	gmi_savebookmarks();
}

#ifndef gmi_savebookmarks // sandboxed on Illumos
int gmi_savebookmarks() {
	int fd = openat(config_fd, "bookmarks.txt",
			O_CREAT|O_WRONLY|O_CLOEXEC|O_TRUNC, 0600);
	if (fd < 0) {
		printf("Failed to write bookmarks, %s\n", strerror(errno));
		return -1;
	}
	FILE* f = fdopen(fd, "wb");
	if (!f)
		return -1;
#if defined(__FreeBSD__) && !defined(NO_SANDBOX)
	if (makefd_writeonly(fd)) {
		fclose(f);
		return -1;
	}
#endif
	for (int i = 0; client.bookmarks[i]; i++)
		fprintf(f, "%s\n", client.bookmarks[i]);
	fclose(f);
	return 0;
}
#endif

char* gmi_getbookmarks(int* len) {
	char* data = NULL;
	int n = 0;
	for (int i = 0; client.bookmarks[i]; i++) {
		char line[2048];
		long length = snprintf(line, sizeof(line), "=>%s\n ",
				       client.bookmarks[i]);
		data = realloc(data, n+length+1);
		if (!data) return fatalP();
		strlcpy(&data[n], line, length);
		n += length-1;
	}
	*len = n;
	return data;
}

void gmi_gohome(struct gmi_tab* tab, int add) {
	strlcpy(tab->url, "about:home", sizeof(tab->url)); 
	int bm = 0;
	char* data = gmi_getbookmarks(&bm);
	pthread_mutex_lock(&tab->render_mutex);
	tab->request.data = malloc(sizeof(home_page) + bm);
	bzero(tab->request.data, sizeof(home_page) + bm);
	if (!tab->request.data) {
		fatal();
		return;
	}

	tab->request.recv = snprintf(tab->request.data,
				     sizeof(home_page) + bm,
				     home_page,
				     data?data:"");
	free(data);

	strlcpy(tab->request.meta, "text/gemini",
		sizeof(tab->request.meta));

	if (!add)
		gmi_freepage(&tab->page);
	bzero(&tab->page, sizeof(struct gmi_page));
	tab->page.data = tab->request.data;
	tab->page.data_len = tab->request.recv;
	tab->page.code = 20;
	strlcpy(tab->page.meta, tab->request.meta, sizeof(tab->page.meta));
	gmi_load(&tab->page);
	if (add)
		gmi_addtohistory(tab);
	else if (tab->history) {
		tab->history->page = tab->page;
		tab->history->cached = 1;
	}
	tab->scroll = -1;
	tab->request.data = NULL;
	pthread_mutex_unlock(&tab->render_mutex);
}

struct gmi_tab* gmi_newtab() {
	return gmi_newtab_url("about:home");
}

struct gmi_tab* gmi_newtab_url(const char* url) {
	client.tabs_count++;
	if (client.tab) {
		struct gmi_tab* next = client.tab->next;
		client.tab->next = malloc(sizeof(struct gmi_tab));
		if (!client.tab->next) return fatalP();
		bzero(client.tab->next, sizeof(struct gmi_tab));
		client.tab->next->next = next;
		client.tab->next->prev = client.tab;
		client.tab = client.tab->next;
		if (next)
			next->prev = client.tab;
	} else {
		client.tab = malloc(sizeof(struct gmi_tab));
		if (!client.tab) return fatalP();
		bzero(client.tab, sizeof(struct gmi_tab));
	}
	pthread_mutex_init(&client.tab->render_mutex, NULL);
	
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, client.tab->thread.pair))
		return NULL;
	pthread_create(&client.tab->thread.thread, NULL,
		       (void *(*)(void *))gmi_request_thread,
		       (void*)client.tab);
	client.tab->thread.started = 1;
	if (url)
		gmi_request(client.tab, url, 1);

	client.tab->scroll = -1;
	return client.tab;
}

int gmi_request_init(struct gmi_tab* tab, const char* url, int add) {
	if (!tab) return -1;
	if (!strcmp(url, "about:home")) {
		gmi_gohome(tab, add);
		return tab->page.data_len;
	}
	tab->show_error = 0;
	if (tab->history) tab->history->scroll = tab->scroll;
	tab->selected = 0;
	tab->request.host[0] = '\0';
	int proto = parse_url(url,
			      tab->request.host, sizeof(tab->request.host),
			      tab->request.url, sizeof(tab->request.url),
			      &tab->request.port);

	if (proto == -2) {
		tab->show_error = 1;
		snprintf(tab->error, sizeof(tab->error),
			 "Link too long, above 1024 characters");
		return -1;
	}
	if (proto == PROTO_FILE) {
		return gmi_loadfile(tab, &tab->request.url[P_FILE]);
	}
	if (proto != PROTO_GEMINI) {
#ifndef DISABLE_XDG
		if (client.xdg && !xdg_open((char*)url)) return 1;
#endif
		tab->show_error = 1;
		snprintf(tab->error, sizeof(tab->error), 
			 "Unable to open the link");
		return -1;
	}
	if (tab->request.tls)
		tls_reset(tab->request.tls);
	else
		tab->request.tls = tls_client();
	if (!tab->request.tls) {
		snprintf(tab->error, sizeof(tab->error),
			 "Failed to initialize TLS");
		tab->show_error = 1;
		return -1;
	}

	int cert = cert_getcert(tab->request.host, 0);
	if (cert >= 0 && 
	    tls_config_set_keypair_mem(config,
		    		       (unsigned char*)client.certs[cert].crt,
				       client.certs[cert].crt_len,
				       (unsigned char*)client.certs[cert].key,
				       client.certs[cert].key_len))
	{
		tab->show_error = 1;
		snprintf(tab->error, sizeof(tab->error), "tls error: %s", 
			 tls_config_error(config));
		return -1;
	}
	
	if (tls_configure(tab->request.tls, cert>=0?config:config_empty)) {
		snprintf(tab->error, sizeof(tab->error),
			 "Failed to configure TLS");
		tab->show_error = 1;
		return -1;
	}
	tab->request.state = STATE_REQUESTED;
	return 0;
}

#ifdef __linux
void dns_async(union sigval sv) {
	struct gmi_tab* tab = sv.sival_ptr;
	tab->request.resolved = 1;
}
#endif

int gmi_request_dns(struct gmi_tab* tab) {
	char host[1024];
	if (idn_to_ascii(tab->request.host, sizeof(tab->request.host),
			 host, sizeof(host))) {
		snprintf(tab->error, sizeof(tab->error),
			 "Invalid domain name: %s", tab->request.host);
		tab->show_error = 1;
		return -1;
	}
	strlcpy(tab->request.host, host, sizeof(tab->request.host));
#if defined(__linux__) && !defined(__MUSL__)
	tab->request.gaicb_ptr = malloc(sizeof(struct gaicb));
	bzero(tab->request.gaicb_ptr, sizeof(struct gaicb));
	if (!tab->request.gaicb_ptr) return fatalI();
        tab->request.gaicb_ptr->ar_name = tab->request.host;
	tab->request.resolved = 0;
        struct sigevent sevp;
	bzero(&sevp, sizeof(sevp));
        sevp.sigev_notify = SIGEV_THREAD;
        sevp.sigev_notify_function = dns_async;
	sevp.sigev_value.sival_ptr = tab;
        int ret = getaddrinfo_a(GAI_NOWAIT, &tab->request.gaicb_ptr, 1, &sevp);
	if (ret) {
		snprintf(tab->error, sizeof(tab->error),
			 "Unable request domain name: %s", tab->request.host);
		tab->show_error = 1;
		return -1;
	}

	long start = time(NULL);
	for (int i=0; !tab->request.resolved; i++) {
		if (tab->request.state == STATE_CANCEL) break;
		if (time(NULL) - start > TIMEOUT) break;
		nanosleep(&timeout, NULL);
	}
	
	if (tab->request.resolved != 1 ||
	    tab->request.gaicb_ptr->ar_result == NULL) {
		gai_cancel(tab->request.gaicb_ptr);
		free(tab->request.gaicb_ptr);
		snprintf(tab->error, sizeof(tab->error),
			 "Unknown domain name: %s", tab->request.host);
		tab->show_error = 1;
		return -1;
	}
	struct addrinfo *result = tab->request.gaicb_ptr->ar_result;
#else
	struct addrinfo hints, *result;
        bzero(&hints, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags |= AI_CANONNAME;
	errno = 0;

        if ((getaddrinfo(tab->request.host, NULL, &hints, &result))) {
                snprintf(tab->error, sizeof(tab->error),
			 "Unknown domain name: %s, %s",
			 tab->request.host, strerror(errno));
		tab->show_error = 1;
		return -1;
        }
#endif

	struct sockaddr_in addr4;
	struct sockaddr_in6 addr6;
	bzero(&addr4, sizeof(addr4));
	bzero(&addr6, sizeof(addr6));

	int error = 0;
	if (result->ai_family == AF_INET) {
		addr4.sin_addr =
			((struct sockaddr_in*)result->ai_addr)->sin_addr;
		addr4.sin_family = AF_INET;
		addr4.sin_port = htons(tab->request.port);
		tab->request._addr.addr4 = addr4;
		tab->request.addr =
			(struct sockaddr*)&tab->request._addr.addr4;
	}
	else if (result->ai_family == AF_INET6) {
		addr6.sin6_addr =
			((struct sockaddr_in6*)result->ai_addr)->sin6_addr;
		addr6.sin6_family = AF_INET6;
		addr6.sin6_port = htons(tab->request.port);
		tab->request._addr.addr6 = addr6;
		tab->request.addr =
			(struct sockaddr*)&tab->request._addr.addr6;
	} else {
		snprintf(tab->error, sizeof(tab->error), 
			 "Unexpected error, invalid address family %s",
			 tab->request.host);
		error = 1;
	}
	tab->request.family = result->ai_family;
	freeaddrinfo(result);
#ifdef __linux__
	free(tab->request.gaicb_ptr);
#endif
	if (error) {
		tab->request.state = STATE_CANCEL;
		return -1;
	}

	tab->request.state = STATE_DNS;
	return 0;
}

int gmi_request_connect(struct gmi_tab* tab) {

	tab->request.socket = socket(tab->request.family, SOCK_STREAM, 0); 
	if (tab->request.socket == -1) {
		snprintf(tab->error, sizeof(tab->error),
			 "Failed to create socket");
		return -1;
	}
	int flags = fcntl(tab->request.socket, F_GETFL);
	if (flags == -1 ||
	    fcntl(tab->request.socket, F_SETFL, flags|O_NONBLOCK) == -1) {
		snprintf(tab->error, sizeof(tab->error),
			 "Failed to create non-blocking socket");
		return -1;
	}

	int addr_size = (tab->request.family == AF_INET)?
			sizeof(struct sockaddr_in):
			sizeof(struct sockaddr_in6);

	int connected = 0;
	int failed = connect(tab->request.socket,
			     tab->request.addr, addr_size);
	failed = failed?(errno != EAGAIN && errno != EWOULDBLOCK &&
			 errno != EINPROGRESS && errno != EALREADY &&
			 errno != 0):failed;
	while (!failed) {
		struct pollfd fds[2];
		fds[0].fd = tab->thread.pair[0];
		fds[0].events = POLLIN;
		fds[1].fd = tab->request.socket;
		fds[1].events = POLLOUT;
		int count = poll(fds, 2, TIMEOUT * 1000);
		if (count < 1 || fds[1].revents != POLLOUT ||
			tab->request.state == STATE_CANCEL) break;
		int value;
		socklen_t len = sizeof(value);
		int ret = getsockopt(tab->request.socket, SOL_SOCKET,
				     SO_ERROR, &value, &len);
		connected = (value == 0 && ret == 0);
		break;
	}

	if (!connected) {
		snprintf(tab->error, sizeof(tab->error), 
			 "Connection to %s timed out : %s",
			 tab->request.host, strerror(errno));
		return -1;
	}

	if (tls_connect_socket(tab->request.tls, tab->request.socket,
			       tab->request.host)) {
		snprintf(tab->error, sizeof(tab->error), 
			 "Unable to connect to: %s : %s",
			 tab->request.host, tls_error(tab->request.tls));
		return -1;
	}
	return 0;
}

int gmi_request_handshake(struct gmi_tab* tab) {
	if (tls_connect_socket(tab->request.tls,
			       tab->request.socket,
			       tab->request.host)) {
		snprintf(tab->error, sizeof(tab->error), 
			 "Unable to connect to: %s : %s",
			 tab->request.host, tls_error(tab->request.tls));
		return -1;
	}
	if (tab->request.state == STATE_CANCEL) return -1;
	int ret = 0;
	time_t start = time(0);
	while ((ret = tls_handshake(tab->request.tls))) {
		if (time(NULL) - start > TIMEOUT ||
		    tab->request.state == STATE_CANCEL ||
		    (ret < 0 &&
		     ret != TLS_WANT_POLLIN &&
		     ret != TLS_WANT_POLLOUT))
			break;
		if (ret == TLS_WANT_POLLIN || ret == TLS_WANT_POLLOUT)
			nanosleep(&timeout, NULL);
	}
	if (ret) {
		snprintf(tab->error, sizeof(tab->error),
			 "Failed to handshake: %s (%s)",
			 tls_error(tab->request.tls),
			 tab->request.host);
		return -1;
	}
	if (tab->request.state == STATE_CANCEL) return -1;
	ret = cert_verify(tab->request.host,
			  tls_peer_cert_hash(tab->request.tls),
			  tls_peer_cert_notbefore(tab->request.tls),
			  tls_peer_cert_notafter(tab->request.tls));
	switch (ret) {
	case 0: // success
		break;
	case -1: // invalid certificate
		snprintf(tab->error, sizeof(tab->error),
			 "Failed to verify server certificate for %s" \
			 "(The certificate changed)",
			 tab->request.host);
		return -1;
	case -5: // expired
		snprintf(tab->error, sizeof(tab->error),
			 "Expired certificate, " \
			 "the certificate for %s has expired",
			 tab->request.host);
		return -1;
	case -6: // certificate changed
		snprintf(tab->error, sizeof(tab->error),
			 "Invalid certificate, enter \":forget %s\"" \
			 " to forget the old certificate.",
			 tab->request.host);
		return -1;
	default: // failed to write certificate
		snprintf(tab->error, sizeof(tab->error),
			 "Failed to write %s certificate information : %s",
			 tab->request.host, strerror(errno));
		return -1;
	}

	if (tab->request.state == STATE_CANCEL) return -1;
	char buf[MAX_URL];
	strlcpy(buf, "gemini://", GMI + 1);
	char toascii[1024];
	if (idn_to_ascii(&tab->request.url[GMI],
			 sizeof(tab->request.url) - GMI,
			 toascii, sizeof(toascii))) {
		snprintf(tab->error, sizeof(tab->error),
			 "Failed to parse url: %s", &tab->request.url[GMI]);
		return -1;
	}
	ssize_t len = strlcpy(&buf[GMI], toascii, sizeof(buf) - GMI) + GMI;
	char url[1024];
	len = parse_query(buf, MAX_URL, url, MAX_URL);
	if (len >= MAX_URL ||
	    (len += strlcpy(&url[len], "\r\n", MAX_URL - len)) >= MAX_URL) {
		snprintf(tab->error, sizeof(tab->error),
			 "Url too long: %s", url);
		return -1;
	}

	if (tls_write(tab->request.tls, url, len) < len) {
		snprintf(tab->error, sizeof(tab->error),
			 "Failed to send data to: %s", tab->request.host);
		return -1;
	}

	if (tab->request.state == STATE_CANCEL) return -1;
	tab->request.state = STATE_CONNECTED;
	return 0;
}

int gmi_request_header(struct gmi_tab* tab) {
	time_t now = time(0);
	char buf[1024];
	int recv = 0;
	while (1) {
		if (tab->request.state == STATE_CANCEL) break;
		if (time(0) - now >= TIMEOUT) {
			snprintf(tab->error, sizeof(tab->error),
				 "Connection to %s timed out (sent no data)",
				 tab->request.host);
			return -1;
		}
		int n = tls_read(tab->request.tls, &buf[recv],
				 sizeof(buf) - recv - 1);
		if (n == TLS_WANT_POLLIN) {
			nanosleep(&timeout, NULL);
			continue;
		}
		if (n < 1) {
			recv = -1;
			break;
		}
		recv += n;
		buf[recv] = '\0';
		if (strstr(buf, "\r\n")) break;
	}
	if (tab->request.state == STATE_CANCEL) return -1;
	
	if (recv <= 0) {
		snprintf(tab->error, sizeof(tab->error),
			 "[%d] Invalid data from: %s",
			 recv, tab->request.host);
		return -1;
	}
	if (!strstr(buf, "\r\n")) {
		snprintf(tab->error, sizeof(tab->error),
			 "Invalid data from: %s (no CRLF)", tab->request.host);
		return -1;
	}
	char* ptr = strchr(buf, ' ');
	if (!ptr) {
		snprintf(tab->error, sizeof(tab->error),
			 "Invalid data from: %s (no metadata)",
			 tab->request.host);
		return -1;
	}
	strlcpy(tab->request.error, ptr +1, sizeof(tab->request.error));

	int previous_code = tab->page.code;
	char c = buf[2];
	buf[2] = '\0';
	tab->page.code = atoi(buf);
	buf[2] = c;

	if (tab->request.state == STATE_CANCEL) return -1;
	switch (tab->page.code) {
	case 10:
	case 11:
		ptr++;
		strlcpy(client.input.label, ptr, sizeof(client.input.label));
		ptr = strchr(client.input.label, '\n');
		if (ptr) *ptr = '\0';
		ptr = strchr(client.input.label, '\r');
		if (ptr) *ptr = '\0';
		tab->request.state = STATE_RECV_HEADER;
		tab->request.recv = recv;
		return 2; // input
	case 20:
	{
#ifdef TERMINAL_IMG_VIEWER
		tab->page.img.tried = 0;
#endif
		char* meta = strchr(++ptr, '\r');
		if (!meta) {
			snprintf(tab->error, sizeof(tab->error),
				 "Invalid data from: %s", tab->request.host);
			return -1;
		}
		*meta = '\0';
		strlcpy(tab->request.meta, ptr, MAX_META);
		*meta = '\r';
		if ((strcasestr(tab->request.meta, "charset=") && 
		    !strcasestr(tab->request.meta, "charset=utf-8"))
		   || ((strncmp(tab->request.meta, "text/", 5))
#ifdef TERMINAL_IMG_VIEWER
		   && (strncmp(tab->request.meta, "image/", 6))
#endif
		   )) {
			if (tab->history)
				strlcpy(tab->url, tab->history->url,
					sizeof(tab->url));
			else
				strlcpy(tab->url, "about:home",
					sizeof(tab->url));
			tab->request.ask = 2;
			snprintf(tab->request.info, sizeof(tab->request.info), 
				 "# Non-renderable meta-data : %s",
				 tab->request.meta);
			strlcpy(tab->request.action, "download", 
				sizeof(tab->request.action));
			tb_interupt();
			while (tab->request.ask == 2) 
				nanosleep(&timeout, NULL);
			tab->request.download = tab->request.ask;
			if (!tab->request.download) {
				tab->request.recv = -1;
				return 1; // download
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
		return -2;
	case 41:
		snprintf(tab->error, sizeof(tab->error),
			 "Server unavailable");
		return -2;
	case 42:
		snprintf(tab->error, sizeof(tab->error),
			 "CGI error");
		return -2;
	case 43:
		snprintf(tab->error, sizeof(tab->error),
			 "Proxy error");
		return -2;
	case 44:
		snprintf(tab->error, sizeof(tab->error),
			 "Slow down: server above rate limit");
		return -2;
	case 50:
		snprintf(tab->error, sizeof(tab->error),
			 "Permanent failure");
		return -2;
	case 51:
		snprintf(tab->error, sizeof(tab->error),
			 "Not found %s", tab->request.url);
		return -2;
	case 52:
		snprintf(tab->error, sizeof(tab->error),
			 "Resource gone");
		return -2;
	case 53:
		snprintf(tab->error, sizeof(tab->error),
			 "Proxy request refused");
		return -2;
	case 59:
		snprintf(tab->error, sizeof(tab->error),
			 "Bad request");
		return -2;
	case 60:
		snprintf(tab->error, sizeof(tab->error),
			 "Client certificate required");
		return -2;
	case 61:
		snprintf(tab->error, sizeof(tab->error),
			 "Client not authorised");
		return -2;
	case 62:
		snprintf(tab->error, sizeof(tab->error),
			 "Client certificate not valid");
		return -2;
	default:
		snprintf(tab->error, sizeof(tab->error),
			 "Unknown status code: %d", tab->page.code);
		tab->page.code = previous_code;
		return -2;
	}
	if (tab->request.state == STATE_CANCEL) return -1;
	tab->request.state = STATE_RECV_HEADER;
	tab->request.recv = recv;
	tab->request.data = malloc(tab->request.recv + 1);
	if (!tab->request.data) return fatalI();
	memcpy(tab->request.data, buf, recv);
	return 0;
}

int gmi_request_body(struct gmi_tab* tab) {
	time_t now = time(0);
	char buf[1024];
	while (tab->request.state != STATE_CANCEL) {
		if (time(0) - now >= TIMEOUT) {
			snprintf(tab->error, sizeof(tab->error),
				 "Server %s stopped responding",
				 tab->request.host);
			return -1;
		}
		int bytes = tls_read(tab->request.tls, buf, sizeof(buf));
		if (tab->request.state == STATE_CANCEL) return -1;
		if (bytes == 0) break;
		if (bytes == TLS_WANT_POLLIN || bytes == TLS_WANT_POLLOUT) {
			nanosleep(&timeout, NULL);
			continue;
		}
		if (bytes < 1) {
			snprintf(tab->error, sizeof(tab->error),
				 "Invalid data from %s: %s",
				 tab->request.host,
				 tls_error(tab->request.tls));
			return -1;
		}
		tab->request.data = realloc(tab->request.data, 
					    tab->request.recv + bytes + 1);
		memcpy(&tab->request.data[tab->request.recv], buf, bytes);
		tab->request.data[tab->request.recv + bytes] = '\0';
		tab->request.recv += bytes;
		now = time(0);
	}
	if (tab->request.state == STATE_CANCEL) return -1;
	tab->request.state = STATE_RECV_BODY;
	return 0;
}

void* gmi_request_thread(struct gmi_tab* tab) {
	unsigned int signal = 0;
	while (!client.shutdown) {
		tab->selected = 0;
		tab->request.state = STATE_DONE;
		if (tab->page.code != 30 && tab->page.code != 31)
			tb_interupt();
		if (recv(tab->thread.pair[0], &signal, 4, 0) != 4 ||
		    client.shutdown || signal == 0xFFFFFFFF) break;
		bzero(&tab->request, sizeof(tab->request));
		tab->request.state = STATE_STARTED;
		int ret = gmi_request_init(tab, tab->thread.url,
					   tab->thread.add);
		if (tab->request.state == STATE_CANCEL ||
		    ret || gmi_request_dns(tab)) {
			if (tab->request.tls) {
				tls_close(tab->request.tls);
				tls_free(tab->request.tls);
				tab->request.tls = NULL;
			}
			if (ret == -1)
				tab->show_error = 1;
			tab->request.recv = ret;
			continue;
		}
		if (gmi_request_connect(tab) ||
		    tab->request.state == STATE_CANCEL) {
			close(tab->request.socket);
			if (tab->request.tls) {
				tls_close(tab->request.tls);
				tls_free(tab->request.tls);
				tab->request.tls = NULL;
			}
			tab->show_error = 1;
			tab->request.recv = -1;
			continue;
		}
		ret = -1;
		if (!gmi_request_handshake(tab) &&
		    tab->request.state != STATE_CANCEL) {
			ret = gmi_request_header(tab);
			if (!ret || ret == 2) gmi_request_body(tab);
		}

		if (ret == -2) {
			char* cr = strchr(tab->request.error, '\r');
			if (cr) {
				*cr = '\0';
				char buf[256];
				snprintf(buf, sizeof(buf),
					 "%s (%d : %s)", tab->request.error,
					 tab->page.code, tab->error);
				strlcpy(tab->error, buf, sizeof(tab->error));
				*cr = '\r';
			}
		}
		if (ret == -1 || ret == -2) {
			free(tab->request.data);
			tab->request.data = NULL;
			if (tab->history)
				strlcpy(tab->url, tab->history->url,
					sizeof(tab->url));
			else
				strlcpy(tab->url, "about:home",
					sizeof(tab->url));
			tab->show_error = 1;
			tab->request.recv = -1;
			tab->page.code = 20;
		}

		if (tab->request.socket != -1)
			close(tab->request.socket);
		if (tab->request.tls) {
			tls_close(tab->request.tls);
			tls_free(tab->request.tls);
			tab->request.tls = NULL;
		}
		if (tab->request.recv > 0 &&
		    (tab->page.code == 11 || tab->page.code == 10)) {
			free(tab->request.data);
			tab->request.data = NULL;
			strlcpy(tab->url, tab->request.url, sizeof(tab->url));
			continue;
		}
		if (tab->request.recv > 0 &&
		    (tab->page.code == 31 || tab->page.code == 30)) {
			tab->request.data[tab->request.recv] = '\0';
			char* ptr = strchr(tab->request.data, ' ');
			if (!ptr) {
				free(tab->request.data);
				continue;
			}
			char* ln = strchr(ptr + 1, ' ');
			if (ln) *ln = '\0';
			ln = strchr(ptr, '\n');
			if (ln) *ln = '\0';
			ln = strchr(ptr, '\r');
			if (ln) *ln = '\0';
			strlcpy(tab->url, tab->request.url, sizeof(tab->url));
			int r = gmi_nextlink(tab, tab->url, ptr + 1);
			if (r < 1 || tab->page.code != 20) {
				free(tab->request.data);
				continue;
			}
			tab->page.data_len = r;
			free(tab->request.data);
			tab->request.data = NULL;
			gmi_load(&tab->page);
			tab->history->page = tab->page;
			tab->history->cached = 1;
			tab->request.recv = r;
			continue;
		}
		if (!tab->request.download &&
		    tab->request.recv > 0 &&
		    tab->page.code == 20) {
			tab->search.entry[0] = '\0';
			struct gmi_link* link_ptr = tab->history?
						    tab->history->prev:NULL;
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
			pthread_mutex_lock(&tab->render_mutex);
			if (!tab->thread.add)
				gmi_freepage(&tab->page);
			bzero(&tab->page, sizeof(struct gmi_page));
			tab->page.code = 20;
			strlcpy(tab->page.meta, tab->request.meta,
				sizeof(tab->page.meta));
			strlcpy(tab->url, tab->request.url,
				sizeof(tab->url));
			tab->page.data_len = tab->request.recv;
			tab->page.data = tab->request.data;
			gmi_load(&tab->page);
			if (tab->thread.add)
				gmi_addtohistory(tab);
			else if (tab->history) {
				tab->history->page = tab->page;
				tab->history->cached = 1;
			}
			tab->scroll = -1;
			pthread_mutex_unlock(&tab->render_mutex);
		}
		if (tab->request.download &&
		    tab->request.recv > 0 &&
		    tab->page.code == 20) {
			int len = strnlen(tab->request.url,
					  sizeof(tab->request.url));
			if (tab->request.url[len-1] == '/')
				tab->request.url[len-1] = '\0';
			char* ptr = strrchr(tab->request.url, '/');
			FILE* f;
			int fd = getdownloadfd();	
			if (fd < 0) {
				snprintf(tab->error, sizeof(tab->error),
					 "Unable to open download folder");
				tab->show_error = 1;
				goto request_end;
			}
			char path[1024];
			if (ptr)
				strlcpy(path, ptr+1, sizeof(path));
			else {
#ifdef __OpenBSD__
				char format[] = "output_%lld.dat";
#else
				char format[] = "output_%ld.dat";
#endif

				snprintf(path, sizeof(path),
					 format, time(NULL));
			}
			for (unsigned int i = 0;
			     i < sizeof(path) && path[i] && ptr; i++) {
				char c = path[i];
				if ((path[i] == '.' && path[i+1] == '.') ||
				    !(c == '.' ||
					(c >= 'A' && c <= 'Z') ||
					(c >= 'a' && c <= 'z') ||
					(c >= '0' && c <= '9')
				     ))
					path[i] = '_';
			}
			int dfd = openat(fd, path,
					 O_CREAT|O_EXCL|O_WRONLY, 0600);
			if (dfd < 0 && errno == EEXIST) {
				char buf[1024];
#ifdef __OpenBSD__
				char format[] = "%lld_%s";
#else
				char format[] = "%ld_%s";
#endif
				snprintf(buf, sizeof(buf), format,
					 time(NULL), path);
				strlcpy(path, buf, sizeof(path));
				dfd = openat(fd, path,
					     O_CREAT|O_EXCL|O_WRONLY, 0600);
				if (dfd < 0) {
					snprintf(tab->error,
						 sizeof(tab->error),
						 "Failed to write to " \
						 "the download folder");
					tab->show_error = 1;
					goto request_end;
				}
			}
			f = fdopen(dfd, "wb");
			if (!f) {
				snprintf(tab->error, sizeof(tab->error),
					 "Failed to write " \
					 "the downloaded file");
				tab->show_error = 1;
				goto request_end;
			}
#if defined(__FreeBSD__) && !defined(NO_SANDBOX)
			if (make_writeonly(f)) {
				client.shutdown = 1;
				break;
			}
#endif
			ptr = strchr(tab->request.data, '\n')+1;
			if (!ptr) {
				tab->show_error = 1;
				snprintf(tab->error, sizeof(tab->error),
					 "Server sent invalid data");
				goto request_end;
			}
			fwrite(ptr, 1, 
			       tab->request.recv -
			       (ptr-tab->request.data), f);
			fclose(f);
#ifndef DISABLE_XDG
			int fail = 0;
			if (client.xdg) {
				tab->request.ask = 2;
				snprintf(tab->request.info,
					 sizeof(tab->request.info), 
					 "# %s download",
					 tab->request.meta);
				strlcpy(tab->request.action, "open", 
					sizeof(tab->request.action));
				tb_interupt();
				while (tab->request.ask == 2) 
					nanosleep(&timeout, NULL);
				if (tab->request.ask) {
					char file[1024];
					snprintf(file, sizeof(file), "%s/%s", 
						 download_path, path);
					fail = xdg_open(file);
				}
			}
			if (fail) {
				tab->show_error = 1;
				snprintf(tab->error, sizeof(tab->info),
					"Failed to open %s", path);
			} else
#endif
			{
				tab->show_info = 1;
				snprintf(tab->info, sizeof(tab->info),
					"File downloaded to %s", path);
			}
request_end:
			free(tab->request.data);
			tab->request.recv = tab->page.data_len;
		}
	}
	return NULL;
}

int gmi_request(struct gmi_tab *tab, const char *url, int add) {
	if (tab->request.state != STATE_DONE)
		tab->request.state = STATE_CANCEL;
	for (int i = 0; i < 5 && tab->request.state == STATE_CANCEL; i++)
		nanosleep(&timeout, NULL);
	tab->thread.add = add;
	strlcpy(tab->thread.url, url, sizeof(tab->thread.url));
	int signal = 0;
	if (send(tab->thread.pair[1], &signal, sizeof(signal), 0) !=
	    sizeof(signal))
		return -1;
	return 0;
}

int gmi_loadfile(struct gmi_tab* tab, char* path) {
	FILE* f = fopen(path, "rb");
	if (!f) {
		snprintf(tab->error, sizeof(tab->error),
			 "Failed to open %s", path);
		tab->show_error = 1;
		return -1;
	}
#if defined(__FreeBSD__) && !defined(NO_SANDBOX)
	if (make_readonly(f)) {
		fclose(f);
		client.shutdown = 1;
		return -1;
	}
#endif
	fseek(f, 0, SEEK_END);
	size_t len = ftell(f);
	fseek(f, 0, SEEK_SET);
	char* data = malloc(len);
	if (!data) return fatalI();
	if (len != fread(data, 1, len, f)) {
		fclose(f);
		free(data);
		return -1;
	}
	fclose(f);
	pthread_mutex_lock(&tab->render_mutex);
	tab->page.code = 20;
	tab->page.data_len = len;
	if (tab->page.data) free(tab->page.data);
	tab->page.data = data;
	snprintf(tab->url, sizeof(tab->url), "file://%s/", path);
	int i = strnlen(path, 1024);
	int gmi = 0;
	if (i > 4 && path[i - 1] == '/') i--;
	if (i > 4 && path[i - 4] == '.' &&
	    path[i - 3] == 'g' &&
	    path[i - 2] == 'm' &&
	    path[i - 1] == 'i') 
		gmi = 1;
	if (gmi)
		strlcpy(tab->page.meta, "text/gemini", sizeof(tab->page.meta));
	else
		strlcpy(tab->page.meta, "text/text", sizeof(tab->page.meta));
	tab->page.no_header = 1;
	gmi_load(&tab->page);
	gmi_addtohistory(tab);
	pthread_mutex_unlock(&tab->render_mutex);
	return len;
}

int gmi_init() {
	if (tls_init()) {
		tb_shutdown();
		printf("Failed to initialize TLS\n");
		return -1;
	}
	
	config = tls_config_new();
	if (!config) {
		tb_shutdown();
		printf("Failed to initialize TLS config\n");
		return -1;
	}

	config_empty = tls_config_new();
	if (!config_empty) {
		tb_shutdown();
		printf("Failed to initialize TLS config\n");
		return -1;
	}

	tls_config_insecure_noverifycert(config);
	tls_config_insecure_noverifycert(config_empty);
#ifndef DISABLE_XDG
	int xdg = client.xdg;
#endif
	bzero(&client, sizeof(client));
#ifndef DISABLE_XDG
	client.xdg = xdg;
#endif

	int fd = getconfigfd();
	if (fd < 0) {
		tb_shutdown();
		printf("Failed to open config folder\n");
		return -1;
	}

	if (gmi_loadbookmarks()) {
		gmi_newbookmarks();
	}

	if (cert_load()) {
		tb_shutdown();
		printf("Failed to load known hosts\n");
		return -1;
	}
		
	return 0;
}

void gmi_free() {
	while (client.tab)
		gmi_freetab(client.tab);
	gmi_savebookmarks();
	for (int i = 0; client.bookmarks[i]; i++)
		free(client.bookmarks[i]);
	free(client.bookmarks);
	cert_free();
}

#ifndef DISABLE_XDG
#ifdef xdg_open
#undef xdg_open
#endif
int xdg_open(char* str) {
	if (client.xdg) {
		char buf[4096];
		snprintf(buf, sizeof(buf), "xdg-open %s>/dev/null 2>&1", str);
		if (fork() == 0) {
			setsid();
			char* argv[] = {"/bin/sh", "-c", buf, NULL};
			execvp(argv[0], argv);
			exit(0);
		}
	}
	return 0;
}
#endif
