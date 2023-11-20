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
#include "memory.h"
#define KNOWN_HOSTS_INTERNAL
#include "known_hosts.h"
#define ABOUT_INTERNAL
#include "about.h"

int about_known_hosts_arg(const char *param) {
	int id = atoi(param), ret;
	if (!id && strcmp(param, "0")) return ERROR_INVALID_ARGUMENT;
	if ((ret = known_hosts_forget_id(id))) return ret;
	if ((ret = known_hosts_rewrite())) return ret;
	return 0;
}

int about_known_hosts(char **out, size_t *length_out) {

	char *data = NULL;
	size_t length = 0;
	size_t i;
	const char title[] = "# Known hosts\n\n";

	length = sizeof(header) + sizeof(title);
	data = malloc(length);
	if (!data) return ERROR_MEMORY_FAILURE;
	strlcpy(data, header, length);
	strlcpy(&data[sizeof(header) - 1], title, length - sizeof(header));
	length -= 2;

	for (i = 0; i < known_hosts_length; i++) {

		char buf[1024], from[64], to[64];
		int len;
		void *tmp;
		struct tm *tm;

		tm = localtime(&known_hosts[i].start);
		strftime(V(from), "%Y/%m/%d %H:%M:%S", tm);

		tm = localtime(&known_hosts[i].end);
		strftime(V(to), "%Y/%m/%d %H:%M:%S", tm);

		len = snprintf(V(buf),
			"* %s\n> Hash: %s\n"
			"> From: %s\n> Expiration: %s\n"
			"=>/%ld Forget\n\n",
			known_hosts[i].host, known_hosts[i].hash,
			from, to, i) + 1;
		tmp = realloc(data, length + len);
		if (!tmp) {
			free(data);
			return ERROR_MEMORY_FAILURE;
		}
		data = tmp;
		strlcpy(&data[length], buf, len);
		length += len - 1;
	}

	readonly(data, length, out);
	free(data);
	*length_out = length;
	return 0;
}
