#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#define TB_IMPL
#include <termbox.h>
#include <stdarg.h>
#include "gemini.h"

int input_mode = 0;
int input_error = 0;
void tb_colorline(int x, int y, uintattr_t color) {
	for (int i=x; i<tb_width(); i++)
		tb_set_cell(i, y, ' ', color, color);
}

int main(int argc, char* argv[]) {
	if (gmi_init()) return 0;
	if (argc < 2)
		strcpy(gmi_url, "gemini://gemini.rmf-dev.com");
	else
		strncpy(gmi_url, argv[1], sizeof(gmi_url));
	int r = gmi_request(gmi_url);
	if (r<1) return 0;
	gmi_load(gmi_data, r);
	tb_init();
	struct tb_event ev;
	char input[1024];
	bzero(input, sizeof(input));
	int ret = 0;
	int posy = -1;
	goto display;
	while ((ret = tb_poll_event(&ev)) == TB_OK || ret == -14) {
		if (ev.type == TB_EVENT_RESIZE) goto display;
		if (ev.type != TB_EVENT_KEY) continue;

		if (input_mode && ev.key == TB_KEY_ESC) {
			input_mode = 0;
			input[0] = '\0';
		}
		if (input_mode && ev.key == TB_KEY_BACKSPACE2) {
			int i = strnlen(input, sizeof(input));
			if (i>1) input[i-1] = '\0';
		}
		if (!input_mode && ev.key == TB_KEY_ENTER) {
			if (posy+tb_height()-3<gmi_lines) posy++;
		}
		if (input_mode && ev.key == TB_KEY_ENTER) {
			input_mode = 0;
			if (input[1] == 'q' && input[2] == '\0') break;
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

		switch (ev.ch) {
		case ':':
			input_mode = 1;
			input[0] = ':';
			input[1] = '\0';
			break;
		case 'b': // Back
			break;
		case 'e': // Forward
			break;
		case 'k': // UP
			if (posy>-1) posy--;
			break;
		case 'j': // DOWN
			if (posy+tb_height()-3<gmi_lines) posy++;
			break;
		case 'h': // LEFT
			break;
		case 'l': // RIGHT
			break;

		}
display:
		tb_clear();

		gmi_render(gmi_data, r, posy);

		// current url
		tb_colorline(0, tb_height()-2, TB_WHITE);
		tb_printf(0, tb_height()-2, TB_BLACK, TB_WHITE, gmi_url);

		// input
		//tb_printf(0, tb_height()-1, TB_DEFAULT, TB_DEFAULT, "%s %d", input, ev.key);
		if (input_error) {
			tb_colorline(0, tb_height()-1, TB_RED);
			tb_printf(0, tb_height()-1, TB_WHITE, TB_RED, gmi_error);
			input[0] = '\0';
			input_error = 0;
		}
		else tb_printf(0, tb_height()-1, TB_DEFAULT, TB_DEFAULT, "%s", input);

		tb_present();
	}
	tb_shutdown();
	return 0;
}
