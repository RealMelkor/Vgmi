/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
int command_quit(struct client *client, const char* ptr, size_t len);
int command_open(struct client *client, const char* ptr, size_t len);
int command_gencert(struct client *client, const char* ptr, size_t len);