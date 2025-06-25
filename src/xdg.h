/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@rmf-dev.com>
 */
int xdg_init(void);
int xdg_request(const char *request);
int xdg_available(void);
int xdg_proc(int in, int out);
