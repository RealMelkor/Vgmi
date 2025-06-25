/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@rmf-dev.com>
 */
struct secure;
struct secure *secure_new(void);
int secure_send(struct secure *secure, const char *data, size_t len);
int secure_read(struct secure *secure, char **data, size_t *len);
int secure_connect(struct secure *secure, struct request request);
int secure_free(struct secure *secure);
