# Linux makefile
SHELL = /bin/sh

PREFIX = /usr
CC = cc

CFLAGS = -O2 -Wall -Wpedantic -Wextra -Wformat-truncation=0 -I./include
LDFLAGS = -L./lib -ltls -lcrypto -lm -lpthread -lssl -lanl
# MUSL
#CFLAGS = -O2 -Wall -Wpedantic -Wextra -Wformat-truncation=0 -I./include -D__MUSL__
#LDFLAGS = -static -L./lib -ltls -lcrypto -lm -lpthread -lssl

FLAGS = -DTERMINAL_IMG_VIEWER

SRC = $(wildcard src/*.c)
OBJ = ${SRC:.c=.o}
OBJS = $(subst src,obj,$(OBJ))

.c.o:
	mkdir -p obj
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
