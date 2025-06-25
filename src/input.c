#include <string.h>
#include <strings.h>
#include <unistd.h>
#define TB_IMPL
#include "wcwidth.h"
#undef wcwidth
#define wcwidth(x) mk_wcwidth(x)
#ifdef sun
#include <termios.h>
void cfmakeraw(struct termios *t);
#endif
#include <termbox.h>
#include "gemini.h"
#include "display.h"
#include "cert.h"
#include "str.h"
#include "url.h"
#include "sandbox.h"
#include "util.h"
#include <stdio.h>

int get_cursor_pos(void) {
	int pos = 0;
	for (int i = 0; i < client.input.cursor; i++) {
		pos += tb_utf8_char_length(client.input.field[pos]);
		if (!client.input.field[pos]) break;
	}
	return pos;
}

int vim_counter(void) {
	int counter = atoi(client.vim.counter);
	bzero(client.vim.counter, sizeof(client.vim.counter));
	return counter?counter:1;
}

void fix_scroll(struct gmi_tab* tab) {
	int h = tab->page.lines - tb_height() + 2 + (client.tabs_count > 1);
	if (tab->scroll > h)
		tab->scroll = h;
	if (tab->scroll < 0)
		tab->scroll = -1;
}

int command(void) {
	struct gmi_tab* tab = client.tab;
	struct gmi_page* page = &tab->page;

	// Trim
	for (int i = STRNLEN(client.input.field) - 1;
			client.input.field[i] == ' ' ||
			client.input.field[i] == '\t'; i--)
		client.input.field[i] = '\0';

	if (!STRCMP(client.input.field, ":q")) {
		gmi_freetab(tab);
		client.input.field[0] = '\0';
		if (client.tabs_count < 1)
			return 1;
		if (client.tabs_count == 1)
			fix_scroll(client.tab);
		return 0;
	}
	if (!STRCMP(client.input.field, ":qa"))
		return 1;
	if ((client.input.field[1] == 'n' && client.input.field[2] == 't'
	    && client.input.field[3] == '\0')
	     || !STRCMP(client.input.field, ":tabnew")) {
		client.tab = gmi_newtab();
		client.input.field[0] = '\0';
		return 0;
	}
	if (STARTWITH(client.input.field, ":nt ") ||
			STARTWITH(client.input.field, ":tabnew ")) {
		int i = client.input.field[1] == 'n' ? 4 : 8;
		client.input.cursor = 0;
		int id = atoi(&client.input.field[i]);
		if (id != 0 || 
		    (client.input.field[i] == '0' &&
		     client.input.field[i + 1] == '\0')) {
			gmi_goto_new(tab, id);
			client.input.field[0] = '\0';
		} else {
			client.tab = gmi_newtab_url(&client.input.field[i]);
			client.input.field[0] = '\0';
		}
		return 0;
	}
	if (STARTWITH(client.input.field, ":o ")) {
		char urlbuf[MAX_URL];
		if (STRLCPY(urlbuf, &client.input.field[3]) >= sizeof(urlbuf)) {
			tab->show_error = 1;
			snprintf(tab->error, sizeof(tab->error),
				 "Url too long");
			return 0;
		}
		client.input.field[0] = '\0';
		gmi_cleanforward(tab);
		int bytes = gmi_request(tab, urlbuf, 1);
		if (bytes > 0) {
			tab->scroll = -1;
		}
		if (page->code == 11 || page->code == 10) {
			client.input.mode = 1;
			client.input.cursor = 0;
		}
		tab->selected = 0;
		return 0;
	}
	if (STARTWITH(client.input.field, ":s")) {
		char urlbuf[MAX_URL];
		snprintf(urlbuf, sizeof(urlbuf),
			 "gemini://tlgs.one/search?%s",
			 &client.input.field[3]);
		client.input.field[0] = '\0';
		gmi_cleanforward(tab);
		int bytes = gmi_request(tab, urlbuf, 1);
		if (bytes > 0) {
			tab->scroll = -1;
		}
		if (page->code == 11 || page->code == 10) {
			client.input.mode = 1;
			client.input.cursor = 0;
		}
		tab->selected = 0;
		return 0;
	}
	if (STARTWITH(client.input.field, ":add")) {
		char* title = client.input.field[4] == '\0'?
				NULL:&client.input.field[5];
		gmi_addbookmark(tab, tab->url, title);
		client.input.field[0] = '\0';
		tab->selected = 0;
		if (!strcmp("about:home", tab->url)) {
			gmi_freetab(tab);
			gmi_gohome(tab, 1);
		}
		return 0;
	}
	int ignore = STARTWITH(client.input.field, ":ignore");
	int forget = STARTWITH(client.input.field, ":forget");
	if (forget || ignore) {
		char* ptr = client.input.field + sizeof(":forget") - 1;
		int space = 0;
		for (; *ptr; ptr++) {
			if (*ptr != ' ') break;
			space++;
		}
		if (space == 0 || !*ptr) goto unknown;
		if ((ignore && cert_ignore_expiration(ptr)) ||
		    (forget && cert_forget(ptr))) {
			tab->show_error = 1;
			if (forget) {
				snprintf(tab->error, sizeof(tab->error),
					 "Unknown %s certificate", ptr);
			} else {
				strerror_r(errno, tab->error,
					   sizeof(tab->error));
			}
			return 0;
		}
		tab->show_info = 1;
		snprintf(tab->info, sizeof(tab->info),
			 "%s %s", ptr, (forget ?
			 "certificate removed from known hosts" :
			 "certificate expiration will be ignored"));
		client.input.field[0] = '\0';
		return 0;
	}
	if (!STRCMP(client.input.field, ":gencert")) {
		client.input.field[0] = '\0';
		tab->selected = 0;
		if (!STRCMP(tab->url, "about:home")) {
			tab->show_error = 1;
			snprintf(tab->error, sizeof(tab->error),
				 "Cannot create a certificate for this page");
			return 0;
		}
		char host[256];
		parse_url(tab->url, host, sizeof(host), NULL, 0, NULL);
		if (cert_create(host, tab->error, sizeof(tab->error))) {
			tab->show_error = 1;
			return 0;
		}
		cert_getcert(host, 1);
		tab->show_info = 1;
		snprintf(tab->info, sizeof(tab->info),
			 "Certificate generated for %s", host);
		return 0;
	}
	if (!STRCMP(client.input.field, ":exec")) {
#ifdef DISABLE_XDG
		tab->show_error = 1;
		snprintf(tab->error, sizeof(tab->error),
			 "xdg is disabled");
		return 0;
#else
		if (!*client.input.download) {
			snprintf(tab->error, sizeof(tab->error),
				 "No file was downloaded");
			tab->show_error = 1;
			client.input.field[0] = '\0';
			return 0;
		}
		xdg_open(client.input.download);
		client.input.field[0] = '\0';
		client.input.download[0] = '\0';
		return 0;
#endif
	}
	if (STARTWITH(client.input.field, ":download")) {
		char* ptr = client.input.field + sizeof(":download") - 1;
		int space = 0;
		for (; *ptr; ptr++) {
			if (*ptr != ' ') break;
			space++;
		}
		if (space == 0 && *ptr) goto unknown;
		char urlbuf[1024];
		char* url = strrchr(tab->history->url, '/');
		if (url && (*(url+1) == '\0')) {
			while (*url == '/' && url > tab->history->url)
				url--;
			char* ptr = url + 1;
			while (*url != '/' && url > tab->history->url)
				url--;
			if (*url == '/') url++;
			if (strlcpy(urlbuf, url, ptr - url + 1) >=
			    sizeof(urlbuf))
				return fatalI();
			url = urlbuf;
		} else if (url) url++;
		else url = tab->history->url;
		char* download = *ptr ? ptr : url;
#ifdef SANDBOX_SUN
		if (sandbox_download(tab, download))
			return -1;
		int fd = wr_pair[1];
#else
		int fd = openat(getdownloadfd(), download,
				O_CREAT|O_EXCL|O_WRONLY, 0600);
		char buf[1024];
		if (fd < 0 && errno == EEXIST) {
#ifdef __OpenBSD__
			snprintf(buf, sizeof(buf), "%lld_%s",
#else
			snprintf(buf, sizeof(buf), "%ld_%s",
#endif
				 time(NULL), download);
			fd = openat(getdownloadfd(), buf,
				    O_CREAT|O_EXCL|O_WRONLY, 0600);
			download = buf;
		}
		if (fd < 0) {
			tab->show_error = -1;
			snprintf(tab->error, sizeof(tab->error),
				 "Failed to write file : %s", strerror(errno));
			return 0;
		}
#endif
		char* data = strnstr(tab->page.data, "\r\n",
				     tab->page.data_len);
		uint64_t data_len = tab->page.data_len;
		if (!data) data = tab->page.data;
		else {
			data += 2;
			data_len -= (data - tab->page.data);
		}
#ifdef SANDBOX_SUN
		sandbox_dl_length(data_len);
#endif
		if (write(fd, data, data_len) != (ssize_t)data_len) return -1;
#ifndef SANDBOX_SUN
		close(fd);
#endif
		tab->show_info = 1;
		snprintf(tab->info, sizeof(tab->info),
			 "File downloaded : %s", download);
		STRLCPY(client.input.download, download);
		client.input.field[0] = '\0';
		tab->selected = 0;
		return 0;
	}
	if (client.input.field[0] == ':' && (atoi(&client.input.field[1]) ||
	   (client.input.field[1] == '0' && client.input.field[2] == '\0'))) {
		client.tab->scroll = atoi(&client.input.field[1]) - 1 -
				     !!client.tabs_count;
		if (client.tab->scroll < 0)
			client.tab->scroll = -!!client.tabs_count;
		fix_scroll(client.tab);
		client.input.field[0] = '\0';
		tab->selected = 0;
		return 0;
	}
	if (client.input.field[0] == '/') {
		STRLCPY(tab->search.entry, &client.input.field[1]);
		client.input.field[0] = '\0';
	} else if (client.input.field[1] == '\0') client.input.field[0] = '\0';
	else {
unknown:
		tab->show_error = -1;
		snprintf(tab->error, sizeof(tab->error),
			 "Unknown input: %s", &client.input.field[1]);
	}
	return 0;
}

int input_page(struct tb_event ev) {
	struct gmi_tab* tab = client.tab;
	struct gmi_page* page = &tab->page;
	int counter;
	switch (ev.key) {
	case TB_KEY_ESC:
		tab->selected = 0;
		bzero(client.vim.counter, sizeof(client.vim.counter));
		client.vim.g = 0;
		return 0;
	case TB_KEY_DELETE:
		if (strcmp(tab->url, "about:home")) return 0;
		if (tab->selected && tab->selected > 0 &&
		    tab->selected <= page->links_count) {
			gmi_removebookmark(tab->selected);
			tab->selected = 0;
			gmi_request(tab, tab->url, 0);
		}
		return 0;
	case TB_KEY_BACK_TAB:
		if (tab->selected && tab->selected > 0 &&
		    tab->selected <= page->links_count) {
			int linkid = tab->selected;
			tab->selected = 0;
			gmi_goto_new(tab, linkid);
		}
		return 0;
	case TB_KEY_ARROW_LEFT:
		if (ev.mod == TB_MOD_SHIFT)
			goto go_back;
		goto tab_prev;
	case TB_KEY_ARROW_RIGHT:
		if (ev.mod == TB_MOD_SHIFT)
			goto go_forward;
		goto tab_next;
	case TB_KEY_ARROW_UP:
		goto move_up;
	case TB_KEY_ARROW_DOWN:
		goto move_down;
	case TB_KEY_TAB:
		client.vim.g = 0;
		if (client.vim.counter[0] == '\0' ||
		    !atoi(client.vim.counter)) {
			if (!tab->selected)
				return 0;
			gmi_goto(tab, tab->selected);
			tab->selected = 0;
			bzero(client.vim.counter, sizeof(client.vim.counter));
			return 0;
		}
		tab->selected = atoi(client.vim.counter);
		bzero(client.vim.counter, sizeof(client.vim.counter));
		if (tab->selected > page->links_count) {
			snprintf(tab->error, sizeof(tab->error),
				 "Invalid link number");
			tab->selected = 0;
			tab->show_error = 1;
			return 0;
		}
		size_t len = STRLCPY(tab->selected_url,
				     page->links[tab->selected - 1]);
		if (len >= sizeof(tab->selected_url)) {
			snprintf(tab->error, sizeof(tab->error),
				 "Invalid link, above %lu characters",
				 sizeof(tab->selected_url));
			tab->selected = 0;
			tab->show_error = 1;
		}
		return 0;
	case TB_KEY_ENTER:
		if (client.vim.counter[0] != '\0' &&
		    atoi(client.vim.counter)) {
			tab->scroll += atoi(client.vim.counter);
			bzero(client.vim.counter, sizeof(client.vim.counter));
		}
		else tab->scroll++;
		fix_scroll(tab);
		client.vim.g = 0;
		return 0;
	case TB_KEY_PGUP:
		tab->scroll -= vim_counter() * tb_height() -
			       2 - (client.tabs_count>1);
		fix_scroll(tab);
		client.vim.g = 0;
		return 0;
	case TB_KEY_PGDN:
		tab->scroll += vim_counter() * tb_height() -
			       2 - (client.tabs_count>1);
		fix_scroll(tab);
		client.vim.g = 0;
		return 0;
	}
	switch (ev.ch) {
	case 'u':
		tab->show_error = 0;
		client.input.mode = 1;
		snprintf(client.input.field,
			 sizeof(client.input.field),
			 ":o %s", tab->url);
		client.input.cursor = utf8_len(client.input.field,
					       sizeof(client.input.field));
		break;
	case ':':
		tab->show_error = 0;
		client.input.mode = 1;
		client.input.cursor = 1;
		client.input.field[0] = ':';
		client.input.field[1] = '\0';
		break;
	case '/':
		tab->show_error = 0;
		client.input.mode = 1;
		client.input.cursor = 1;
		client.input.field[0] = '/';
		client.input.field[1] = '\0';
		break;
	case 'r': // Reload
		if (!tab->history) break;
		gmi_request(tab, tab->history->url, 0);
		break;
	case 'T': // Tab left
tab_prev:
		if (!client.vim.g) break;
		client.vim.g = 0;
		counter = vim_counter();
		if (!client.tab->next && !client.tab->prev) break;
		for (int i = counter; i > 0; i--) {
			if (client.tab->prev)
				client.tab = client.tab->prev;
			else
				while (client.tab->next)
					client.tab = client.tab->next;
			fix_scroll(client.tab);
		}
		break;
	case 't': // Tab right
tab_next:
		if (!client.vim.g) break;
		client.vim.g = 0;
		counter = vim_counter() % client.tabs_count;
		if (!client.tab->next && !client.tab->prev) break;
		for (int i = counter; i > 0; i--) {
			if (client.tab->next)
				client.tab = client.tab->next;
			else
				while (client.tab->prev)
					client.tab = client.tab->prev;
			fix_scroll(client.tab);
		}
		break;
	case 'H': // Back
go_back:
		if (!tab->history || !tab->history->prev) break;
		tab->selected = 0;
		if (page->code == 20 || page->code == 10 || page->code == 11) {
			tab->history->scroll = tab->scroll;
			if (!tab->history->next) {
				tab->history->page = tab->page;
				tab->history->cached = 1;
			}
			tab->history = tab->history->prev;
			tab->scroll = tab->history->scroll;
			if (tab->history->cached)
				tab->page = tab->history->page;
		} 
		if (tab->history->cached) {
			tab->page = tab->history->page;
			STRLCPY(tab->url, tab->history->url);
		} else if (gmi_request(tab, tab->history->url, 1) < 0) break;
		fix_scroll(client.tab);
		break;
	case 'L': // Forward
go_forward:
		if (!tab->history || !tab->history->next) break;
		tab->selected = 0;
		if (!tab->history->cached) {
			tab->history->page = tab->page;
			tab->history->cached = 1;
		}
		if (tab->history->next->cached)
			tab->page = tab->history->next->page;
		else if (gmi_request(tab, tab->history->next->url, 1) < 0)
			break;
		tab->history->scroll = tab->scroll;
		tab->history = tab->history->next;
		tab->scroll = tab->history->scroll;
		STRLCPY(tab->url, tab->history->url);
		fix_scroll(client.tab);
		break;
	case 'k': // UP
move_up:
		tab->scroll -= vim_counter();
		fix_scroll(tab);
		client.vim.g = 0;
		break;
	case 'j': // DOWN
move_down:
		tab->scroll += vim_counter();
		fix_scroll(tab);
		client.vim.g = 0;
		break;
	case 'f': // Show history
		display_history();
		break;
	case 'g': // Start of file
		if (client.vim.g) {
			tab->scroll = -1;
			client.vim.g = 0;
		} else client.vim.g++;
		break;
	case 'G': // End of file
		tab->scroll = page->lines-tb_height()+2;
		if (client.tabs_count != 1)
			tab->scroll++;
		if (tb_height()-2-(client.tabs_count>1) > page->lines)
			tab->scroll = -1;
		break;
	case 'n': // Next occurence
		tab->search.cursor++;
		tab->scroll = tab->search.pos[1] - tb_height()/2;
		fix_scroll(tab);
		break;
	case 'N': // Previous occurence
		tab->search.cursor--;
		tab->scroll = tab->search.pos[0] - tb_height()/2;
		fix_scroll(tab);
		break;
	default:
		if (!(ev.ch >= '0' && ev.ch <= '9'))
			break;
		tab->show_error = 0;
		tab->show_info = 0;
		unsigned int len = STRNLEN(client.vim.counter);
		if (len == 0 && ev.ch == '0') break;
		if (len >= sizeof(client.vim.counter)) break;
		client.vim.counter[len] = ev.ch;
		client.vim.counter[len+1] = '\0';
	}
	return 0;
}

int input_field(struct tb_event ev) {
	struct gmi_tab* tab = client.tab;
	struct gmi_page* page = &tab->page;
	int pos = 0;
	switch (ev.key) {
	case TB_KEY_ESC:
		client.input.mode = 0;
		client.input.cursor = 0;
		if (page->code == 11 || page->code == 10)
			page->code = 20;
		client.input.field[0] = '\0';
		tb_hide_cursor();
		return 0;
	case TB_KEY_BACKSPACE:
	case TB_KEY_BACKSPACE2:
		if (client.input.cursor >
		    ((page->code == 10 || page->code == 11)?0:1)) {
			if (client.input.field[0] == '/' &&
			    page->code == 20)
				tab->search.scroll = 1;
			char* ptr = client.input.field;
			int l = 1;
			int pos = 0;
			while (*ptr) {
				l = tb_utf8_char_length(*ptr);
				pos++;
				if (pos == client.input.cursor)
					break;
				ptr += l;
			}

			strlcpy(ptr, ptr + l,
				sizeof(client.input.field) -
				(ptr - client.input.field));
			client.input.cursor--;
		}
		return 0;
	case TB_KEY_ENTER:
		client.input.mode = 0;
		tb_hide_cursor();
		if (page->code == 10 || page->code == 11) {
			char urlbuf[MAX_URL];
			char* start = strstr(tab->url, "gemini://");
			char* request = strrchr(tab->url, '?');
			if (!(start?
			    strchr(&start[GMI], '/'):strchr(tab->url, '/')))
				snprintf(urlbuf, sizeof(urlbuf),
					 "%s/?%s", tab->url,
					 client.input.field);
			else if (request && request > strrchr(tab->url, '/')) {
				*request = '\0';
				snprintf(urlbuf, sizeof(urlbuf),
					 "%s?%s", tab->url,
					 client.input.field);
				*request = '?';
			} else
				snprintf(urlbuf, sizeof(urlbuf),
					 "%s?%s", tab->url,
					 client.input.field);
			int bytes = gmi_request(tab, urlbuf, 1);
			if (bytes>0) {
				tab = client.tab;
				tab->scroll = -1;
			}
			client.input.field[0] = '\0';
			return 0;
		}
		return command();
	case TB_KEY_ARROW_LEFT:
	{
		int min = 1;
		if (tab->page.code == 10 || tab->page.code == 11)
			min = 0;
		if (ev.mod != TB_MOD_CTRL) {
			if (client.input.cursor > min)
				client.input.cursor--;
			return 0;
		}
		pos = get_cursor_pos();
		while (client.input.cursor > min) {
			pos -= tb_utf8_char_length(client.input.field[pos]);
			client.input.cursor--;
			if (client.input.field[pos] == ' ' ||
			    client.input.field[pos] == '.')
				break;
		}
		return 0;
	}
	case TB_KEY_ARROW_RIGHT:
		pos = get_cursor_pos();
		if (ev.mod != TB_MOD_CTRL) {
			if (client.input.field[pos])
				client.input.cursor++;
			return 0;
		}
		while (client.input.field[pos]) {
			pos += tb_utf8_char_length(client.input.field[pos]);
			client.input.cursor++;
			if (!client.input.field[pos]) break;
			if (client.input.field[pos] == ' ' ||
			    client.input.field[pos] == '.')
				break;
		}
		return 0;
	}
	if (!ev.ch) {
		tb_hide_cursor();
		return 0;
	}
	char* end = client.input.field;
	while (*end)
		end += tb_utf8_char_length(*end);
	if ((size_t)(end - client.input.field) >=
	    sizeof(client.input.field) - 1)
		return 0;

	char insert[16];
	int insert_len = tb_utf8_unicode_to_char(insert, ev.ch);
	char* start = client.input.field;
	for (int i = 0; *start && i < client.input.cursor; i++)
		start += tb_utf8_char_length(*start);
	for (char* ptr = end; start <= ptr; ptr--)
		ptr[insert_len] = *ptr;

	memcpy(start, insert, insert_len);
	client.input.cursor++;
	end += insert_len;
	*end = '\0';
	if (client.input.field[0] != '/' || page->code != 20)
		return 0;
	int lines = 0;
	int posx = 0;
	int w = tb_width();
	int found = 0;
	for (int i = 0; i < tab->page.data_len - 1; i++) {
		if (posx == 0 && tab->page.data[i] == '=' &&
		    tab->page.data[i + 1] == '>') {
			int ignore = 0;
			int firstspace = 0;
			while (i + ignore < tab->page.data_len) {
				if (tab->page.data[i + ignore] != '\n') {
					i += 2 + firstspace;
					break;
				}
				if (tab->page.data[i + ignore] == ' ') {
					ignore++;
					if (firstspace) continue;
					i += ignore;
					break;
				}
				ignore++;
				firstspace = 1;
			}
			if (i > tab->page.data_len)
				break;
		}
		if (lines &&
		    !strncasecmp(&client.input.field[1],
			         &tab->page.data[i],
			         strnlen(&client.input.field[1],
			         sizeof(client.input.field) - 1))) {
			found = 1;
			break;
		}
		if (posx == w || tab->page.data[i] == '\n') {
			lines++;
			posx = 0;
			continue;
		}
		posx++;
	}
	if (found) {
		tab->scroll = lines - tb_height()/2;
		fix_scroll(tab);
	}
	return 0;
}

int input(struct tb_event ev) {
	struct gmi_tab* tab = client.tab;
	struct gmi_page* page = &tab->page;
	if (page->code == 11 || page->code == 10) {
		if (!client.input.mode) client.input.cursor = 0;
		client.input.mode = 1;
	}
	if (ev.type == TB_EVENT_RESIZE) {
		fix_scroll(tab);
		return 0;
	}
	if (ev.type != TB_EVENT_KEY) return 0;
	if (client.input.mode)
		return input_field(ev);
	return input_page(ev);
}

int tb_interupt(void) {
	int sig = 0;
	return write(global.resize_pipefd[1], &sig, sizeof(sig)) ==
		sizeof(sig) ? 0 : -1;
}
