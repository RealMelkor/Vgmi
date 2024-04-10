/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#include <stdlib.h>
#include "macro.h"
#include "error.h"
#include "strlcpy.h"
#include "strnstr.h"
#include "gemini.h"

static const char *status_str[] = {
	0,0,0,0,0,0,0,0,0,0,
	"Input",
	"Sensitive input",
	0,0,0,0,0,0,0,0,
	"Success",
	0,0,0,0,0,0,0,0,0,
	"Temporary redirect",
	"Permanent redirect",
	0,0,0,0,0,0,0,0,
	"Temporary failure",
	"Server unvailable",
	"CGI error",
	"Proxy error",
	"Slow down",
	0,0,0,0,0,
	"Permanent failure",
	"Not found",
	"Gone",
	"Proxy request refused",
	0,0,0,0,0,
	"Bad request",
	"Client certificate required",
	"Certificate not authorised",
	"Certificate not valid",
};

int gemini_status_code(int status) {
	if (status < 10 || status >= 70) return -1;
	if (status >= (signed)LENGTH(status_str) || !status_str[status])
		return status - (status % 10);
	return status;
}

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
	if ((unsigned)status >= LENGTH(status_str) || !status_str[status])
		return -1;
	return strlcpy(out, status_str[status], length);
}
