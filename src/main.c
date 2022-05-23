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
#include <stdio.h>
#include <string.h>
#include <strings.h>

int main(int argc, char* argv[]) {
#ifdef MEM_CHECK
	__init();
#endif
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
	if (
#ifndef HIDE_HOME
		unveil(path, "r") ||
#endif
		unveil(certpath, "rwc") || 
#ifndef DISABLE_XDG
		unveil("/bin/sh", "x") ||
		unveil("/usr/bin/which", "x") ||
		unveil("/usr/local/bin/xdg-open", "x") ||
#endif
		unveil("/etc/resolv.conf", "r") ||
		unveil(NULL, NULL)) {
		printf("Failed to unveil\n");
		return -1;
	}
#ifndef DISABLE_XDG
	if (pledge("stdio rpath wpath cpath inet dns tty exec proc", NULL)) {
#else
	if (pledge("stdio rpath wpath cpath inet dns tty", NULL)) {
#endif
		printf("Failed to pledge\n");
		return -1;
	}
#endif
	if (gmi_init()) return 0;
	struct gmi_tab* tab = gmi_newtab_url(NULL);
	if (argc > 1) {
		if (gmi_loadfile(tab, argv[1]) <= 0) {
			gmi_gohome(tab, 1);
			gmi_request(tab, argv[1], 1);
		}
	} else gmi_gohome(tab, 1);
	
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

	while (!client.shutdown) {
		display();
		if (!((ret = tb_poll_event(&ev)) == TB_OK || ret == -14)) {
			break;
		}
		if (input(ev)) break;
	}
	tb_shutdown();
	gmi_free();
#ifdef MEM_CHECK
	__check();
#endif
	return 0;
}
