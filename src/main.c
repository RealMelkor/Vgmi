/* See LICENSE file for copyright and license details. */
#include "gemini.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#define TB_IMPL
#include <termbox.h>

void tb_colorline(int x, int y, uintattr_t color) {
	for (int i=x; i<tb_width(); i++)
		tb_set_cell(i, y, ' ', color, color);
}

void show_history() {
	int ret = 0;
	struct tb_event ev;
	bzero(&ev, sizeof(ev));
	do {
		struct gmi_tab* tab = &client.tabs[client.tab];
		if (ev.key == TB_KEY_ESC) break;

		tb_clear();

		int y = 0;
		for (struct gmi_link* link = tab->history->next; link; link = link->next) {
			tb_printf(0, y, TB_DEFAULT, TB_RED, "%s", link->url); 
			y++;
		}

		tb_printf(0, y, TB_DEFAULT, TB_DEFAULT, "-> %s", tab->history->url); 
		y++;

		for (struct gmi_link* link = tab->history->prev; link; link = link->prev) {
			tb_printf(0, y, TB_DEFAULT, TB_BLUE, "%s", link->url); 
			y++;
		}

		tb_present();

	} while(0); //while ((ret = tb_poll_event(&ev)) == TB_OK || ret == -14);
	ret = tb_poll_event(&ev);
}

