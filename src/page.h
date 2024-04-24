/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
struct page_line;
struct page_search;

struct rect {
	int x;
	int y;
	int w;
	int h;
};

struct page {
	char title[1024];
	struct page_line *lines;
	struct page_search *results;
	size_t length;
	size_t links_count;
	char **links;
	int width;
	int mime;
	int offset;
	unsigned int selected;
	unsigned int occurrences;
	int img_tried;
	int img_w;
	int img_h;
	void *img;
};

#define TAB_SIZE 4
#define PARSER_CHUNK 512

#define PAGE_NEWLINE	(uint8_t)1
#define PAGE_EOF	(uint8_t)2
#define PAGE_BLANK	(uint8_t)3
#define PAGE_RESET	(uint8_t)4
#define BLOCK_SIZE 4096

#ifdef PAGE_INTERNAL
#define OFFSETX 2

struct page_cell {
	uint32_t codepoint;
	uint32_t link;
	uint32_t selected;
	uint16_t color;
	unsigned width:4;
	unsigned special:4;
};

struct page_line {
	struct page_cell *cells;
	size_t length;
};

struct page_search {
	size_t line;
	struct page_search *next;
};

struct termwriter {
	struct page_cell cells[512]; /* maximum cells for a line */
	size_t pos;
	size_t sent;
	size_t width;
	size_t last_reset;
	size_t last_x;
	int fd;
};

int writecell(struct termwriter *termwriter, struct page_cell cell,
		size_t pos);
int writenewline(struct termwriter *termwriter, size_t pos);
int writeto(struct termwriter *termwriter, const char *str,
			int color, int link, size_t pos);
#endif

int page_display(struct page text, int from, struct rect rect, int selected);
int page_update(int in, int out, const char *data, size_t length,
			struct page *page);
int page_free(struct page page);
void page_search(struct page *page, const char *search);
int page_selection_line(struct page page);
int page_link_line(struct page page, int y);
