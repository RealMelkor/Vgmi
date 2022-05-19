/* See LICENSE file for copyright and license details. */
#include <string.h>
#include <strings.h>
#include <termbox.h>
#include "input.h"
#include "gemini.h"
#include "display.h"
#include "cert.h"
#include <stdio.h>
#ifdef __linux__
#include <bsd/string.h>
#endif

int command() {
	struct gmi_tab* tab = &client.tabs[client.tab];
	struct gmi_page* page = &tab->page;

	// Trim
	for (int i=strnlen(client.input.field, sizeof(client.input.field)) - 1;
	     client.input.field[i] == ' ' || client.input.field[i] == '\t'; i--)
		client.input.field[i] = '\0';

	if (client.input.field[1] == 'q' && client.input.field[2] == '\0') {
		client.tabs_count--;
		gmi_freetab(&client.tabs[client.tab]);
		for (int i = client.tab; i < client.tabs_count; i++) {
			client.tabs[i] = client.tabs[i+1];
		}
		if (client.tab > 0 && client.tab == client.tabs_count)
			client.tab--;
		client.input.field[0] = '\0';
		if (client.tabs_count < 1)
			return 1;
		return 0;
	}
	if (client.input.field[1] == 'q' && client.input.field[2] == 'a'
	    && client.input.field[3] == '\0') 
		return 1;
	if (client.input.field[1] == 'n' && client.input.field[2] == 't'
	    && client.input.field[3] == '\0') {
		gmi_newtab();
		client.tab = client.tabs_count - 1;
		client.input.field[0] = '\0';
		return 0;
	}
	if (client.input.field[1] == 'n' && client.input.field[2] == 't'
	   && client.input.field[3] == ' ') {
		client.input.cursor = 0;
		int id = atoi(&client.input.field[4]);
		if (id != 0 || (client.input.field[4] == '0' && client.input.field[5] == '\0')) {
			gmi_goto_new(tab, id);
			client.input.field[0] = '\0';
		} else {
			gmi_newtab_url(&client.input.field[4]);
			client.tab = client.tabs_count - 1;
			client.input.field[0] = '\0';
		}
		return 0;
	}
	if (client.input.field[1] == 'o' && client.input.field[2] == ' ') {
		char urlbuf[MAX_URL];
		if (strlcpy(urlbuf, &client.input.field[3], sizeof(urlbuf)) >= sizeof(urlbuf)) {
			client.input.error = 1;
			snprintf(tab->error, sizeof(tab->error), "Url too long");
			return 0;
		}
		client.input.field[0] = '\0';
		gmi_cleanforward(tab);
		int bytes = gmi_request(tab, urlbuf);
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
	if (client.input.field[1] == 's' && client.input.field[2] == ' ') {
		char urlbuf[MAX_URL];
		snprintf(urlbuf, sizeof(urlbuf),
			 "gemini://geminispace.info/search?%s", &client.input.field[3]);
		client.input.field[0] = '\0';
		gmi_cleanforward(tab);
		int bytes = gmi_request(tab, urlbuf);
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
	if (client.input.field[1] == 'a' && client.input.field[2] == 'd'
	   && client.input.field[3] == 'd' && 
	   (client.input.field[4] == ' ' || client.input.field[4] == '\0')) {
		char* title = client.input.field[4] == '\0'?NULL:&client.input.field[5];
		gmi_addbookmark(tab, tab->url, title);
		client.input.field[0] = '\0';
		tab->selected = 0;
		if (!strcmp("about:home", tab->url)) {
			gmi_freetab(tab);
			gmi_gohome(tab);
		}
		return 0;
	}
	if (strcmp(client.input.field, ":gencert") == 0) {
		char host[256];
		gmi_parseurl(client.tabs[client.tab].url, host, sizeof(host), NULL, 0, NULL);
		cert_create(host);
		client.input.field[0] = '\0';
		tab->selected = 0;
		return 0;
	}
	if (atoi(&client.input.field[1]) ||
	   (client.input.field[1] == '0' && client.input.field[2] == '\0')) {
		int bytes = gmi_goto(tab, atoi(&client.input.field[1]));
		if (bytes > 0) {
			tab->scroll = -1;
		}
		client.input.field[0] = '\0';
		tab->selected = 0;
		return 0;
	}
	if (client.input.field[1] == '\0') client.input.field[0] = '\0';
	else {
		client.input.error = -1;
		snprintf(tab->error, sizeof(tab->error),
			 "Unknown input: %s", &client.input.field[1]);
	}
	return 0;
}

int input(struct tb_event ev) {
	struct gmi_tab* tab = &client.tabs[client.tab];
	struct gmi_page* page = &tab->page;
	if (page->code == 11 || page->code == 10) {
		if (!client.input.mode) client.input.cursor = 0;
		client.input.mode = 1;
	}
	if (ev.type == TB_EVENT_RESIZE) {
		int lines = gmi_render(tab);
		tab->scroll -= page->lines - lines;
		if (tab->scroll+tb_height()-2 > lines) tab->scroll = lines - tb_height() + 2;
		tb_clear();
		return 0;
	}
	if (ev.type != TB_EVENT_KEY) return 0;

	if (!client.input.mode && ev.key == TB_KEY_ESC) {
		bzero(client.vim.counter, sizeof(client.vim.counter));
		client.vim.g = 0;
	}
	if (client.input.mode && ev.key == TB_KEY_ESC) {
		client.input.mode = 0;
		client.input.cursor = 0;
		if (page->code == 11 || page->code == 10)
			page->code = 20;
		client.input.field[0] = '\0';
		tb_hide_cursor();
		return 0;
	}
	if (client.input.mode && (ev.key == TB_KEY_BACKSPACE2 || ev.key == TB_KEY_BACKSPACE)) {
		int i = client.input.cursor;
		if (i>((page->code==10||page->code==11)?0:1)) {
			strlcpy(&client.input.field[i-1],
				&client.input.field[i], sizeof(client.input.field)-i);
			client.input.cursor--;
		}
		return 0;
	}
	if (ev.key == TB_KEY_DELETE) {
		if (strcmp(tab->url, "about:home")) return 0;
		if (tab->selected && tab->selected > 0 && tab->selected <= page->links_count) {
			gmi_removebookmark(tab->selected);
			tab->selected = 0;
			gmi_freetab(tab);
			gmi_gohome(tab);
		}
		return 0;
	}
	if (ev.key == TB_KEY_BACK_TAB) {
		if (tab->selected && tab->selected > 0 && tab->selected <= page->links_count) {
			int linkid = tab->selected;
			tab->selected = 0;
			gmi_goto_new(tab, linkid);
		}
		return 0;
	}
	if (!client.input.mode && ev.key == TB_KEY_TAB) {
		if (client.vim.counter[0] != '\0' && atoi(client.vim.counter)) {
			tab->selected = atoi(client.vim.counter);
			bzero(client.vim.counter, sizeof(client.vim.counter));
			if (tab->selected > page->links_count) {
				snprintf(tab->error, sizeof(tab->error),
					 "Invalid link number");
				tab->selected = 0;
				client.input.error = 1;
			}
			else if (strlcpy(tab->selected_url, page->links[tab->selected - 1],
			sizeof(tab->selected_url)) >= sizeof(tab->selected_url)) {
				snprintf(tab->error, sizeof(tab->error),
					 "Invalid link, above %lu characters",
					 sizeof(tab->selected_url));
				tab->selected = 0;
				client.input.error = 1;
			}
		}
		else if (tab->selected) {
			gmi_goto(tab, tab->selected);
			tab->selected = 0;
		}
		client.vim.g = 0;
		return 0;
	}
	if (!client.input.mode && ev.key == TB_KEY_ENTER) {
		if (client.vim.counter[0] != '\0' && atoi(client.vim.counter)) {
			tab->scroll += atoi(client.vim.counter);
			bzero(client.vim.counter, sizeof(client.vim.counter));
			if (tab->scroll+tb_height()-2>page->lines)
				tab->scroll = page->lines - tb_height() + 2;
		}
		else if (tab->scroll+tb_height()-2<page->lines) tab->scroll++;
		client.vim.g = 0;
		return 0;
	}
	if (client.input.mode && ev.key == TB_KEY_ENTER) {
		client.input.mode = 0;
		tb_hide_cursor();
		if (page->code == 10 || page->code == 11) {
			char urlbuf[MAX_URL];
			char* start = strstr(tab->url, "gemini://");
			char* request = strrchr(tab->url, '?');
			if (!(start?strchr(&start[GMI], '/'):strchr(tab->url, '/')))
				snprintf(urlbuf, sizeof(urlbuf),
					 "%s/?%s", tab->url, client.input.field);
			else if (request && request > strrchr(tab->url, '/')) {
				*request = '\0';
				snprintf(urlbuf, sizeof(urlbuf),
					 "%s?%s", tab->url, client.input.field);
				*request = '?';
			} else
				snprintf(urlbuf, sizeof(urlbuf),
					 "%s?%s", tab->url, client.input.field);
			int bytes = gmi_request(tab, urlbuf);
			if (bytes>0) {
				tab = &client.tabs[client.tab];
				tab->scroll = -1;
			}
			client.input.field[0] = '\0';
			return 0;
		}
		return command();	
	}
	if (client.input.mode && ev.key == TB_KEY_ARROW_LEFT && ev.mod == TB_MOD_CTRL) {
		while (client.input.cursor>1) {
			client.input.cursor--;
			if (client.input.field[client.input.cursor] == ' ' ||
			    client.input.field[client.input.cursor] == '.')
				break;
		}
		return 0;
	}
	if (client.input.mode && ev.key == TB_KEY_ARROW_LEFT) {
		if (client.input.cursor>1)
			client.input.cursor--;
		return 0;
	}
	if (client.input.mode && ev.key == TB_KEY_ARROW_RIGHT && ev.mod == TB_MOD_CTRL) {
		while (client.input.field[client.input.cursor]) {
			client.input.cursor++;
			if (client.input.field[client.input.cursor] == ' ' ||
			    client.input.field[client.input.cursor] == '.')
				break;
		}
		return 0;
	}
	if (client.input.mode && ev.key == TB_KEY_ARROW_RIGHT) {
		if (client.input.field[client.input.cursor])
			client.input.cursor++;
		return 0;
	}
	unsigned int l = strnlen(client.input.field, sizeof(client.input.field));
	if (client.input.mode && ev.ch && l < sizeof(client.input.field)) {
		for (int i = l-1; i >= client.input.cursor; i--) {
			client.input.field[i+1] = client.input.field[i];
		}
		client.input.field[client.input.cursor] = ev.ch;
		client.input.cursor++;
		l++;
		client.input.field[l] = '\0';
		return 0;
	} else
		tb_hide_cursor();

	if (ev.key == TB_KEY_PGUP) {
		int counter = atoi(client.vim.counter);
		if (!counter) counter++;
		int H = tb_height() - 2 - (client.tabs_count>1);
		tab->scroll -= counter * H;
		bzero(client.vim.counter, sizeof(client.vim.counter));
		if (tab->scroll < -1) tab->scroll = -1;
		client.vim.g = 0;
		return 0;
	}
	if (ev.key == TB_KEY_PGDN) {
		int counter = atoi(client.vim.counter);
		if (!counter) counter++;
		int H = tb_height() - 2 - (client.tabs_count>1);
		tab->scroll += counter * H;
		bzero(client.vim.counter, sizeof(client.vim.counter));
		if (page->lines <= H) tab->scroll = -1;
		else if (tab->scroll + H >page->lines)
			tab->scroll = page->lines - H;
		client.vim.g = 0;
		return 0;
	}
	switch (ev.ch) {
	case 'u':
		client.input.mode = 1;
		snprintf(client.input.field, sizeof(client.input.field), ":o %s", tab->url);
		client.input.cursor = strnlen(client.input.field, sizeof(client.input.field));
		break;
	case ':':
		client.input.error = 0;
		client.input.mode = 1;
		client.input.cursor = 1;
		client.input.field[0] = ':';
		client.input.field[1] = '\0';
		break;
	case 'r': // Reload
		if (!strcmp("about:home", tab->url)) {
			gmi_freetab(tab);
			gmi_gohome(tab);
			break;
		}
		if (!tab->history) break;
		gmi_request(tab, tab->history->url);
		struct gmi_link* prev = tab->history->prev;
		prev->next = NULL;
		free(tab->history);
		tab->history = prev;
		break;
	case 'h': // Tab left
		client.tab--;
		if (client.tab < 0) client.tab = client.tabs_count - 1;
		break;
	case 'l': // Tab right
		client.tab++;
		if (client.tab >= client.tabs_count) client.tab = 0;
		break;
	case 'H': // Back
		if (!tab->history) break;
		if (page->code == 20 || page->code == 10 || page->code == 11) {
			if (!tab->history->prev) break;
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
			strlcpy(tab->url, tab->history->url, sizeof(tab->url));
		} else if (gmi_request(tab, tab->history->url) < 0) break;
		break;
	case 'L': // Forward
		if (!tab->history || !tab->history->next) break;
		if (!tab->history->cached) {
			tab->history->page = tab->page;
			tab->history->cached = 1;
		}
		if (tab->history->next->cached)
			tab->page = tab->history->next->page;
		else if (gmi_request(tab, tab->history->next->url) < 0) break;
		tab->history->scroll = tab->scroll;
		tab->history = tab->history->next;
		tab->scroll = tab->history->scroll;
		break;
	case 'k': // UP
		if (client.vim.counter[0] != '\0' && atoi(client.vim.counter)) {
			tab->scroll -= atoi(client.vim.counter);
			bzero(client.vim.counter, sizeof(client.vim.counter));
			if (tab->scroll < -1) tab->scroll = -1;
		}
		else if (tab->scroll>-1) tab->scroll--;
		client.vim.g = 0;
		break;
	case 'j': // DOWN
		if (client.vim.counter[0] != '\0' && atoi(client.vim.counter)) {
			tab->scroll += atoi(client.vim.counter);
			bzero(client.vim.counter, sizeof(client.vim.counter));
			if (tab->scroll+tb_height()-2>page->lines)
				tab->scroll = page->lines - tb_height() + 2;
		}
		else if (tab->scroll+(client.tabs_count==1?tb_height()-2:tb_height()-3)
			 < page->lines) tab->scroll++;
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
		if (client.tabs_count != 1) tab->scroll++;
		if (tb_height()-2-(client.tabs_count>1) > page->lines) tab->scroll = -1;
		break;
	default:
		if (!(ev.ch >= '0' && ev.ch <= '9'))
			break;
		unsigned int len = strnlen(client.vim.counter, sizeof(client.vim.counter));
		if (len == 0 && ev.ch == '0') break;
		if (len >= sizeof(client.vim.counter)) break;
		client.vim.counter[len] = ev.ch;
		client.vim.counter[len+1] = '\0';
	}
	return 0;
}
