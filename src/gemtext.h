#define GMI_SUCCESS 20

struct gemtext_line;

struct gemtext {
	char title[1024];
	struct gemtext_line *lines;
	size_t length;
	size_t links_count;
	char **links;
	int width;
};

int gemtext_display(struct gemtext, int, int, int);
int gemtext_links(const char*, size_t, char***, size_t*);
int gemtext_parse(const char*, size_t, int, struct gemtext*);
int gemtext_free(struct gemtext gemtext);
