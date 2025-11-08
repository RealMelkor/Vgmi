/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@rmf-dev.com>
 */
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <tls.h>

#include "strscpy.h"
#include "macro.h"
#include "error.h"

#define ERROR_MASK 0xFFFF

char error_tls[1024];

int error_string(int error, char *out, size_t len) {
	const char *ptr;
	switch (error & ERROR_MASK) {
	case ERROR_MEMORY_FAILURE:
		strscpy(out, "Memory failure", len);
		break;
	case ERROR_ERRNO:
		strscpy(out, strerror(errno), len);
		break;
	case ERROR_UNKNOWN_PROTOCOL:
		strscpy(out, "Unknown protocol", len);
		break;
	case ERROR_UNSUPPORTED_PROTOCOL:
		strscpy(out, "Protocol not supported", len);
		break;
	case ERROR_UNKNOWN_HOST:
		strscpy(out, "Unknown host", len);
		break;
	case ERROR_TLS_FAILURE:
		strscpy(out, error_tls, len);
		break;
	case ERROR_GETADDRINFO:
		ptr = gai_strerror(-((error & 0xFFFF0000) >> 16));
		if (!errno) {
			snprintf(out, len, "DNS: %s", ptr);
			break;
		}
		snprintf(out, len, "DNS: %s (%s)", ptr, strerror(errno));
		break;
	case ERROR_INVALID_DATA:
		strscpy(out, "Invalid response", len);
		break;
	case ERROR_INVALID_ARGUMENT:
		strscpy(out, "Invalid argument", len);
		break;
	case ERROR_INVALID_METADATA:
		strscpy(out, "No metadata", len);
		break;
	case ERROR_INVALID_ADDRESS:
		strscpy(out, "Invalid address", len);
		break;
	case ERROR_INVALID_STATUS:
		strscpy(out, "Invalid status", len);
		break;
	case ERROR_NO_CRLN:
		strscpy(out, "Invalid response", len);
		break;
	case ERROR_TERMBOX_FAILURE:
		strscpy(out, "Failed initializing TUI mode", len);
		break;
	case ERROR_TOO_MANY_REDIRECT:
		strscpy(out, "Too many redirects", len);
		break;
	case ERROR_INVALID_URL:
		strscpy(out, "Invalid URL", len);
		break;
	case ERROR_CERTIFICATE_MISMATCH:
		strscpy(out, "The server certificate changed", len);
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
		strscpy(out, "The server response is too large", len);
		break;
	case ERROR_XDG:
		strscpy(out, "Unable to open the link", len);
		break;
	case ERROR_SOCKET_CONNECTION:
		strscpy(out, "Failed to connect to remote server", len);
		break;
	case ERROR_SOCKET_CREATION:
		strscpy(out, "Failed to create socket", len);
		break;
	case ERROR_PATH_TOO_LONG:
		strlcpy(out, "Path too long", len);
		break;
	default:
		snprintf(out, len, "Unknown error (%d)", error);
		return -1;
	}
	return 0;
}
