/*
 * ISC License
 * Copyright (c) 2024 RMF <rawmonk@rmf-dev.com>
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
#include "strscpy.h"

int idn_to_ascii(const char* domain, size_t dlen, char* out, size_t outlen) {

	const char* ptr = domain;
	uint32_t part[1024] = {0};
	size_t pos = 0;
	int n = 0;
	int unicode = 0;
	size_t i;

	for (i = 0; i < LENGTH(part) && i < dlen; i++) {
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
			pos += strscpy(&out[pos], "xn--", outlen - pos);
			ret = punycode_encode(i - n, &part[n],
					NULL, &len, &out[pos]);
			if (ret != punycode_success)
				return -1;
			pos += len;
		} else {
			size_t j;
			for (j = n; j < i; j++) {
				if (pos >= outlen) return -1;
				out[pos] = part[j];
				pos++;
			}
		}
		unicode = 0;
		n = i + 1;
		if (*ptr == '.') {
			if (pos >= outlen) return -1;
			out[pos] = '.';
			pos++;
			ptr++;
		}

		if (!*ptr) {
			if (pos >= outlen) return -1;
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
	if (!end) end = start + strnlen(url, MAX_URL);

	if ((size_t)(end - start) >= len) return ERROR_BUFFER_OVERFLOW;

	strscpy(out, start, end - start + 1);
	return 0;
}

int protocol_from_url(const char *url) {
	if (!memcmp(url, V("mailto:") - 1)) return PROTOCOL_MAIL;
	if (!memcmp(url, V("about:") - 1)) return PROTOCOL_INTERNAL;
	if (!strnstr(url, "://", MAX_URL)) return PROTOCOL_NONE; /* default */
	if (!memcmp(url, V("gemini://") - 1)) return PROTOCOL_GEMINI;
	if (!memcmp(url, V("http://") - 1)) return PROTOCOL_HTTP;
	if (!memcmp(url, V("https://") - 1)) return PROTOCOL_HTTPS;
	if (!memcmp(url, V("gopher://") - 1)) return PROTOCOL_GOPHER;
	return PROTOCOL_UNKNOWN;
}

int port_from_url(const char *url) {

	const char *const_ptr;
	char *ptr, buf[MAX_URL];
	int port;

	const_ptr = strnstr(url, "://", MAX_URL);
	if (!const_ptr) const_ptr = url;
	else const_ptr += sizeof("://") - 1;
	STRSCPY(buf, const_ptr);
	ptr = strchr(buf, '/');
	if (ptr) *ptr = '\0';
	ptr = strchr(buf, ':');
	if (!ptr) return 0;
	port = atoi(ptr + 1);
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
		size_t length = STRSCPY(buf, "gemini://");
		int i;
		i = strscpy(&buf[length], url, sizeof(buf) - length);
		i += length;
		buf[i] = '\0';
		protocol = PROTOCOL_GEMINI;
	} else STRSCPY(buf, url);

	port = port_from_url(url);
	if (port < 0) return port;
	if (!port) {
		switch (protocol) {
		case PROTOCOL_GEMINI: port = 1965; break;
		}
	}

	request->protocol = protocol;
	request->port = port;
	STRSCPY(request->url, buf);

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
		strscpy(out, in, out_length);
		return 0;
	}
	servername_from_url(in, V(buf));
	if (idn_to_ascii(V(buf), V(host)))
		return ERROR_INVALID_URL;
	strscpy(out, in, out_length);
	ptr = strnstr(out, buf, out_length);
	if (!ptr) return ERROR_INVALID_URL;
	offset = (ptr - out) + strnlen(V(buf));
	ptr += strscpy(ptr, host, out_length - (ptr - out));
	strscpy(ptr, &in[offset], out_length - (ptr - out));
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
	unsigned int j = 0, i = 0;
	int slash = 0;
	while (i < length) {
		uint32_t ch;
		int len, k;
		if (j > MAX_URL) break;
		len = utf8_char_to_unicode(&ch, &url[j]);
		if (!ch) {
			out[i] = 0;
			return 0;
		}
		if (slash < 3) {
			slash += ch == '/';
			i += utf8_unicode_to_char(&out[i], ch);
			j += len;
			continue;
		}
		if ((len == 1 && valid_char(ch))) {
			out[i++] = url[j++];
			continue;
		}
		for (k = 0; k < len; k++) {
			int ret;
			if (i + 4 > length) break;
			out[i++] = '%';
			if (j > MAX_URL) {
				out[i] = 0;
				return -1;
			}
			ret = snprintf(&out[i], length - i, "%02X", url[j++]);
			if (ret < 1) {
				out[i] = 0;
				return -1;
			}
			i += ret;
		}
	}
	out[length - 1] = 0;
	return -1;
}

int url_is_absolute(const char *url) {
	return !!strnstr(url, "://", MAX_URL) ||
		!memcmp(url, V("mailto:") - 1);
}

int url_domain_port(const char *in, char *domain, int *port) {
	char *ptr = strrchr(in, ':');
	if (!ptr) {
		strscpy(domain, in, MAX_HOST);
		*port = 1965;
		return 0;
	}
	*port = atoi(ptr + 1);
	if (!*port) *port = 1965;
	strscpy(domain, in, ptr - in + 1);
	return 0;
}
