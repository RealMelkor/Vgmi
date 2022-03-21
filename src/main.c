#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#define TB_IMPL
#include <termbox.h>
#include <stdarg.h>
#include <math.h>
#include "gemini.h"

int input_mode = 0;
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
	else
		r = gmi_request(argv[1]);
	if (r<1) {
		printf("error %s\n", gmi_error);
		return 0;
	}
	//printf("%s\n", gmi_data);
	gmi_load(gmi_data, r);
	tb_init();
	struct tb_event ev;
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
			if (gmi_code == 11 || gmi_code == 10)
				gmi_code = 0;
			input[0] = '\0';
		}
		if (input_mode && (ev.key == TB_KEY_BACKSPACE2) || (ev.key == TB_KEY_BACKSPACE)) {
			int i = strnlen(input, sizeof(input));
			if (i>((gmi_code==10||gmi_code==11)?0:1)) input[i-1] = '\0';
		}
		if (!input_mode && ev.key == TB_KEY_ENTER) {
			int counter = atoi(vim_counter);
			if (!counter) counter++;
			posy += counter;
			bzero(vim_counter, sizeof(vim_counter));
			if (posy+tb_height()-2>gmi_lines) posy = gmi_lines - tb_height() + 2;
			vim_g = 0;
			goto display;
		}
		if (input_mode && ev.key == TB_KEY_ENTER) {
			input_mode = 0;
			if (gmi_code == 10 || gmi_code == 11) {
				char urlbuf[MAX_URL];
				snprintf(urlbuf, sizeof(urlbuf), "%s/?%s", gmi_url, input);
				int bytes = gmi_request(urlbuf);
				if (bytes < 1) input_error = 1;
				else {
					r = bytes;
					posy = -1;
					gmi_load(gmi_data, bytes);
				}
				input[0] = '\0';
				goto display;
			}
			if (input[1] == 'q' && input[2] == '\0') break;
			if (input[1] == 'o' && input[2] == ' ') {
				char urlbuf[MAX_URL];
				strncpy(urlbuf, &input[3], sizeof(urlbuf));
				input[0] = '\0';
				int bytes = gmi_request(urlbuf);
				if (bytes < 1) {
					input_error = 1;
					input_mode = 0;
				}
				else {
					r = bytes;
					posy = -1;
					gmi_load(gmi_data, bytes);
				}
				if (gmi_code == 11 || gmi_code == 10) {
					input_mode = 1;
				}
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
			}
			else {
				input_error = -1;
				snprintf(gmi_error, sizeof(gmi_error), "Unknown input: %s", &input[1]);
			}
		}
		if (input_mode) {
			int i = strnlen(input, sizeof(input));
			input[i] = ev.ch;
			input[i+1] = '\0';
			goto display;
		}

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
		case ':':
			input_mode = 1;
			input[0] = ':';
			input[1] = '\0';
			break;
		case 'r': // Reload
			r = gmi_request(gmi_url);
			gmi_load(gmi_data, r);
			break;
		case 'b': // Back
			break;
		case 'e': // Forward
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
		case 'h': // LEFT
			break;
		case 'l': // RIGHT
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

		if (gmi_code == 20) {
			if (ev.type == TB_EVENT_RESIZE) {
				int lines = gmi_render(gmi_data, r, posy);
				posy -= gmi_lines - lines;
				tb_clear();
			}
			gmi_lines = gmi_render(gmi_data, r, posy);
		} else if (gmi_error[0] != '\0') {
			tb_printf(2, 1, TB_RED, TB_DEFAULT, "# %s", gmi_error);
		}
		//tb_print(0, 0, TB_DEFAULT, TB_DEFAULT, gmi_data);

		// current url
		tb_colorline(0, tb_height()-2, TB_WHITE);
		tb_printf(0, tb_height()-2, TB_BLACK, TB_WHITE, "%s", gmi_url);

		// input
		if (input_error) {
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
			//tb_printf(0, tb_height()-1, TB_DEFAULT, TB_DEFAULT, "%s", gmi_input);
		}
		else tb_printf(0, tb_height()-1, TB_DEFAULT, TB_DEFAULT, "%s", input);

		tb_present();
	}
	tb_shutdown();
	return 0;
}
