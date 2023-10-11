/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */

#ifdef KNOWN_HOSTS_INTERNAL
struct known_host {
	char host[256];
	char hash[128];
	time_t start;
	time_t end;
};
extern struct known_host *known_hosts;
extern size_t known_hosts_length;
#endif

int known_hosts_verify(const char *, const char *, time_t, time_t);
int known_hosts_load();
void known_hosts_free();
