/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
int gemini_status(const char*, size_t, char*, size_t, int*);
int gemini_iserror(int status);
int gemini_isredirect(int status);
