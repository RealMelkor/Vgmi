/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */

int known_hosts_verify(const char *, const char *, time_t, time_t);
int known_hosts_load();
void known_hosts_free();
