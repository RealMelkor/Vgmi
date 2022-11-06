/* See LICENSE file for copyright and license details. */
#include <strings.h>
#include <termbox.h>
#include "gemini.h"
#include "display.h"
#include "wcwidth.h"
#include "url.h"

void tb_colorline(int x, int y, uintattr_t color) {
        for (int i=x; i<tb_width(); i++)
                tb_set_cell(i, y, ' ', color, color);
}

void hide_query(char* url, char* urlbuf) {
	int hide = 0;
        int posx = 0;
        for (int i=0; url[i] && i < 1024; i++) {
                if (hide && (url[i] == '/')) {
                        hide = 0;
                }
                if (!hide) {
                        urlbuf[posx] = url[i];
                        posx++;
                }
                if (!hide && url[i] == '?') {
                        hide = 1;
                        urlbuf[posx] = '<';
                        urlbuf[posx+1] = '*';
                        urlbuf[posx+2] = '>';
                        posx+=3;
                }
        }
        urlbuf[posx] = '\0';
}

void display() {
	int bg = GREY;
        struct gmi_tab* tab = client.tab;
        struct gmi_page* page = &tab->page;
	if (tab->request.ask == 2) {
		tab->request.ask = display_ask(tab->request.info,
					       tab->request.action);
	}
        tb_clear();
	
	int input_offset = 0;
        if (client.input.mode) {
		int cpos = utf8_width_to(client.input.field,
					 sizeof(client.input.field),
					 client.input.cursor);
		int w = tb_width();
		w -= w&1; // make width even so double-width
			  // characters don't cause problem
		if (page->code == 10 || page->code == 11)
			w -= utf8_width(client.input.label,
					sizeof(client.input.label)) + 2;
		if (cpos >= w) {
			input_offset = utf8_len_to(client.input.field,
						   sizeof(client.input.field),
						   cpos - cpos%w);
			cpos = cpos%w;
		}

                if (page->code == 11 || page->code == 10)
                        tb_set_cursor(cpos + utf8_width(client.input.label,
				      sizeof(client.input.label))+2,
				      tb_height()-1);
                else
                        tb_set_cursor(cpos, tb_height()-1);
        }

        if (page->code == 20 || page->code == 10 || page->code == 11) {
                tb_clear();
		if (client.tabs_count > 1) tab->scroll--;
                page->lines = gmi_render(tab);
		if (client.tabs_count > 1) tab->scroll++;
        } else if (tab->error[0] != '\0') {
                tb_printf(2, 1+(client.tabs_count>1), RED, TB_DEFAULT,
			  "# %s", tab->error);
        }

	// current tab
	if (client.tabs_count > 1) {
		tb_colorline(0, 0, bg);
		gmi_gettitle(page, tab->url);
		int index = 1;
		struct gmi_tab* ptr = tab;
		while (ptr->prev) {
			index++;
			ptr = ptr->prev;
		}
		tb_printf(0, 0, BLACK, bg,
			  " %s [%d/%d]", page->title,
			  index, client.tabs_count);
	}

        // current url
        tb_colorline(0, tb_height()-2, bg);
	char urlbuf[MAX_URL];
	hide_query(tab->url, urlbuf);
	tb_printf(0, tb_height()-2, BLACK, bg, "%s (%s)",
		  urlbuf, tab->page.meta);

        // Show selected link url
        if (tab->selected != 0) {
		int x = tb_width() -
			utf8_width(tab->selected_url, MAX_URL) - 5;
		x = x < 10?10:x;
                tb_printf(x, tb_height()-2, bg, BLUE,
			  " => %s ", tab->selected_url);
        } else if (tab->request.state != STATE_DONE) {
		hide_query(tab->request.url, urlbuf);
                int llen = strnlen(tab->request.url, sizeof(tab->request.url));
                tb_printf(tb_width()-llen-5, tb_height()-2, BLACK, bg,
			  " [%s] ", urlbuf);
	}

        int count = atoi(client.vim.counter);
        if (count) {
                tb_printf(tb_width() - 8, tb_height() - 1,
			  TB_DEFAULT, TB_DEFAULT, "%d", count);
        }

        // input
        if (tab->show_error) {
                tb_hide_cursor();
                tb_colorline(0, tb_height()-1, RED);
                tb_print(0, tb_height()-1, WHITE, RED, tab->error);
                client.input.field[0] = '\0';
	} else if (tab->show_info) {
                tb_hide_cursor();
                tb_colorline(0, tb_height()-1, GREEN);
                tb_print(0, tb_height()-1, WHITE, GREEN, tab->info);
                client.input.field[0] = '\0';
		tab->show_info = 0;
        } else if (page->code == 10) {
                tb_printf(0, tb_height()-1, TB_DEFAULT, TB_DEFAULT,
			 "%s: %s", client.input.label,
			 client.input.field + input_offset);
        } else if (page->code == 11) {
                char input_buf[1024];
                size_t i = 0;
                for (; client.input.field[i] &&
		       i < sizeof(client.input.field); i++)
			input_buf[i] = '*';
                input_buf[i] = '\0';
                tb_printf(0, tb_height()-1, TB_DEFAULT, TB_DEFAULT,
			  "%s: %s", client.input.label, input_buf);
        } else {
		tb_printf(0, tb_height()-1, TB_DEFAULT, TB_DEFAULT, "%s",
			  client.input.field + input_offset);
	}

	if (client.input.mode && tb_width() & 1)
		tb_set_cell(tb_width() - 1, tb_height() -1, ' ',
			    TB_DEFAULT, TB_DEFAULT);
	tb_present();
}

