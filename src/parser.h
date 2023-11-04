/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
enum {
	PARSER_GEMTEXT,
	PARSER_REQUEST
};

struct parser;

#ifdef PARSER_INTERNAL
int vread(int fd, void *buf, size_t nbytes);
int readnext(int fd, uint32_t *ch, size_t *pos);
int renderable(uint32_t codepoint);
int skip_meta(int fd, size_t *pos, size_t length);
#endif

#define WHITESPACE(X) ((X) <= ' ' && (X) != '\n')
#define SEPARATOR(X) ((X) <= ' ')

int is_gemtext(char *meta, size_t len);
int parse_request(struct parser *parser, struct request *request);
int parse_page(struct parser *parser, struct request *request, int width);
int parser_request_create();
int parser_page_create();
