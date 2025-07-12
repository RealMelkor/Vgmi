/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@rmf-dev.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include "macro.h"
#include "strscpy.h"
#define KNOWN_HOSTS_INTERNAL
#include "known_hosts.h"
#include "storage.h"
#include "error.h"

#define FILENAME "known_hosts"

#define HT_MOD	(HT_SIZE - 1)

struct known_host *known_hosts[HT_SIZE] = {NULL};

const uint32_t fnv_prime = 0x01000193;
const uint32_t fnv_offset = 0x811C9DC5;
static uint32_t fnv1a(const uint8_t *data, size_t length) {
	uint32_t hash = fnv_offset;
	size_t i;
	for (i = 0; i < length; i++)
		hash = (hash ^ data[i]) * fnv_prime;
	return hash;
}
#define FNV1A(X) (fnv1a((uint8_t*)X, strlen(X)) & HT_MOD)

int known_hosts_add(const char *host, const char *hash,
			time_t start, time_t end) {
	struct known_host **ptr;
	int index = FNV1A(host);
	for (ptr = &known_hosts[index]; *ptr; ptr = &((*ptr)->next)) ;
	*ptr = calloc(1, sizeof(struct known_host));
	if (!*ptr) return ERROR_MEMORY_FAILURE;

	STRSCPY((*ptr)->host, host);
	STRSCPY((*ptr)->hash, hash);
	(*ptr)->start = start;
	(*ptr)->end = end;
	return 0;
}

int known_hosts_load(void) {

	FILE *f;
	int ch = !EOF;
	int ret = 0;
	ASSERT((HT_SIZE & HT_MOD) == 0);

	f = storage_fopen(FILENAME, "r");
	if (!f) return 0;

	while (ch != EOF) {

		char line[1024] = {0};
		char *host, *hash, *start, *end;
		size_t i;
		struct known_host known_host;

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

		if ((ret = known_hosts_add(host, hash,
					known_host.start, known_host.end))) {
			return ret;
		}
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

int known_hosts_rewrite(void) {
	FILE *f;
	int i;

	f = storage_fopen(FILENAME, "w");
	if (!f) return ERROR_STORAGE_ACCESS;

	for (i = 0; i < HT_SIZE; i++) {
		struct known_host *ptr;
		for (ptr = known_hosts[i]; ptr; ptr = ptr->next) {
			fprintf(f, "%s %s "TIME_T" "TIME_T"\n",
				ptr->host, ptr->hash, ptr->start, ptr->end);
		}
	}
	fclose(f);
	return 0;
}

struct known_host *known_hosts_get(const char *host) {
	int index = FNV1A(host);
	struct known_host *ptr = known_hosts[index];
	for (; ptr; ptr = ptr->next) {
		if (!strcmp(ptr->host, host)) break;
	}
	return ptr;
}

int known_hosts_verify(const char *host, const char *hash,
			time_t start, time_t end) {

	time_t now = time(NULL);
	int ret;
	struct known_host *ptr = known_hosts_get(host);
	if (!ptr) {
		if ((ret = known_hosts_add(host, hash, start, end)))
			return ret;
		return known_hosts_write(host, hash, start, end);
	}
	if (ptr->end < now) {
		if (start > now) return ERROR_CERTIFICATE_AHEAD;
		/* old certificate expired, accept new certificate */
		STRSCPY(ptr->hash, hash);
		ptr->start = start;
		ptr->end = end;
		return known_hosts_rewrite();
	}
	if (strcmp(ptr->hash, hash))
		return ERROR_CERTIFICATE_MISMATCH;
	if (ptr->end < now)
		return ERROR_CERTIFICATE_EXPIRED;
	return 0;
}

int known_hosts_forget(const char *host) {
	struct known_host *ptr, *prev = NULL;
	int index = FNV1A(host);
	for (ptr = known_hosts[index]; ptr; ptr = ptr->next) {
		if (!strcmp(ptr->host, host)) break;
		prev = ptr;
	}
	if (!ptr) return ERROR_INVALID_ARGUMENT;
	if (prev) prev->next = ptr->next;
	else known_hosts[index] = ptr->next;
	free(ptr);
	return known_hosts_rewrite();
}

int known_hosts_expired(const char *host) {
	struct known_host *ptr = known_hosts_get(host);
	if (!ptr) return -1;
	return time(NULL) > ptr->end;
}

void known_hosts_free(void) {
	int i;
	for (i = 0; i < HT_SIZE; i++) {
		struct known_host *ptr, *next = NULL;
		for (ptr = known_hosts[i]; ptr; ptr = next) {
			next = ptr->next;
			free(ptr);
		}
	}
}
