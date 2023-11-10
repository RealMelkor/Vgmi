/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "macro.h"
#include "strlcpy.h"
#define KNOWN_HOSTS_INTERNAL
#include "known_hosts.h"
#include "storage.h"
#include "error.h"

#define FILENAME "known_hosts"
#ifdef __OpenBSD__
#define TIME_T "%lld"
#else
#define TIME_T "%ld"
#endif

struct known_host *known_hosts = NULL;
size_t known_hosts_length = 0;

int known_hosts_add(const char *host, const char *hash,
			time_t start, time_t end) {
	void *tmp;
	tmp = realloc(known_hosts,
			sizeof(struct known_host) * (known_hosts_length + 1));
	if (!tmp) return ERROR_MEMORY_FAILURE;
	known_hosts = tmp;
	STRLCPY(known_hosts[known_hosts_length].host, host);
	STRLCPY(known_hosts[known_hosts_length].hash, hash);
	known_hosts[known_hosts_length].start = start;
	known_hosts[known_hosts_length].end = end;
	known_hosts_length++;
	return 0;
}

int known_hosts_load() {

	FILE *f;
	int ch = !EOF;
	int ret = 0;

	f = storage_fopen(FILENAME, "r");
	if (!f) return 0;

	while (ch != EOF) {

		char line[1024];
		char *host, *hash, *start, *end;
		size_t i;
		struct known_host known_host;
		void *tmp;

		for (i = 0; i < sizeof(line); i++) {
			ch = getc(f);
			if (ch == EOF || ch == '\n') break;
			if (ch == '\t') ch = ' ';
			if (i > 0 && ch == ' ' && line[i - 1] == ' ') i--;
			line[i] = ch;
		}
		if (i == sizeof(line)) continue;
		line[i] = '\0';

		host = line;

		hash = strchr(host, ' ');
		if (!hash) continue;
		*(hash++) = '\0';

		start = strchr(hash, ' ');
		if (!start) continue;
		*(start++) = '\0';

		end = strchr(start, ' ');
		if (!end) continue;
		*(end++) = '\0';

		known_host.start = strtoll(start, NULL, 10);
		if (!known_host.start) continue;
		known_host.end = strtoll(end, NULL, 10);
		if (!known_host.end) continue;
		STRLCPY(known_host.host, host);
		STRLCPY(known_host.hash, hash);

		tmp = realloc(known_hosts,
				sizeof(known_host) * (known_hosts_length + 1));
		if (!tmp) {
			ret = ERROR_MEMORY_FAILURE;
			break;
		}
		known_hosts = tmp;
		known_hosts[known_hosts_length] = known_host;
		known_hosts_length++;
	}

	fclose(f);
	return ret;
}

int known_hosts_write(const char *host, const char *hash,
			time_t start, time_t end) {
	FILE *f;
	f = storage_fopen(FILENAME, "a");
	if (!f) return ERROR_STORAGE_ACCESS;
	fprintf(f, "%s %s "TIME_T" "TIME_T"\n", host, hash, start, end);
	fclose(f);
	return 0;
}

int known_hosts_rewrite() {

	FILE *f;
	size_t i;

	f = storage_fopen(FILENAME, "w");
	if (!f) return ERROR_STORAGE_ACCESS;
	for (i = 0; i < known_hosts_length; i++) {
		fprintf(f, "%s %s "TIME_T" "TIME_T"\n",
			known_hosts[i].host, known_hosts[i].hash,
			known_hosts[i].start, known_hosts[i].end);
	}
	fclose(f);
	return 0;
}

int known_hosts_verify(const char *host, const char *hash,
			time_t start, time_t end) {

	time_t now = time(NULL);
	int ret;
	size_t i;

	for (i = 0; i < known_hosts_length; i++) {
		if (strcmp(known_hosts[i].host, host)) continue;
		if (known_hosts[i].end < now) {
			if (start > now)
				return ERROR_CERTIFICATE_AHEAD;
			STRLCPY(known_hosts[i].hash, hash);
			known_hosts[i].start = start;
			known_hosts[i].end = end;
			known_hosts_rewrite();
			return 0;
		}
		if (strcmp(known_hosts[i].hash, hash))
			return ERROR_CERTIFICATE_MISMATCH;
		if (known_hosts[i].end < now)
			return ERROR_CERTIFICATE_EXPIRED;
		return 0;
	}
	if ((ret = known_hosts_add(host, hash, start, end))) return ret;
	return known_hosts_write(host, hash, start, end);
}

int known_hosts_forget_id(int id) {
	size_t i;
	if ((size_t)id > known_hosts_length) return ERROR_INVALID_ARGUMENT;
	known_hosts_length--;
	for (i = id; i < known_hosts_length; i++) {
		known_hosts[i] = known_hosts[i + 1];
	}
	return 0;
}

int known_hosts_forget(const char *host) {
	size_t i;
	for (i = 0; i < known_hosts_length; i++) {
		if (!STRCMP(known_hosts[i].host, host)) break;
	}
	if (i == known_hosts_length) return ERROR_UNKNOWN_HOST;
	return known_hosts_forget_id(i);
}

/* TODO: make it O(1) */
int known_hosts_expired(const char *host) {
	size_t i;
	for (i = 0; i < known_hosts_length; i++) {
		if (!STRCMP(known_hosts[i].host, host)) break;
	}
	if (i == known_hosts_length) return -1;
	return time(NULL) > known_hosts[i].end;
}

void known_hosts_free() {
	free(known_hosts);
}
