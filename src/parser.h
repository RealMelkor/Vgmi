enum {
	PARSER_GEMTEXT,
	PARSER_REQUEST
};

struct parser;

int parse_request(struct parser *parser, struct request *request);
int parse_gemtext(struct parser *parser, struct request *request, int width);
int parser_request_create();
int parser_gemtext_create();
