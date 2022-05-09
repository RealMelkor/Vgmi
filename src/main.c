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
	if (gethomefolder(path, sizeof(path)) < 1) {
		printf("Failed to get home folder\n");
		return -1;
	}
#endif
	char certpath[1024];
	if (getcachefolder(certpath, sizeof(certpath)) < 1) {
		printf("Failed to get cache folder\n");
		return -1;
	}
	tb_init();
	if (unveil(certpath, "rwc") || 
#ifndef HIDE_HOME
		unveil(path, "r") ||
#endif
		unveil(NULL, NULL)) {
		printf("Failed to unveil\n");
		return -1;
	}
	if (pledge("stdio rpath wpath cpath inet dns tty", NULL)) {
		printf("Failed to pledge\n");
		return -1;
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
	
#ifdef TERMINAL_IMG_VIEWER
	if (tb_set_output_mode(TB_OUTPUT_256)) {
		gmi_free();
		printf("Terminal doesn't support 256 colors mode\n");
		return -1;
	}
	client.c256 = 1;
#endif

	struct tb_event ev;
	bzero(&ev, sizeof(ev));
	int ret = 0;
	client.tabs[client.tab].scroll = -1;

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
