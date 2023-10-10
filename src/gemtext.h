/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#define GMI_SUCCESS 20
#define GMI_INPUT 10

struct gemtext_line;

struct gemtext {
	char title[1024];
	struct gemtext_line *lines;
	size_t length;
	size_t links_count;
	char **links;
	int width;
};

#ifdef GEMTEXT_INTERNAL
#define OFFSETX 2

struct gemtext_cell {
	uint32_t codepoint;
	uint16_t color;
	uint8_t width;
	int link;
};

struct gemtext_line {
	struct gemtext_cell *cells;
	size_t length;
};
#endif

int gemtext_display(struct gemtext, int, int, int);
int gemtext_links(const char*, size_t, char***, size_t*);
int gemtext_parse(const char*, size_t, int, struct gemtext*);
int gemtext_free(struct gemtext gemtext);
