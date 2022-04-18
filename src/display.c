/* See LICENSE file for copyright and license details. */
#include <termbox.h>
#include "gemini.h"

void tb_colorline(int x, int y, uintattr_t color) {
        for (int i=x; i<tb_width(); i++)
                tb_set_cell(i, y, ' ', color, color);
}

void display() {
        struct gmi_tab* tab = &client.tabs[client.tab];
        struct gmi_page* page = &tab->page;
        tb_clear();
        if (client.input.mode) {
                if (page->code == 11 || page->code == 10)
                        tb_set_cursor(client.input.cursor+strnlen(client.input.label,
				      sizeof(client.input.label))+2, tb_height()-1);
                else
                        tb_set_cursor(client.input.cursor, tb_height()-1);
        }

        if (page->code == 20 || page->code == 10 || page->code == 11) {
                tb_clear();
		if (client.tabs_count > 1) tab->scroll--;
                page->lines = gmi_render(tab);
		if (client.tabs_count > 1) tab->scroll++;
        } else if (client.error[0] != '\0') {
                tb_printf(2, 1, TB_RED, TB_DEFAULT, "# %s", client.error);
        }

	// current tab
	if (client.tabs_count > 1) {
		tb_colorline(0, 0, TB_WHITE);
		tb_printf(0, 0, TB_BLACK, TB_WHITE,
			  "Tabs : %d/%d", client.tab+1, client.tabs_count);
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
                if (!hide && tab->url[i] == '?') {
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
                tb_printf(tb_width()-llen-5, tb_height()-2, TB_WHITE, TB_BLUE,
			  " => %s ", tab->selected_url);
        }

        int count = atoi(client.vim.counter);
        if (count) {
                tb_printf(tb_width() - 8, tb_height() - 1, TB_DEFAULT, TB_DEFAULT,
			 "%d", count);
        }

        // input
        if (client.input.error) {
                tb_hide_cursor();
                tb_colorline(0, tb_height()-1, TB_RED);
                tb_print(0, tb_height()-1, TB_WHITE, TB_RED, client.error);
                client.input.field[0] = '\0';
                client.input.error = 0;
        } else if (page->code == 10) {
                tb_printf(0, tb_height()-1, TB_DEFAULT, TB_DEFAULT,
			 "%s: %s", client.input.label, client.input.field);
        } else if (page->code == 11) {
                char input_buf[1024];
                size_t i = 0;
                for (; client.input.field[i] && i < sizeof(client.input.field); i++)
			input_buf[i] = '*';
                input_buf[i] = '\0';
                tb_printf(0, tb_height()-1, TB_DEFAULT, TB_DEFAULT,
			  "%s: %s", client.input.label, input_buf);
        }
        else tb_printf(0, tb_height()-1, TB_DEFAULT, TB_DEFAULT, "%s", client.input.field);

        tb_present();
}

void display_history() {
        int ret = 0;
        struct tb_event ev;
        bzero(&ev, sizeof(ev));
        do {
                struct gmi_tab* tab = &client.tabs[client.tab];

                tb_clear();
                tb_print(2, 1, TB_RED, TB_DEFAULT, "# History");

                if (!tab->history) {
                        tb_present();
                        continue;
                }
                int y = 3;
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

        } while(((ret = tb_poll_event(&ev)) == TB_OK && ev.type == TB_EVENT_RESIZE) || ret == -14);
}

