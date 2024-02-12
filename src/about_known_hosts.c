/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "macro.h"
#include "strlcpy.h"
#include "error.h"
#define KNOWN_HOSTS_INTERNAL
#include "known_hosts.h"
#define ABOUT_INTERNAL
#include "about.h"

int about_known_hosts_arg(const char *param) {
	int ret;
	if ((ret = known_hosts_forget(param))) return ret;
	if ((ret = known_hosts_rewrite())) return ret;
	return 0;
}

int about_known_hosts(char **out, size_t *length_out) {

	char *data = NULL;
	size_t length = 0;
	size_t i;
	const char title[] = "# Known hosts\n\n";

	if (!(data = dyn_strcat(data, &length, V(header)))) goto fail;
	if (!(data = dyn_strcat(data, &length, V(title)))) goto fail;

	for (i = 0; i < HT_SIZE; i++) {
		struct known_host *ptr;
		for (ptr = known_hosts[i]; ptr; ptr = ptr->next) {
			char buf[1024], from[64], to[64];
			int len;
			struct tm *tm;

			tm = localtime(&ptr->start);
			strftime(V(from), "%Y/%m/%d %H:%M:%S", tm);

			tm = localtime(&ptr->end);
			strftime(V(to), "%Y/%m/%d %H:%M:%S", tm);

			len = snprintf(V(buf),
				"* %s\n> Hash: %s\n"
				"> From: %s\n> Expiration: %s\n"
				"=>/%s Forget\n\n",
				ptr->host, ptr->hash,
				from, to, ptr->host) + 1;
			if (!(data = dyn_strcat(data, &length, buf, len)))
				goto fail;
		}
	}

	*out = data;
	*length_out = length;
	return 0;
fail:
	free(data);
	return ERROR_MEMORY_FAILURE;
}
