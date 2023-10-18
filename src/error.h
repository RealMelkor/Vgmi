/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
enum {
	ERROR_NONE,
	ERROR_MEMORY_FAILURE,
	ERROR_UNKNOWN_PROTOCOL,
	ERROR_INVALID_DATA,
	ERROR_INVALID_ARGUMENT,
	ERROR_INVALID_COMMAND_NAME,
	ERROR_TERMBOX_FAILURE,
	ERROR_NULL_ARGUMENT,
	ERROR_URL_TOO_LONG,
	ERROR_INVALID_URL,
	ERROR_INVALID_PORT,
	ERROR_GETADDRINFO, /* gai_strerror */
	ERROR_INVALID_ADDRESS,
	ERROR_BUFFER_OVERFLOW,
	ERROR_SOCKET_CREATION,
	ERROR_SOCKET_CONNECTION,
	ERROR_TOO_MANY_REDIRECT,
	ERROR_TLS_FAILURE,
	ERROR_INVALID_STATUS,
	ERROR_INVALID_METADATA,
	ERROR_STORAGE_ACCESS,
	ERROR_CERTIFICATE_MISMATCH,
	ERROR_CERTIFICATE_EXPIRED,
	ERROR_CERTIFICATE_AHEAD,
	ERROR_PTHREAD,
	ERROR_SANDBOX_FAILURE,
	ERROR_NO_CRLN
};

int error_string(int error, char *out, size_t len);
extern char error_tls[1024];
