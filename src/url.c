/* See LICENSE file for copyright and license details. */
#include <stdlib.h>
#include <termbox.h>
#include <ctype.h>
#include "str.h"
#include "punycode.h"
#include "wcwidth.h"
#include "url.h"

int utf8_width_to(char* ptr, size_t len, size_t to) {
	int width = 0;
	char* max = ptr + len;
	for (size_t i = 0; *ptr && ptr < max && i < to; i++) {
		uint32_t c;
		ptr += tb_utf8_char_to_unicode(&c, ptr);
		width += mk_wcwidth(c);
	}
	return width;
}

int utf8_width(char* ptr, size_t len) {
	int width = 0;
	char* max = ptr + len;
	while (*ptr && ptr < max) {
		uint32_t c;
		ptr += tb_utf8_char_to_unicode(&c, ptr);
		width += mk_wcwidth(c);
	}
	return width;
}

int utf8_len(char* ptr, size_t len) {
	int ret = 0;
	char* max = ptr + len;
	while (*ptr && ptr < max) {
		ptr += tb_utf8_char_length(*ptr);
		ret++;
	}
	return ret;

}

int utf8_len_to(char* ptr, size_t len, size_t to_width) {
	char* start = ptr;
	char* max = ptr + len;
	size_t width = 0;
	while (*ptr && ptr < max) {
		uint32_t c;
		ptr += tb_utf8_char_to_unicode(&c, ptr);
		width += mk_wcwidth(c);
		if (width >= to_width)
			return ptr - start;
	}
	return ptr - start;
}

void parse_relative(const char* urlbuf, int host_len, char* buf) {
	int j = 0;
	for (size_t i = 0; i < MAX_URL; i++) {
		if (j + 1 >= MAX_URL) {
			buf[j] = '\0';
			break;
		}
		buf[j] = urlbuf[i];
		j++;
		if (urlbuf[i] == '\0') break;
		if (i > 0 && i + 2 < MAX_URL &&
			urlbuf[i - 1] == '/' && urlbuf[i + 0] == '.' &&
			(urlbuf[i + 1] == '/' || urlbuf[i + 1] == '\0')) {
			i += 1;
			j--;
			continue;
		}
		if (!(i > 0 && i + 3 < MAX_URL &&
			urlbuf[i - 1] == '/' &&
			urlbuf[i + 0] == '.' && urlbuf[i + 1] == '.' &&
			(urlbuf[i + 2] == '/' || urlbuf[i + 2] == '\0')))
			continue;
		int k = j - 3;
		i += 2;
		if (k <= (int)host_len) {
			j = k + 2;
			buf[j] = '\0';
			continue;
		}
		for (; k >= host_len && buf[k] != '/'; k--) ;
		j = k + 1;
		buf[k + 1] = '\0';
	}
}

