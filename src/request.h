/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
enum {
	STATE_ONGOING,
	STATE_COMPLETED,
	STATE_CANCELED,
	STATE_ABANDONED,
	STATE_FAILED,
	STATE_ENDED
};

struct secure;
struct rect;

struct request {
	unsigned short port;
	int protocol;
	char url[MAX_URL];
	char name[MAX_URL];
	char meta[MAX_URL];
	void *addr;
	char *data;
	size_t length;
	int state;
	int error;
	int scroll;
	int selected;
	int status;
	struct request *next;
	struct page page;
	void *thread;
};

int request_process(struct request*, struct secure*, const char*);
int request_cancel(struct request*);
int request_scroll(struct request*, int, struct rect);
int request_follow(struct request*, const char*, char*, size_t);
void request_free(struct request*);
void request_free_ref(struct request req);
