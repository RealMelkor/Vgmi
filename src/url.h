/*
 * ISC License
 * Copyright (c) 2024 RMF <rawmonk@rmf-dev.com>
 */
enum {
	PROTOCOL_NONE,
	PROTOCOL_GEMINI,
	PROTOCOL_INTERNAL,
	PROTOCOL_HTTP,
	PROTOCOL_HTTPS,
	PROTOCOL_GOPHER,
	PROTOCOL_MAIL,
	PROTOCOL_UNKNOWN
};

struct request;
int url_parse(struct request*, const char*);
int protocol_from_url(const char *url);
int url_parse_idn(const char *in, char *out, size_t out_length);
int url_hide_query(const char *url, char *out, size_t length);
int url_convert(const char *url, char *out, size_t length);
int url_is_absolute(const char *url);
int url_domain_port(const char *in, char *domain, int *port);