int main(int argc, char* argv[]) {
	if (gmi_init()) return 0;
	gmi_newtab();
	if (argc < 2)
		//gmi_request("gemini://gemini.rmf-dev.com");
		gmi_request("file://./tests/output.txt");
	else if (argv[1][0] == '/' || argv[1][0] == '.') {
		struct gmi_tab* tab = &client.tabs[client.tab];
		FILE* f = fopen(argv[1], "rb");
		if (!f) {
			printf("Failed to open %s\n", argv[1]);
			return -1;
		}
		fseek(f, 0, SEEK_END);
		int len = ftell(f);
		fseek(f, 0, SEEK_SET);
		char* data = malloc(len);
		if (len != fread(data, 1, len, f)) {
			fclose(f);
			free(data);
			return -1;
		}
		fclose(f);
		tab->page.code = 20;
		tab->page.data_len = len;
		tab->page.data = data;
		snprintf(tab->url, sizeof(tab->url), "file://%s/", argv[1]);
	} else
		gmi_request(argv[1]);
	/*
	if (<0) {
		printf("error %s\n", gmi_error);
		return -1;
	}*/

	tb_init();
	struct tb_event ev;
	int ret = 0;
	client.tabs[client.tab].scroll = -1;
	// vim
	int vim_g = 0;
	char vim_counter[6];
	bzero(vim_counter, sizeof(vim_counter));

	struct gmi_tab* tab = &client.tabs[client.tab];
	struct gmi_page* page = &tab->page;
	goto display;
	while ((ret = tb_poll_event(&ev)) == TB_OK || ret == -14) {
		tab = &client.tabs[client.tab];
		page = &tab->page;
		if (page->code == 11 || page->code == 10) {
			if (!client.input.mode) client.input.cursor = 0;
			client.input.mode = 1;
		}
		if (ev.type == TB_EVENT_RESIZE) goto display;
		if (ev.type != TB_EVENT_KEY) continue;

		if (!client.input.mode && ev.key == TB_KEY_ESC) {
			bzero(vim_counter, sizeof(vim_counter));
			vim_g = 0;
		}
		if (client.input.mode && ev.key == TB_KEY_ESC) {
			client.input.mode = 0;
			client.input.cursor = 0;
			if (page->code == 11 || page->code == 10) {
				page->code = 0;
				if (tab->history->prev) {
					tab->history = tab->history->prev;
					gmi_request(tab->history->url);
				}
			}
			client.input.field[0] = '\0';
		}
		if (client.input.mode && (ev.key == TB_KEY_BACKSPACE2) || (ev.key == TB_KEY_BACKSPACE)) {
			int i = client.input.cursor;
			if (i>((page->code==10||page->code==11)?0:1)) {
				strncpy(&client.input.field[i-1], &client.input.field[i], sizeof(client.input.field)-i);
				client.input.cursor--;
			}
			goto display;
		}
		if (ev.key == TB_KEY_BACK_TAB) {
			if (tab->selected && tab->selected > 0 && tab->selected < page->links_count) {
				char linkbuf[1024];
				char urlbuf[1024];
				strncpy(linkbuf, page->links[tab->selected - 1], sizeof(linkbuf));
				strncpy(urlbuf, tab->url, sizeof(urlbuf));
				gmi_newtab();
				client.tab++;
				tab = &client.tabs[client.tab];
				page = &client.tabs[client.tab].page;
				strncpy(tab->url, urlbuf, sizeof(tab->url));
				strcpy(tab->url, "helloworld");
				gmi_request(linkbuf);
				tab->selected = 0;
			}
			goto display;
		}
		if (!client.input.mode && ev.key == TB_KEY_TAB) {
			if (vim_counter[0] != '\0' && atoi(vim_counter)) {
				tab->selected = atoi(vim_counter);
				bzero(vim_counter, sizeof(vim_counter));
				if (tab->selected > page->links_count) {
					snprintf(client.error, sizeof(client.error), "Invalid link number");
					tab->selected = 0;
					client.input.error = 1;
				}
				else strncpy(tab->selected_url, page->links[tab->selected - 1], sizeof(tab->selected_url));
			}
			else if (tab->selected) {
				gmi_goto(tab->selected);
				tab->selected = 0;
			}
			vim_g = 0;
			goto display;
		}
		if (!client.input.mode && ev.key == TB_KEY_ENTER) {
			if (vim_counter[0] != '\0' && atoi(vim_counter)) {
				tab->scroll += atoi(vim_counter);
				bzero(vim_counter, sizeof(vim_counter));
				if (tab->scroll+tb_height()-2>page->lines) tab->scroll = page->lines - tb_height() + 2;
			}
			else if (tab->scroll+tb_height()-2<page->lines) tab->scroll++;
			vim_g = 0;
			goto display;
		}
		if (client.input.mode && ev.key == TB_KEY_ENTER) {
			client.input.mode = 0;
			tb_hide_cursor();
			if (page->code == 10 || page->code == 11) {
				char urlbuf[MAX_URL];
				char* start = strstr(tab->url, "gemini://");
				if (!(start?strchr(&start[GMI], '/'):strchr(tab->url, '/')))
					snprintf(urlbuf, sizeof(urlbuf), "%s/?%s", tab->url, client.input.field);
				else
					snprintf(urlbuf, sizeof(urlbuf), "%s?%s", tab->url, client.input.field);
				int bytes = gmi_request(urlbuf);
				if (bytes>0) {
					tab = &client.tabs[client.tab];
					page = &tab->page;
					tab->scroll = -1;
				}
				client.input.field[0] = '\0';
				goto display;
			}
			if (client.input.field[1] == 'q' && client.input.field[2] == '\0') break;
			if (client.input.field[1] == 'o' && client.input.field[2] == ' ') {
				char urlbuf[MAX_URL];
				strncpy(urlbuf, &client.input.field[3], sizeof(urlbuf));
				client.input.field[0] = '\0';
				gmi_cleanforward(tab);
					//client.input.mode = 0;
				int bytes = gmi_request(urlbuf);
				if (bytes > 0) {
					tab->scroll = -1;
				}
				if (page->code == 11 || page->code == 10) {
					client.input.mode = 1;
				}
				tab->selected = 0;
				goto display;
			}
			else if (client.input.field[1] == '\0') client.input.field[0] = '\0';
			else if (atoi(&client.input.field[1]) || (client.input.field[1] == '0' && client.input.field[2] == '\0')) {
				int bytes = gmi_goto(atoi(&client.input.field[1]));
				if (bytes > 0) {
					tab->scroll = -1;
				}
				client.input.field[0] = '\0';
				tab->selected = 0;
			}
			else {
				client.input.error = -1;
				snprintf(client.error, sizeof(client.error), "Unknown input: %s", &client.input.field[1]);
			}
		}
		if (client.input.mode && ev.key == TB_KEY_ARROW_LEFT) {
			if (client.input.cursor>1)
				client.input.cursor--;	
			goto display;
		}
		if (client.input.mode && ev.key == TB_KEY_ARROW_RIGHT) {
			if (client.input.field[client.input.cursor])
				client.input.cursor++;	
			goto display;
		}
		int l = strnlen(client.input.field, sizeof(client.input.field));
		if (client.input.mode && ev.ch && l < sizeof(client.input.field)) {
			char c1, c2;
                        c1 = client.input.field[l];
                        for (int i = l-1; i > client.input.cursor; i--) {
                                client.input.field[i+1] = client.input.field[i];
                        }
                        client.input.field[client.input.cursor] = ev.ch;
                        client.input.cursor++;
                        l++;
                        client.input.field[l] = '\0';
			goto display;
		} else
			tb_hide_cursor();

		if (ev.key == TB_KEY_PGUP) {
			int counter = atoi(vim_counter);
			if (!counter) counter++;
			tab->scroll -= counter * tb_height();
			bzero(vim_counter, sizeof(vim_counter));
			if (tab->scroll < -1) tab->scroll = -1;
			vim_g = 0;
			goto display;
		}
		if (ev.key == TB_KEY_PGDN) {
			int counter = atoi(vim_counter);
			if (!counter) counter++;
			tab->scroll += counter * tb_height();
			bzero(vim_counter, sizeof(vim_counter));
			if (tab->scroll+tb_height()-2>page->lines) tab->scroll = page->lines - tb_height() + 2;
			vim_g = 0;
			goto display;
		}
		switch (ev.ch) {
		case 'u':
			client.input.mode = 1;
			snprintf(client.input.field, sizeof(client.input.field), ":o %s", tab->url);
			client.input.cursor = strnlen(client.input.field, sizeof(client.input.field));
			break;
		case ':':
			client.input.mode = 1;
			client.input.cursor = 1;
			client.input.field[0] = ':';
			client.input.field[1] = '\0';
			break;
		case 'r': // Reload
			gmi_request(tab->history->url);
			struct gmi_link* prev = tab->history->prev;
			prev->next = NULL;
			free(tab->history);
			tab->history = prev;
			break;
		case 'h': // Back
			if (page->code == 20 || page->code == 10 || page->code == 11) {
				if (!tab->history->prev) break;
				tab->history = tab->history->prev;
			} else 
				if (!tab->history) break;
			if (gmi_request(tab->history->url) < 0) break;
			break;
		case 'l': // Forward
			if (!tab->history->next) break;
			if (gmi_request(tab->history->next->url) < 0) break;
			tab->history = tab->history->next;
			break;
		case 'k': // UP
			if (vim_counter[0] != '\0' && atoi(vim_counter)) {
				tab->scroll -= atoi(vim_counter);
				bzero(vim_counter, sizeof(vim_counter));
				if (tab->scroll < -1) tab->scroll = -1;
			}
			else if (tab->scroll>-1) tab->scroll--;
			vim_g = 0;
			break;
		case 'j': // DOWN
			if (vim_counter[0] != '\0' && atoi(vim_counter)) {
				tab->scroll += atoi(vim_counter);
				bzero(vim_counter, sizeof(vim_counter));
				if (tab->scroll+tb_height()-2>page->lines) tab->scroll = page->lines - tb_height() + 2;
			}
			else if (tab->scroll+tb_height()-2<page->lines) tab->scroll++;
			vim_g = 0;
			break;
		case 'H': // Show history
			show_history();
			break;
		case 'g': // Start of file
			if (vim_g) {
				tab->scroll = -1;
				vim_g = 0;
			} else vim_g++;
			break;
		case 'G': // End of file
			tab->scroll = page->lines-tb_height()+2;
			break;
		default:
			if (!(ev.ch >= '0' && ev.ch <= '9'))
				break;
			int len = strnlen(vim_counter, sizeof(vim_counter));
			if (len == 0 && ev.ch == '0') break;
			if (len >= sizeof(vim_counter)) break;
			vim_counter[len] = ev.ch;
			vim_counter[len+1] = '\0';

		}
display:
		tab = &client.tabs[client.tab];
		page = &tab->page;
		tb_clear();
		if (client.input.mode) {
			if (page->code == 11 || page->code == 10)
				tb_set_cursor(client.input.cursor+strnlen(client.input.label, sizeof(client.input.label))+2, tb_height()-1);
			else
				tb_set_cursor(client.input.cursor, tb_height()-1);
		}

		if (page->code == 20 || page->code == 10 || page->code == 11) {
			if (ev.type == TB_EVENT_RESIZE) {
				int lines = gmi_render(tab);
				tab->scroll -= page->lines - lines;
				tb_clear();
			}
			tb_clear();
			page->lines = gmi_render(tab);
		} else if (client.error[0] != '\0') {
			tb_printf(2, 1, TB_RED, TB_DEFAULT, "# %s", client.error);
		}

		// current url
		tb_colorline(0, tb_height()-2, TB_WHITE);
		char urlbuf[MAX_URL];
		int hide = 0;
		int posx = 0;
		for (int i=0; tab->url[i]; i++) {
			if (hide && (tab->url[i] == '/')) {
				hide = 0;
			}
			if (!hide) {
				urlbuf[posx] = tab->url[i];
				posx++;
			}
			if (tab->url[i] == '?') {
				hide = 1;
				urlbuf[posx] = '<';
				urlbuf[posx+1] = '*';
				urlbuf[posx+2] = '>';
				posx+=3;
			}
		}
		urlbuf[posx] = '\0';
		tb_printf(0, tb_height()-2, TB_BLACK, TB_WHITE, "%s", urlbuf);

		// Show selected link url
		if (tab->selected != 0) {
			int llen = strnlen(tab->selected_url, sizeof(tab->selected_url));
			tb_printf(tb_width()-llen-5, tb_height()-2, TB_WHITE, TB_BLUE, " => %s ", tab->selected_url);
		}

		int count = atoi(vim_counter);
		if (count) {
			tb_printf(tb_width() - 8, tb_height() - 1, TB_DEFAULT, TB_DEFAULT, "%d", count);
		}

		// input
		if (client.input.error) {
			tb_hide_cursor();
			tb_colorline(0, tb_height()-1, TB_RED);
			tb_printf(0, tb_height()-1, TB_WHITE, TB_RED, client.error);
			client.input.field[0] = '\0';
			client.input.error = 0;
		} else if (page->code == 10) {
			tb_printf(0, tb_height()-1, TB_DEFAULT, TB_DEFAULT, "%s: %s", client.input.label, client.input.field);
		} else if (page->code == 11) {
			char input_buf[1024];
			int i = 0;
			for (; client.input.field[i] && i < sizeof(client.input.field); i++) input_buf[i] = '*';
			input_buf[i] = '\0';
			tb_printf(0, tb_height()-1, TB_DEFAULT, TB_DEFAULT, "%s: %s", client.input.label, input_buf);
		}
		else tb_printf(0, tb_height()-1, TB_DEFAULT, TB_DEFAULT, "%s", client.input.field);

		tb_present();
	}
	tb_shutdown();
	return 0;
}
