/* See LICENSE file for copyright and license details. */
#include "gemini.h"
#include "display.h"
#include "input.h"
#include "cert.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#define TB_IMPL
#include <termbox.h>

int main(int argc, char* argv[]) {
	if (gmi_init()) return 0;
	gmi_newtab();
	if (argc < 2)
		;//gmi_request("gemini://gemini.rmf-dev.com");
	else if (argv[1][0] == '/' || argv[1][0] == '.') {
		if (gmi_loadfile(argv[1]))
			gmi_load(&client.tabs[client.tab].page);
		else
			client.input.error = 1;
	} else
		gmi_request(argv[1]);
	
	tb_init();
	struct tb_event ev;
	bzero(&ev, sizeof(ev));
	int ret = 0;
	client.tabs[client.tab].scroll = -1;

	struct gmi_tab* tab = &client.tabs[client.tab];
	struct gmi_page* page = &tab->page;
	while (1) {
		display();
		if (!((ret = tb_poll_event(&ev)) == TB_OK || ret == -14)) {
			break;
		}
		if (input(ev)) break;
	}
	tb_shutdown();
	gmi_free();
	return 0;
}
