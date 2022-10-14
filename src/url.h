/* See LICENSE file for copyright and license details. */
#ifndef _URL_H_
#define _URL_H_

#include <stdint.h>
#include <stddef.h>
#define MAX_URL 1024
#define PROTO_GEMINI 0
#define PROTO_HTTP 1
#define PROTO_HTTPS 2
#define PROTO_GOPHER 3
#define PROTO_FILE 4

int idn_to_ascii(const char* domain, size_t dlen, char* out, size_t outlen);
void parse_relative(const char* urlbuf, int host_len, char* buf);
int parse_url(const char* url, char* host, int host_len, char* buf,
	      int url_len, unsigned short* port);
int parse_query(const char* url, int len, char* buf, int llen);
int utf8_width(char* ptr, size_t len);
int utf8_len(char* ptr, size_t len);
int utf8_width_to(char* ptr, size_t len, size_t to);
int utf8_len_to(char* ptr, size_t len, size_t to_width);
int parse_link(char* data, int len);

#endif
