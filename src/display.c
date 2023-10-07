#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "termbox.h"
#include "macro.h"
#include "gemtext.h"
#include "request.h"
#include "client.h"
#include "tab.h"

struct rect client_display_rect(struct client *client) {
	struct rect rect = {0};
	rect.w = client->width;
	rect.h = client->height - 2;
	return rect;
}

void client_display(struct client* client) {
	if (!client) return;
	tb_clear();
	client_draw(client);
	tab_display(client->tab, client);
	tb_present();
}

void client_draw(struct client* client) {

	int i;
	struct request *req;

	if (!client) return;

	req = tab_completed(client->tab);
	/* reset counter and error when a new page is loaded */
	if (req != client->last_request) {
		client->count = 0;
		client->error = 0;
	}
	client->last_request = req;

	client->width = tb_width();
	client->height = tb_height();

	for (i = 0; i < client->width; i++) 
		tb_set_cell(i, client->height - 2, ' ', TB_BLACK, TB_WHITE);

	if (client->error)
		tb_print(0, client->height - 1, TB_WHITE, TB_RED, client->cmd);

	if (client->mode == MODE_CMDLINE) {
		tb_print(0, client->height - 1, TB_DEFAULT, TB_DEFAULT,
				client->cmd);
	}

	if (client->count) {
		tb_printf(client->width - 10, client->height - 1,
				TB_DEFAULT, TB_DEFAULT, "%d", client->count);
	}

	tb_printf(0, client->height - 2, TB_BLACK, TB_WHITE,
			"%s", req ? req->url : "about:blank");

	if (req && req->selected &&
			(size_t)req->selected <= req->text.links_count) {
		const char *link = req->text.links[req->selected - 1];
		const char format[] = " => %s ";
		int len = strnlen(link, MAX_URL) + sizeof(format) - 3;
		tb_printf(client->width - len, client->height - 2,
				TB_WHITE, TB_BLUE, format, link);
	}

}
