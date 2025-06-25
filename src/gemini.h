/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@rmf-dev.com>
 */
#define GMI_INPUT	10
#define GMI_SECRET	11
#define GMI_SUCCESS	20

int gemini_status(const char*, size_t, char*, size_t, int*);
int gemini_status_string(int status, char *out, size_t length);
int gemini_status_code(int status);
int gemini_iserror(int status);
int gemini_isredirect(int status);
int gemini_isinput(int status);
