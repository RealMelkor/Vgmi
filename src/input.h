/*
 * ISC License
 * Copyright (c) 2025 RMF <rawmonk@rmf-dev.com>
 */

void client_enter_mode_cmdline(struct client *client);
int client_input_cmdline(struct client *client, struct tb_event ev);
int client_input_normal(struct client *client, struct tb_event ev);
int client_input_mouse(struct client *client, struct tb_event ev);
void client_reset(struct client *client);
void client_reset_mode(struct client *client);
void move_tab_left(struct client *client);
void move_tab_right(struct client *client);
