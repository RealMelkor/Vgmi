/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@rmf-dev.com>
 */

#ifdef KNOWN_HOSTS_INTERNAL
struct known_host {
	char host[MAX_HOST];
	char hash[128];
	time_t start;
	time_t end;
	struct known_host *next;
};
#define HT_SIZE	4096
extern struct known_host *known_hosts[HT_SIZE];
#endif

int known_hosts_verify(const char *, const char *, time_t, time_t);
int known_hosts_load(void);
int known_hosts_rewrite(void);
int known_hosts_expired(const char *host);
int known_hosts_forget(const char *host);
void known_hosts_free(void);
