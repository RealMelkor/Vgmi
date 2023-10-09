/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#define LTS 0
#define MAJOR "2"
#define MINOR "0"
#if LTS
#define VERSION MAJOR "." MINOR " LTS"
#else
#define VERSION MAJOR "." MINOR " (" __DATE__ " " __TIME__ ")"
#endif

int about_parse(struct request *request);
