#ifdef __APPLE__
#undef strlcpy
#endif
size_t strlcpy(char *dst, const char *src, size_t dsize);
