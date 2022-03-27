/* See LICENSE file for copyright and license details. */
#include "gemini.h"
#include <stdio.h>
#include <string.h>
#define TB_IMPL
#include <termbox.h>

int input_mode = 0;
int input_cursor = 0;
int input_error = 0;
void tb_colorline(int x, int y, uintattr_t color) {
	for (int i=x; i<tb_width(); i++)
		tb_set_cell(i, y, ' ', color, color);
}

int main(int argc, char* argv[]) {
	if (gmi_init()) return 0;
	int r = 0;
	if (argc < 2)
		r = gmi_request("gemini://gemini.rmf-dev.com");
	else if (argv[1][0] == '/' || argv[1][0] == '.') {
		FILE* f = fopen(argv[1], "rb");
		if (!f) {
			printf("Failed to open %s\n", argv[1]);
			return -1;
		}
		fseek(f, 0, SEEK_END);
		r = ftell(f);
		fseek(f, 0, SEEK_SET);
		gmi_data = malloc(r);
		if (r != fread(gmi_data, 1, r, f)) {
			fclose(f);
			free(gmi_data);
			return -1;
		}
		fclose(f);
		gmi_code = 20;
		snprintf(gmi_url, sizeof(gmi_url), "file://%s/", argv[1]);
	} else
		r = gmi_request(argv[1]);
	if (r<0) {
		printf("error %s\n", gmi_error);
		return -1;
	}

	gmi_load(gmi_data, r);
	tb_init();
	struct tb_event ev;
	char link_url[1024];
	bzero(link_url, sizeof(link_url));
	int link_id = 0;
	char input[1024];
	bzero(input, sizeof(input));
	int ret = 0;
	int posy = -1;
	// vim
	int vim_g = 0;
	char vim_counter[8];
	bzero(vim_counter, sizeof(vim_counter));

	goto display;
	while ((ret = tb_poll_event(&ev)) == TB_OK || ret == -14) {
		if (gmi_code == 11 || gmi_code == 10) {
			if (!input_mode) input_cursor = 0;
			input_mode = 1;
		}
		if (ev.type == TB_EVENT_RESIZE) goto display;
		if (ev.type != TB_EVENT_KEY) continue;

		if (!input_mode && ev.key == TB_KEY_ESC) {
			bzero(vim_counter, sizeof(vim_counter));
			vim_g = 0;
		}
		if (input_mode && ev.key == TB_KEY_ESC) {
			input_mode = 0;
			input_cursor = 0;
			if (gmi_code == 11 || gmi_code == 10) {
				gmi_code = 0;
				if (gmi_history->prev) {
					gmi_history = gmi_history->prev;
					r = gmi_request(gmi_history->url);
					if (r < 0) gmi_load(gmi_data, r);
				}
			}
			input[0] = '\0';
		}
		if (input_mode && (ev.key == TB_KEY_BACKSPACE2) || (ev.key == TB_KEY_BACKSPACE)) {
			int i = input_cursor;
			if (i>((gmi_code==10||gmi_code==11)?0:1)) {
				strncpy(&input[i-1], &input[i], sizeof(input)-i);
				input_cursor--;
			}
			goto display;
		}
		if (!input_mode && ev.key == TB_KEY_TAB) {
			if (vim_counter[0] != '\0' && atoi(vim_counter)) {
				link_id = atoi(vim_counter);
				bzero(vim_counter, sizeof(vim_counter));
				if (link_id > gmi_links_count) {
					snprintf(gmi_error, sizeof(gmi_error), "Invalid link number");
					input_error = 1;
				}
				else strncpy(link_url, gmi_links[link_id - 1], sizeof(link_url));
			}
			else if (link_id) {
				gmi_goto(link_id);
				link_id = 0;
			}
			vim_g = 0;
			goto display;
		}
		if (!input_mode && ev.key == TB_KEY_ENTER) {
			if (vim_counter[0] != '\0' && atoi(vim_counter)) {
				posy += atoi(vim_counter);
				bzero(vim_counter, sizeof(vim_counter));
				if (posy+tb_height()-2>gmi_lines) posy = gmi_lines - tb_height() + 2;
			}
			else if (posy+tb_height()-2<gmi_lines) posy++;
			vim_g = 0;
			goto display;
		}
		if (input_mode && ev.key == TB_KEY_ENTER) {
			input_mode = 0;
			tb_hide_cursor();
			if (gmi_code == 10 || gmi_code == 11) {
				// --------------------------------
				char urlbuf[MAX_URL];
				char* start = strstr(gmi_url, "gemini://");
				if (!(start?strchr(&start[GMI], '/'):strchr(gmi_url, '/')))
					snprintf(urlbuf, sizeof(urlbuf), "%s/?%s", gmi_url, input);
				else
					snprintf(urlbuf, sizeof(urlbuf), "%s?%s", gmi_url, input);
				int bytes = gmi_request(urlbuf);
				if (bytes < 1) input_error = 1;
				else {
					r = bytes;
					posy = -1;
					gmi_load(gmi_data, bytes);
				}
				input[0] = '\0';
				// --------------------------------
				goto display;
			}
			if (input[1] == 'q' && input[2] == '\0') break;
			if (input[1] == 'o' && input[2] == ' ') {
				char urlbuf[MAX_URL];
				strncpy(urlbuf, &input[3], sizeof(urlbuf));
				input[0] = '\0';
				gmi_cleanforward();
				int bytes = gmi_request(urlbuf);
				if (bytes < 1) {
					input_error = 1;
					input_mode = 0;
					strncpy(gmi_url, gmi_history->url, sizeof(gmi_url));
				}
				else {
					r = bytes;
					posy = -1;
					gmi_load(gmi_data, bytes);
				}
				if (gmi_code == 11 || gmi_code == 10) {
					input_mode = 1;
				}
				link_id = 0;
				goto display;
			}
			else if (input[1] == '\0') input[0] = '\0';
			else if (atoi(&input[1]) || (input[1] == '0' && input[2] == '\0')) {
				int bytes = gmi_goto(atoi(&input[1]));
				if (bytes < 1) input_error = 1;
				else {
					r = bytes;
					posy = -1;
				}
				input[0] = '\0';
				link_id = 0;
			}
			else {
				input_error = -1;
				snprintf(gmi_error, sizeof(gmi_error), "Unknown input: %s", &input[1]);
			}
		}
		if (input_mode && ev.key == TB_KEY_ARROW_LEFT) {
			if (input_cursor>1)
				input_cursor--;	
			goto display;
		}
		if (input_mode && ev.key == TB_KEY_ARROW_RIGHT) {
			if (input[input_cursor])
				input_cursor++;	
			goto display;
		}
		int l = strnlen(input, sizeof(input));
		if (input_mode && ev.ch && l < sizeof(input)) {
			char c1, c2;
                        c1 = input[l];
                        for (int i = l-1; i > input_cursor; i--) {
                                input[i+1] = input[i];
                        }
                        input[input_cursor] = ev.ch;
                        input_cursor++;
                        l++;
                        input[l] = '\0';
			goto display;
		} else
			tb_hide_cursor();

		if (ev.key == TB_KEY_PGUP) {
			int counter = atoi(vim_counter);
			if (!counter) counter++;
			posy -= counter * tb_height();
			bzero(vim_counter, sizeof(vim_counter));
			if (posy < -1) posy = -1;
			vim_g = 0;
			goto display;
		}
		if (ev.key == TB_KEY_PGDN) {
			int counter = atoi(vim_counter);
			if (!counter) counter++;
			posy += counter * tb_height();
			bzero(vim_counter, sizeof(vim_counter));
			if (posy+tb_height()-2>gmi_lines) posy = gmi_lines - tb_height() + 2;
			vim_g = 0;
			goto display;
		}
		switch (ev.ch) {
		case 'u':
			input_mode = 1;
			snprintf(input, sizeof(input), ":o %s", gmi_url);
			input_cursor = strnlen(input, sizeof(input));
			break;
		case ':':
			input_mode = 1;
			input_cursor = 1;
			input[0] = ':';
			input[1] = '\0';
			break;
		case 'r': // Reload
			r = gmi_request(gmi_history->url);
			gmi_load(gmi_data, r);
			struct gmi_link* prev = gmi_history->prev;
			prev->next = NULL;
			free(gmi_history);
			gmi_history = prev;
			break;
		case 'h': // Back
			if (gmi_code == 20 || gmi_code == 10 || gmi_code == 11) {
				if (!gmi_history->prev) break;
				gmi_history = gmi_history->prev;
			} else 
				if (!gmi_history) break;
			r = gmi_request(gmi_history->url);
			if (r < 0) break;
			gmi_load(gmi_data, r);
			break;
		case 'l': // Forward
			if (!gmi_history->next) break;
			r = gmi_request(gmi_history->next->url);
			if (r < 0) break;
			gmi_history = gmi_history->next;
			gmi_load(gmi_data, r);
			break;
		case 'k': // UP
			if (vim_counter[0] != '\0' && atoi(vim_counter)) {
				posy -= atoi(vim_counter);
				bzero(vim_counter, sizeof(vim_counter));
				if (posy < -1) posy = -1;
			}
			else if (posy>-1) posy--;
			vim_g = 0;
			break;
		case 'j': // DOWN
			if (vim_counter[0] != '\0' && atoi(vim_counter)) {
				posy += atoi(vim_counter);
				bzero(vim_counter, sizeof(vim_counter));
				if (posy+tb_height()-2>gmi_lines) posy = gmi_lines - tb_height() + 2;
			}
			else if (posy+tb_height()-2<gmi_lines) posy++;
			vim_g = 0;
			break;
		case 'g': // Start of file
			if (vim_g) {
				posy = -1;
				vim_g = 0;
			} else vim_g++;
			break;
		case 'G': // End of file
			posy = gmi_lines-tb_height()+2;
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
		tb_clear();
		if (input_mode) {
			if (gmi_code == 11 || gmi_code == 10)
				tb_set_cursor(input_cursor+strnlen(gmi_input, sizeof(gmi_input))+2, tb_height()-1);
			else
				tb_set_cursor(input_cursor, tb_height()-1);
		}

		if (gmi_code == 20 || gmi_code == 10 || gmi_code == 11) {
			if (ev.type == TB_EVENT_RESIZE) {
				int lines = gmi_render(gmi_data, r, posy);
				posy -= gmi_lines - lines;
				tb_clear();
			}
			tb_clear();
			gmi_lines = gmi_render(gmi_data, r, posy);
		} else if (gmi_error[0] != '\0') {
			tb_printf(2, 1, TB_RED, TB_DEFAULT, "# %s", gmi_error);
		}

		// current url
		tb_colorline(0, tb_height()-2, TB_WHITE);
		char urlbuf[MAX_URL];
		int hide = 0;
		int posx = 0;
		for (int i=0; gmi_url[i]; i++) {
			if (hide && (gmi_url[i] == '/')) {
				hide = 0;
			}
			if (!hide) {
				urlbuf[posx] = gmi_url[i];
				posx++;
			}
			if (gmi_url[i] == '?') {
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
		if (link_id != 0) {
			int llen = strnlen(link_url, sizeof(link_url));
			tb_printf(tb_width()-llen-1, tb_height()-2, TB_BLACK, TB_WHITE, "%s", link_url);
		}

		// input
		if (input_error) {
			tb_hide_cursor();
			tb_colorline(0, tb_height()-1, TB_RED);
			tb_printf(0, tb_height()-1, TB_WHITE, TB_RED, gmi_error);
			input[0] = '\0';
			input_error = 0;
		} else if (gmi_code == 10) {
			tb_printf(0, tb_height()-1, TB_DEFAULT, TB_DEFAULT, "%s: %s", gmi_input, input);
		} else if (gmi_code == 11) {
			char input_buf[1024];
			int i = 0;
			for (; input[i] && i < sizeof(input); i++) input_buf[i] = '*';
			input_buf[i] = '\0';
			tb_printf(0, tb_height()-1, TB_DEFAULT, TB_DEFAULT, "%s: %s", gmi_input, input_buf);
		}
		else tb_printf(0, tb_height()-1, TB_DEFAULT, TB_DEFAULT, "%s", input);

		tb_present();
	}
	tb_shutdown();
	return 0;
}
