/* See LICENSE file for copyright and license details. */
#define MAX(X, Y) (X > Y ? Y : X) /* if X is greater than Y return Y */
#define STRCMP(X, Y) strncmp(X, Y, sizeof(X))
#define STARTWITH(X, Y) !strncmp(X, Y, MAX(sizeof(Y) - 1, sizeof(X)))
#define STRNLEN(X) strnlen(X, sizeof(X))
#define STRLCPY(X, Y) strlcpy(X, Y, sizeof(X))
#ifndef PATH_MAX
#define PATH_MAX 1024
#endif
