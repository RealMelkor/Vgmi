/* See LICENSE file for copyright and license details. */
#ifdef __linux__
#define _GNU_SOURCE
#endif
#include <signal.h>
#include <termbox.h>
#include "input.h"
#include "gemini.h"
#include "display.h"
#include "cert.h"
#include "sandbox.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>

int main(int argc, char* argv[]) {

#ifdef MEM_CHECK
	__init();
#endif

#ifdef __FreeBSD__
	int ttyfd = open("/dev/tty", O_RDWR);
	if (ttyfd < 0) {
		printf("Failed to open tty\n");
		return -1;
	}
#endif

	if (sandbox_init()) {
		printf("Failed to sandbox\n");
		return -1;
	}

#ifdef __FreeBSD__
	if (tb_init_fd(ttyfd) == TB_ERR_INIT_OPEN) {
#else
	if (tb_init() == TB_ERR_INIT_OPEN) {
#endif
		printf("Failed to initialize termbox\n");
		return -1;
	}

	if (gmi_init()) return 0;

	struct gmi_tab* tab = gmi_newtab_url(NULL);
	if (argc > 1) {
		if (gmi_loadfile(tab, argv[1]) <= 0) {
			gmi_gohome(tab, 1);
			gmi_request(tab, argv[1], 1);
		}
	} else gmi_gohome(tab, 1);
	
#ifdef TERMINAL_IMG_VIEWER
	if (tb_set_output_mode(TB_OUTPUT_256)) {
		gmi_free();
#ifdef __FreeBSD__
		close(ttyfd);
#endif
		printf("Terminal doesn't support 256 colors mode\n");
		return -1;
	}
	client.c256 = 1;
#endif

	struct tb_event ev;
	bzero(&ev, sizeof(ev));
	int ret = 0;
	client.tabs[client.tab].scroll = -1;

	while (!client.shutdown) {
		display();
		if (!((ret = tb_poll_event(&ev)) == TB_OK || ret == -14)) {
			break;
		}
		if (input(ev)) break;
	}
	tb_shutdown();
#ifdef __FreeBSD__
	close(ttyfd);
#endif
	gmi_free();
#ifdef MEM_CHECK
	__check();
#endif
	return 0;
}
