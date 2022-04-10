/* See LICENSE file for copyright and license details. */
#include "gemini.h"
#include "display.h"
#include "input.h"
#include "cert.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#define TB_IMPL
#include <termbox.h>

int main(int argc, char* argv[]) {
#ifdef __OpenBSD__
#ifndef HIDE_HOME
	char path[1024];
	gethomefolder(path, sizeof(path));
#endif
	char certpath[1024];
	getcachefolder(certpath, sizeof(certpath));
	tb_init();
	if (unveil(certpath, "rwc") || 
#ifndef HIDE_HOME
	    unveil(path, "r") ||
#endif
	    unveil(NULL, NULL)) {
		printf("Failed to unveil\n");
		return 0;
	}
	if (pledge("stdio rpath wpath cpath inet dns tty", NULL)) {
		printf("Failed to pledge\n");
		return 0;
	}
#endif
	if (gmi_init()) return 0;
	gmi_newtab();
	if (argc > 1) {
		if (gmi_loadfile(argv[1]) > 0)
			gmi_load(&client.tabs[client.tab].page);
		else if (gmi_request(argv[1]) > 0) {
			client.error[0] = '\0';
			client.input.error = 0;
		}
			
	}
	
#ifndef __OpenBSD__
	tb_init();
#endif
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
