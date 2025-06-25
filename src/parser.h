/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@rmf-dev.com>
 */
enum {
	PARSER_GEMTEXT,
	PARSER_REQUEST
};

struct parser;
struct request;

#ifdef PARSER_INTERNAL
int vread(int fd, void *buf, size_t nbytes);
int readnext(int fd, uint32_t *ch, size_t *pos, size_t length);
int renderable(uint32_t codepoint);
#endif

#define WHITESPACE(X) ((X) <= ' ' && (X) != '\n')
#define SEPARATOR(X) ((X) <= ' ')

enum {
	MIME_GEMTEXT,
	MIME_PLAIN,
	MIME_IMAGE,
	MIME_TEXT,
	MIME_UNKNOWN
};

int is_gemtext(char *meta, size_t len);
int format_link(const char *link, size_t length, char *out, size_t out_length);
int parser_mime(char *meta, size_t len);
int parse_response(int fd, size_t length, char *meta, size_t len, int *code,
			int *bytes_read);
int parse_request(struct parser *parser, struct request *request);
int parse_page(struct parser *parser, struct request *request, int width);
int parser_request_create(void);
int parser_page_create(void);
int parse_gemtext(int in, size_t length, int width, int out);
int parse_plain(int in, size_t length, int width, int out);
int parse_binary(int in, size_t length, int width, int out);
int parse_links(int in, size_t length, int out);
void parser_page(int in, int out);
void parser_request(int in, int out);
int parser_sandbox(int out, const char *title);
