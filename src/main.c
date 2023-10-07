#include <stddef.h>
#include <stdio.h>
#include "macro.h"
#include "client.h"
#include "dns.h"
#include "gemtext.h"
#include "request.h"
#include "secure.h"
#include "tab.h"

int main(int argc, char *argv[]) {
	
	struct client client = {0};

	if (argc) if (!argv) printf("\n");

	if (client_init(&client)) {
		printf("Initialisation failure\n");
		return -1;
	}

	client.tab = tab_new();
	tab_request(client.tab, "about:newtab");

	do {
		client_display(&client);
	} while (!client_input(&client));

	client_destroy(&client);

	return 0;
}
