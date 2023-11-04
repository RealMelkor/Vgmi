/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
int gemtext_parse(int in, size_t length, int out, int *status, char *meta,
			size_t meta_length);
int gemtext_prerender(int in, size_t length, int width, int out);
int gemtext_links(int in, size_t length, int out);
