# Linux makefile
SHELL = /bin/sh

PREFIX = /usr
CFLAGS = -O2 -Wall -Wpedantic -Wextra -Wformat-truncation=0
LDFLAGS = -ltls -lcrypto -lbsd -lm
FLAGS = -DTERMINAL_IMG_VIEWER -DMEM_CHECK
CC = cc
SRC = $(wildcard src/*.c)
OBJ = ${SRC:.c=.o}
OBJS = $(subst src,obj,$(OBJ))

.c.o:
#	mkdir -p obj
	${CC} -c ${CFLAGS} ${FLAGS} $< -o $(subst src,obj,${<:.c=.o})

vgmi: ${OBJ}
	${CC} -o $@ ${OBJS} ${LDFLAGS}

install:
	cp vgmi ${PREFIX}/bin
	chmod 755 ${PREFIX}/bin/vgmi

uninstall:
	rm ${PREFIX}/bin/vgmi

clean:
	rm vgmi ${OBJS}
