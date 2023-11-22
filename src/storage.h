/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */

FILE* storage_fopen(const char *name, const char *mode);
FILE* storage_download_fopen(const char *name, const char *mode);
int storage_download_exist(const char *name);
int storage_path(char *out, size_t length);
int storage_download_path(char *out, size_t length);
int storage_open(const char *name, int flags, int mode);
int storage_read(const char*, char*, size_t, size_t*);
int storage_init();
int storage_close();
extern int storage_fd;