int parse_url(const char* url, char* host, int host_len, char* buf,
		 int url_len, unsigned short* port) {
	char urlbuf[MAX_URL];
	int proto = PROTO_GEMINI;
	char* proto_ptr = strstr(url, "://");
	char* ptr = (char*)url;
	if (!proto_ptr) {
		goto skip_proto;			
	}
	char proto_buf[16];
	for(; proto_ptr!=ptr; ptr++) {
		if (!((*ptr > 'a' && *ptr < 'z') ||
		    (*ptr > 'A' && *ptr < 'Z')))
			goto skip_proto;
		if (ptr - url >= (signed)sizeof(proto_buf)) goto skip_proto;
		proto_buf[ptr-url] = tolower(*ptr);
	}
	proto_buf[ptr-url] = '\0';
	ptr+=3;
	proto_ptr+=3;
	if (!strcmp(proto_buf,"gemini")) goto skip_proto;
	else if (!strcmp(proto_buf,"http")) proto = PROTO_HTTP;
	else if (!strcmp(proto_buf,"https")) proto = PROTO_HTTPS;
	else if (!strcmp(proto_buf,"gopher")) proto = PROTO_GOPHER;
	else if (!strcmp(proto_buf,"file")) proto = PROTO_FILE;
	else return -1; // unknown protocol
skip_proto:;
	if (port && proto == PROTO_GEMINI) *port = 1965;
	if (!proto_ptr) proto_ptr = ptr;
	char* host_ptr = strchr(ptr, '/');
	if (!host_ptr) host_ptr = ptr+strnlen(ptr, MAX_URL);
	char* port_ptr = strchr(ptr, ':');
	if (port_ptr && port_ptr < host_ptr) {
		port_ptr++;	
		char c = *host_ptr;
		*host_ptr = '\0';
		if (port) {
			*port = atoi(port_ptr);
			if (*port < 1)
				return -1; // invalid port
		}
		*host_ptr = c;
		host_ptr = port_ptr - 1;
	}
	int utf8 = 0;
	for(; host_ptr!=ptr; ptr++) {
		if (host_len <= host_ptr-ptr) {
			return -1;
		}
		host[ptr - proto_ptr] = *ptr;
		if (utf8) {
			utf8--;
			continue;
		}
		utf8 += tb_utf8_char_length(*ptr) - 1;
		if (utf8) continue;
		if (*ptr < 32) {
			host[ptr - proto_ptr] = '?';
			continue;
		}
	}
	host[ptr-proto_ptr] = '\0';
	if (!buf) return proto;
	if (url_len < 16) return -1; // buffer too small
	unsigned int len = 0;
	switch (proto) {
	case PROTO_GEMINI:
		len = strlcpy(urlbuf, "gemini://", url_len);
		break;
	case PROTO_HTTP:
		len = strlcpy(urlbuf, "http://", url_len);
		break;
	case PROTO_HTTPS:
		len = strlcpy(urlbuf, "https://", url_len);
		break;
	case PROTO_GOPHER:
		len = strlcpy(urlbuf, "gopher://", url_len);
		break;
	case PROTO_FILE:
		len = strlcpy(urlbuf, "file://", url_len);
		break;
	default:
		return -1;
	}
	size_t l = strlcpy(urlbuf + len, host, sizeof(urlbuf) - len);
	if (l >= url_len - len) {
		goto parseurl_overflow;
	}
	len += l;
	if (host_ptr &&
	    strlcpy(urlbuf + len, host_ptr, url_len - len) >= 
	    url_len - len)
		goto parseurl_overflow;
	if (buf)
		parse_relative(urlbuf, len + 1, buf);
	return proto;
parseurl_overflow:
	return -2;
}

int isCharValid(char c, int inquery) {
	return (c >= 'a' && c <= 'z') ||
	    (c >= 'A' && c <= 'Z') ||
	    (c >= '0' && c <= '9') ||
	    (c == '?' && !inquery) ||
	    c == '.' || c == '/' ||
	    c == ':' || c == '-' ||
	    c == '_' || c == '~';
}

int parse_query(const char* url, int len, char* buf, int llen) {
	char urlbuf[1024];
	parse_relative(url, 0, urlbuf);
	url = urlbuf;
	int j = 0;
	int inquery = 0;
	for (int i = 0; j < llen && i < len && url[i]; i++) {
		if (url[i] == '/') inquery = 0;
		if (!inquery || isCharValid(url[i], inquery)) {
			if (url[i] == '?') inquery = 1;
			buf[j] = url[i];
			j++;
			continue;
		}
		char format[8];
		snprintf(format, sizeof(format),
			 "%%%x", (unsigned char)url[i]);
		buf[j] = '\0';
		j = strlcat(buf, format, llen);
	}
	if (j >= llen) j = llen - 1;
	buf[j] = '\0';
	return j;
}

int idn_to_ascii(const char* domain, size_t dlen, char* out, size_t outlen) {
	const char* ptr = domain;
	uint32_t part[1024] = {0};
	size_t pos = 0;
	int n = 0;
	int unicode = 0;
	for (size_t i = 0; i < sizeof(part) && i < dlen; i++) {
		if (*ptr && *ptr != '.') {
			if (*ptr & 128)
				unicode = 1;
			ptr += tb_utf8_char_to_unicode(&part[i], ptr);
			continue;
		}
		uint32_t len = outlen - pos;
		if (unicode) {
			pos += strlcpy(&out[pos], "xn--", sizeof(out) - pos);
			if (punycode_encode(i - n, &part[n],
					    NULL, &len, &out[pos]) !=
			    punycode_success)
				return -1;
			pos += len;
		} else {
			for (size_t j = n; j < i; j++) {
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

int parse_link(char* data, int len) {
        int i = 0;
        while (i < len) {
                i += tb_utf8_char_length(data[i]);
                if (data[i] == '\n' || data[i] == '\0' ||
                    data[i] == '\r' || data[i] == ' ' || data[i] == '\t')
                        return i;
                if (!(data[i]&127) && data[i] < 32)
                        data[i]  = '?';
        }
        return i;
}
