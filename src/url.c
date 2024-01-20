/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
#include "macro.h"
#include "utf8.h"
#include "url.h"
#include "punycode.h"
#include "page.h"
#include "request.h"
#include "error.h"
#include "strnstr.h"
#include "strlcpy.h"

int idn_to_ascii(const char* domain, size_t dlen, char* out, size_t outlen) {

	const char* ptr = domain;
	uint32_t part[1024] = {0};
	size_t pos = 0;
	int n = 0;
	int unicode = 0;
	size_t i;

	for (i = 0; i < sizeof(part) && i < dlen; i++) {
		uint32_t len;
		if (*ptr && *ptr != '.') {
			if (*ptr & 128)
				unicode = 1;
			ptr += utf8_char_to_unicode(&part[i], ptr);
			continue;
		}
		len = outlen - pos;
		if (unicode) {
			int ret;
			pos += strlcpy(&out[pos], "xn--", sizeof(out) - pos);
			ret = punycode_encode(i - n, &part[n],
					NULL, &len, &out[pos]);
			if (ret != punycode_success)
				return -1;
			pos += len;
		} else {
			size_t j;
			for (j = n; j < i; j++) {
				out[pos] = part[j];
				pos++;
			}
		}
		unicode = 0;
		n = i + 1;
		if (*ptr == '.') {
			out[pos] = '.';
			pos++;
			ptr++;
		}

		if (!*ptr) {
			out[pos] = '\0';
			break;
		}
	}
	return 0;
}

int servername_from_url(const char *url, char* out, size_t len) {

	const char *start, *port, *end;

	start = strnstr(url, "://", len);
	if (!start) start = url;
	else start += sizeof("://") - 1;

	port = strchr(start, ':');
	end = strchr(start, '/');
	if (!end || (port && port < end)) end = port;
	if (!end) end = start + strlen(url);

	if ((size_t)(end - start) >= len) return ERROR_BUFFER_OVERFLOW;

	strlcpy(out, start, end - start + 1);
	return 0;
}

int protocol_from_url(const char *url) {
	if (!memcmp(url, V("mailto:") - 1)) return PROTOCOL_MAIL;
	if (!strnstr(url, "://", MAX_URL)) return PROTOCOL_NONE; /* default */
	if (!memcmp(url, V("gemini://") - 1)) return PROTOCOL_GEMINI;
	if (!memcmp(url, V("http://") - 1)) return PROTOCOL_HTTP;
	if (!memcmp(url, V("https://") - 1)) return PROTOCOL_HTTPS;
	if (!memcmp(url, V("gopher://") - 1)) return PROTOCOL_GOPHER;
	return PROTOCOL_UNKNOWN;
}

int port_from_url(const char *url) {

	const char *start, *end;
	char buf[MAX_URL];
	int port;

	start = strnstr(url, "://", MAX_URL);
	if (!start) start = url;
	end = strchr(start + sizeof("://"), '/');
	start = strchr(start + sizeof("://"), ':');
	if (!start || (end && end < start)) return 0;
	start++;
	end = strchr(start, '/') + 1;
	if (!end) end = start + strlen(start);
	strlcpy(buf, start, end - start);
	port = atoi(buf);
	if (!port) return ERROR_INVALID_PORT;
	return port;
}

int url_parse(struct request* request, const char *url) {

	int protocol, port, ret;
	char buf[MAX_URL];

	memset(request, 0, sizeof(*request));

	if ((ret = servername_from_url(url, V(request->name)))) return ret;

	protocol = protocol_from_url(url);
	if (protocol == PROTOCOL_UNKNOWN) return ERROR_UNKNOWN_PROTOCOL;
	if (protocol == PROTOCOL_NONE) {
		size_t length = STRLCPY(buf, "gemini://");
		int i;
		i = strlcpy(&buf[length], url, sizeof(buf) - length);
		i += length;
		buf[i] = '\0';
		protocol = PROTOCOL_GEMINI;
	} else STRLCPY(buf, url);

	port = port_from_url(url);
	if (port < 0) return port;
	if (!port) {
		switch (protocol) {
		case PROTOCOL_GEMINI: port = 1965; break;
		}
	}

	request->protocol = protocol;
	request->port = port;
	STRLCPY(request->url, buf);

	return 0;
}

int url_parse_idn(const char *in, char *out, size_t out_length) {
	char host[256] = {0}, buf[256] = {0}, *ptr, *end;
	size_t offset;
	ptr = out;
	end = out + out_length;
	while (*ptr && ptr < end) {
		if (utf8_char_length(*ptr) != 1) {
			ptr = NULL;
			break;
		}
		ptr++;
	}
	if (ptr) {
		strlcpy(out, in, out_length);
		return 0;
	}
	servername_from_url(in, V(buf));
	if (idn_to_ascii(V(buf), V(host)))
		return ERROR_INVALID_URL;
	strlcpy(out, in, out_length);
	ptr = strnstr(out, buf, out_length);
	if (!ptr) return ERROR_INVALID_URL;
	offset = (ptr - out) + strnlen(V(buf));
	ptr += strlcpy(ptr, host, out_length - (ptr - out));
	strlcpy(ptr, &in[offset], out_length - (ptr - out));
	return 0;
}

int url_hide_query(const char *url, char *out, size_t length) {
	size_t i, j;
	int inquery;
	for (inquery = i = j = 0; i < length; ) {
		uint32_t ch;
		i += utf8_char_to_unicode(&ch, &url[i]);
		if (!ch) break;
		if (ch == '/' && inquery) inquery = 0;
		if (inquery) continue;
		j += utf8_unicode_to_char(&out[j], ch);
		if (ch == '?') {
			out[j++] = '<';
			out[j++] = '*';
			out[j++] = '>';
			inquery = 1;
		}
	}
	out[j] = 0;
	return 0;
}

static int valid_char(char c) {
	if (c == '"' || c == '%') return 0;
	return ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
		(c >= '!' && c <= ';') || c == '=' || c == '~' || c == '_');
}

int url_convert(const char *url, char *out, size_t length) {
	unsigned int j, i;
	int slash = 0;
	for (i = j = 0; i < length;) {
		uint32_t ch;
		int len, k;
		len = utf8_char_to_unicode(&ch, &url[j]);
		if (!ch) {
			out[i] = 0;
			return 0;
		}
		if (slash < 3) {
			slash += ch == '/';
			utf8_unicode_to_char(&out[i], ch);
			i += len;
			j += len;
			continue;
		}
		if ((len == 1 && valid_char(ch))) {
			out[i++] = url[j++];
			continue;
		}
		for (k = 0; k < len; k++) {
			if (i + 3 > length) break;
			out[i++] = '%';
			i += snprintf(&out[i], length - i, "%02X", url[j++]);
		}
	}
	out[length - 1] = 0;
	return -1;
}

int url_is_absolute(const char *url) {
	return !!strnstr(url, "://", MAX_URL) ||
		!memcmp(url, V("mailto:") - 1);
}
