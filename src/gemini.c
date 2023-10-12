/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#include <stdlib.h>
#include "macro.h"
#include "error.h"
#include "strlcpy.h"
#include "strnstr.h"
#include "gemtext.h"

int gemini_iserror(int status) {
	return status < 10 || status >= 40;
}

int gemini_isredirect(int status) {
	return status >= 30 && status < 40;
}

int gemini_isinput(int status) {
	return status == GMI_INPUT || status == GMI_SECRET;
}

int gemini_status_string(int status, char *out, size_t length) {
	switch (status) {
	case 10: return strlcpy(out, "Input", length);
	case 11: return strlcpy(out, "Sensitive input", length);
	case 20: return strlcpy(out, "Success", length);
	case 30: return strlcpy(out, "Temporary redirect", length);
	case 31: return strlcpy(out, "Permanent redirect", length);
	case 40: return strlcpy(out, "Temporary failure", length);
	case 41: return strlcpy(out, "Server unvailable", length);
	case 42: return strlcpy(out, "CGI error", length);
	case 43: return strlcpy(out, "Proxy error", length);
	case 44: return strlcpy(out, "Slow down", length);
	case 50: return strlcpy(out, "Permanent failure", length);
	case 51: return strlcpy(out, "Not found", length);
	case 52: return strlcpy(out, "Gone", length);
	case 53: return strlcpy(out, "Proxy request refused", length);
	case 59: return strlcpy(out, "Bad request", length);
	case 60: return strlcpy(out, "Client certificate required", length);
	case 61: return strlcpy(out, "Certificate not authorised", length);
	case 62: return strlcpy(out, "Certificate not valid", length);
	}
	return -1;
}

int gemini_status(const char *data, size_t length, char *meta, size_t len,
			int *code) {

	const char *ptr;
	char buf[64], *end;
	int i;

	ptr = strnstr(data, "\r\n", length);
	if (!ptr) return ERROR_NO_CRLN;
	length = ptr - data;

	ptr = strnstr(data, " ", length);
	if (!ptr) return ERROR_INVALID_METADATA;
	if (length - (ptr - data) < 1) return ERROR_INVALID_STATUS;
	if ((size_t)(ptr - data) >= sizeof(buf)) return ERROR_INVALID_STATUS;
	strlcpy(buf, data, ptr - data + 1);
	i = atoi(buf);
	if (!i) return ERROR_INVALID_STATUS;

	strlcpy(meta, ptr + 1, len);
	end = strnstr(meta, "\r\n", len);
	if (end) *end = '\0';

	*code = i;
	return 0;
}
