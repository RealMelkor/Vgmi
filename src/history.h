/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@rmf-dev.com>
 */
#ifdef HISTORY_INTERNAL
struct history_entry {
	char url[1024];
	char title[1024];
	struct history_entry *next;
};
extern struct history_entry *history;
extern pthread_mutex_t history_mutex;
#endif
void history_init(void);
int history_save(void);
int history_add(const char *url, const char *title);
void history_free(void);
int history_clear(void);
