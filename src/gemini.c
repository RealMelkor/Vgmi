#include <stdlib.h>
#include "error.h"
#include "strlcpy.h"
#include "strnstr.h"

int gemini_iserror(int status) {
	return status < 10 || status >= 40;
}

int gemini_isredirect(int status) {
	return status >= 30 && status < 40;
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
