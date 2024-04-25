# Linux makefile
SHELL = /bin/sh

PREFIX = /usr
CC = cc

CFLAGS = -ansi -std=c89 -O2 -Wall -Wpedantic -Wextra \
		-I./include -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE
LDFLAGS = -s -L./lib -ltls -lssl -lcrypto -lm -lpthread -ldl
# MacOS
#CFLAGS = -O2 -Wall -Wpedantic -Wextra -I./include

FLAGS = -DENABLE_SECCOMP_FILTER

# Uncomment for GPM support on Linux
#LDFLAGS+=-lgpm
#FLAGS+=-DENABLE_GPM

SRC = $(wildcard src/*.c)
OBJ = ${SRC:.c=.o}
OBJS = $(subst src,obj,$(OBJ))

.c.o:
	mkdir -p obj
	${CC} -c ${CFLAGS} ${FLAGS} $< -o $(subst src,obj,${<:.c=.o})

vgmi: ${OBJ}
	${CC} -O2 -c -o obj/stb_image.o -I./include stb_image/stb_image.c
	${CC} -o $@ obj/stb_image.o ${OBJS} ${LDFLAGS}

install:
	cp vgmi ${PREFIX}/bin
	chmod 755 ${PREFIX}/bin/vgmi

uninstall:
	rm ${PREFIX}/bin/vgmi

clean:
	rm vgmi ${OBJS}
