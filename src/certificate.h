/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@rmf-dev.com>
 */

int certificate_getpath(const char* host, char* crt, size_t crt_len,
				char* key, size_t key_len);
int certificate_create(char* host, char* error, int errlen);
