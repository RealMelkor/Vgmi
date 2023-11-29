/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
enum {
	PROTOCOL_NONE,
	PROTOCOL_GEMINI,
	PROTOCOL_HTTP,
	PROTOCOL_HTTPS,
	PROTOCOL_GOPHER,
	PROTOCOL_UNKNOWN
};

struct request;
int url_parse(struct request*, const char*);
int protocol_from_url(const char *url);
int url_parse_idn(const char *in, char *out, size_t out_length);
int url_hide_query(const char *url, char *out, size_t length);
int url_convert(const char *url, char *out, size_t length);
