/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
int bookmark_load(void);
int bookmark_rewrite(void);
int bookmark_add(const char *url, const char *name);
int bookmark_remove(size_t id);

struct bookmark {
	char url[MAX_URL];
	char name[128];
};

extern struct bookmark *bookmarks;
extern size_t bookmark_length;
