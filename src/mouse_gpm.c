struct tb_event;
#include "mouse.h"
#ifdef HAS_GPM
#include <gpm.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include "termbox.h"

int mouse_gpm_fd;

int mouse_init(void) {
	struct Gpm_Connect gpm;
	const char *tty, *tty_start;
	int tty_id, fd;

	mouse_gpm_fd = -1;

	tty_start = ttyname(STDIN_FILENO);
	tty = tty_start + strlen(tty_start) - 1;
	while (*tty >= '0' && *tty <= '9' && tty > tty_start) tty--;
	tty++;
	tty_id = atoi(tty);
	if (tty_id == 0 && strcmp(tty, "0")) return -1;

	gpm.eventMask = ~0 & ~GPM_DRAG;
	gpm.defaultMask = 0;
	gpm.minMod = 0;
	gpm.maxMod = ~0;
	if ((fd = Gpm_Open(&gpm, tty_id)) < 0) return -1;
	mouse_gpm_fd = fd;
	return 0;
}

int mouse_event(struct tb_event *ev) {
	struct Gpm_Event event;
	if (!Gpm_GetEvent(&event)) return -1;
	if (event.type == GPM_MOVE) {
		tb_set_cursor(event.x - 1, event.y - 1);
		tb_present();
		if (!event.wdy) return 1;
	}
	ev->type = TB_EVENT_MOUSE;
	ev->x = event.x - 1;
	ev->y = event.y - 1;
	ev->mod = 0;
	switch (event.buttons) {
	case GPM_B_LEFT:
		ev->key = TB_KEY_MOUSE_LEFT;
		break;
	case GPM_B_RIGHT:
		ev->key = TB_KEY_MOUSE_RIGHT;
		break;
	case GPM_B_MIDDLE:
		ev->key = TB_KEY_MOUSE_MIDDLE;
		break;
	default:
		if (event.wdy > 0) ev->key = TB_KEY_MOUSE_WHEEL_UP;
		if (event.wdy < 0) ev->key = TB_KEY_MOUSE_WHEEL_DOWN;
	}
	return 0;
}
#endif
