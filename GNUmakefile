# Linux makefile
SHELL = /bin/sh

PREFIX = /usr
CC = cc

CFLAGS = -ansi -std=c89 -O2 -Wall -Wpedantic -Wextra \
		-I./include -D_POSIX_C_SOURCE=200809L
LDFLAGS = -s -L./lib -ltls -lssl -lcrypto -lm -lpthread -ldl
# MacOS
#CFLAGS = -O2 -Wall -Wpedantic -Wextra -I./include

FLAGS = -DENABLE_SECCOMP_FILTER

SRC = $(wildcard src/*.c)
OBJ = ${SRC:.c=.o}
OBJS = $(subst src,obj,$(OBJ))

.c.o:
	mkdir -p obj
	${CC} -c ${CFLAGS} ${FLAGS} $< -o $(subst src,obj,${<:.c=.o})

vgmi: ${OBJ}
	${CC} -O2 -c -o stb_image/stbimage.o -I./include \
		stb_image/libstbimage.c
	${CC} -o $@ stb_image/stbimage.o ${OBJS} ${LDFLAGS}

install:
	cp vgmi ${PREFIX}/bin
	chmod 755 ${PREFIX}/bin/vgmi

uninstall:
	rm ${PREFIX}/bin/vgmi

clean:
	rm vgmi ${OBJS}
