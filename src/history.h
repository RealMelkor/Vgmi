/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#ifdef HISTORY_INTERNAL
struct history_entry {
	char url[1024];
	char title[1024];
	struct history_entry *next;
};
extern struct history_entry *history;
#endif
void history_init();
int history_save();
int history_add(const char *url, const char *title);
void history_free();
