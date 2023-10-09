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
