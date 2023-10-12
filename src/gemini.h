/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
int gemini_status(const char*, size_t, char*, size_t, int*);
int gemini_status_string(int status, char *out, size_t length);
int gemini_iserror(int status);
int gemini_isredirect(int status);
int gemini_isinput(int status);
