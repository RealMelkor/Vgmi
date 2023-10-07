#include <stdlib.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "strlcpy.h"
#include "macro.h"
#include "error.h"

#define ERROR_MASK 0xFFFF

int error_string(int error, char *out, size_t len) {
	const char *ptr;
	switch (error & ERROR_MASK) {
	case ERROR_MEMORY_FAILURE:
		strlcpy(out, "Memory failure", len);
		break;
	case ERROR_GETADDRINFO:
		ptr = gai_strerror(-((error & 0xFFFF0000) >> 16));
		strlcpy(out, ptr, len);
		break;
	case ERROR_INVALID_METADATA:
		strlcpy(out, "No metadata", len);
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
	default:
		strlcpy(out, "Unknown error", len);
		return -1;
	}
	return 0;
}
