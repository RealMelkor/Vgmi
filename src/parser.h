enum {
	PARSER_GEMTEXT,
	PARSER_REQUEST
};

struct parser;

int parser_create(struct parser *parser, int type);
int parse_request(struct parser *parser, struct request *request);
int parser_request_create();
