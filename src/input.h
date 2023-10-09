/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
void client_enter_mode_cmdline(struct client *client);
int client_input_cmdline(struct client *client, struct tb_event ev);
int client_input_normal(struct client *client, struct tb_event ev);