void display_history() {
	char urlbuf[1024];
        int ret = 0;
        struct tb_event ev;
        bzero(&ev, sizeof(ev));
        do {
                struct gmi_tab* tab = client.tab;

                tb_clear();
                tb_print(2, 1, RED, TB_DEFAULT, "# History");

                if (!tab->history) {
                        tb_present();
                        continue;
                }
                int y = 3;
                for (struct gmi_link* link = tab->history->next; link;
		     link = link->next) {
			hide_query(link->url, urlbuf);
                        tb_printf(4, y, TB_DEFAULT, TB_DEFAULT, "%s", urlbuf);
                        y++;
                }

		hide_query(tab->history->url, urlbuf);
                tb_printf(4, y, TB_DEFAULT, BLUE, "-> %s", urlbuf);
                y++;

                for (struct gmi_link* link = tab->history->prev; link;
		     link = link->prev) {
			hide_query(link->url, urlbuf);
                        tb_printf(4, y, TB_DEFAULT, TB_DEFAULT, "%s", urlbuf);
                        y++;
                }

                tb_present();

        } while(((ret = tb_poll_event(&ev)) == TB_OK &&
		ev.type == TB_EVENT_RESIZE)
		|| ret == -14);
}

int display_ask(char* info, char* action) {
        int ret = 0;
        struct tb_event ev;
        bzero(&ev, sizeof(ev));
        do {

                tb_clear();
                tb_printf(2, 1, RED, TB_DEFAULT, "%s", info);
		int w = tb_width();
		int h = tb_height();
		const char* line1 = "Press 'y' to %s";
		const char* line2 = "Press any other key to cancel";
		int x1 = w/2-(strlen(line1)+strnlen(action, w/2))/2;
		int x2	 = w/2-strlen(line2)/2;
                tb_printf(x1, h/2-1, TB_DEFAULT, TB_DEFAULT, line1, action);
                tb_printf(x1+7, h/2-1, GREEN, TB_DEFAULT, "y");
                tb_printf(x2, h/2, TB_DEFAULT, TB_DEFAULT, line2);

                tb_present();

        } while(((ret = tb_poll_event(&ev)) == TB_OK &&
		ev.type == TB_EVENT_RESIZE) || ret == -14);
	return ev.ch == 'y' || ev.ch == 'Y';
}
