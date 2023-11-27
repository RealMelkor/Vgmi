/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <tls.h>

#include "strlcpy.h"
#include "macro.h"
#include "error.h"

#define ERROR_MASK 0xFFFF

char error_tls[1024];

int error_string(int error, char *out, size_t len) {
	const char *ptr;
	switch (error & ERROR_MASK) {
	case ERROR_MEMORY_FAILURE:
		strlcpy(out, "Memory failure", len);
		break;
	case ERROR_ERRNO:
		strlcpy(out, strerror(errno), len);
		break;
	case ERROR_UNKNOWN_PROTOCOL:
		strlcpy(out, "Unknown protocol", len);
		break;
	case ERROR_UNSUPPORTED_PROTOCOL:
		strlcpy(out, "Protocol not supported", len);
		break;
	case ERROR_UNKNOWN_HOST:
		strlcpy(out, "Unknown host", len);
		break;
	case ERROR_TLS_FAILURE:
		strlcpy(out, error_tls, len);
		break;
	case ERROR_GETADDRINFO:
		ptr = gai_strerror(-((error & 0xFFFF0000) >> 16));
		strlcpy(out, ptr, len);
		break;
	case ERROR_INVALID_DATA:
		strlcpy(out, "Invalid response", len);
		break;
	case ERROR_INVALID_ARGUMENT:
		strlcpy(out, "Invalid argument", len);
		break;
	case ERROR_INVALID_METADATA:
		strlcpy(out, "No metadata", len);
		break;
	case ERROR_INVALID_ADDRESS:
		strlcpy(out, "Invalid address", len);
		break;
	case ERROR_INVALID_STATUS:
		strlcpy(out, "Invalid status", len);
		break;
	case ERROR_NO_CRLN:
		strlcpy(out, "Invalid response", len);
		break;
	case ERROR_TERMBOX_FAILURE:
		strlcpy(out, "Failed initializing TUI mode", len);
		break;
	case ERROR_TOO_MANY_REDIRECT:
		strlcpy(out, "Too many redirects", len);
		break;
	case ERROR_INVALID_URL:
		strlcpy(out, "Invalid URL", len);
		break;
	case ERROR_CERTIFICATE_MISMATCH:
		strlcpy(out, "The server certificate changed", len);
		break;
	case ERROR_STORAGE_ACCESS:
		snprintf(out, len, "Failed to access disk storage (%s)",
				strerror(errno));
		break;
	case ERROR_SANDBOX_FAILURE:
		snprintf(out, len, "Failed to apply sandboxing (%s)",
				strerror(errno));
		break;
	case ERROR_RESPONSE_TOO_LARGE:
		strlcpy(out, "The server response is too large", len);
		break;
	case ERROR_XDG:
		strlcpy(out, "Unable to open the link", len);
		break;
	default:
		snprintf(out, len, "Unknown error (%d)", error);
		return -1;
	}
	return 0;
}
