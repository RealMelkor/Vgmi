# Linux makefile
SHELL = /bin/sh

PREFIX = /usr
CC = cc

CFLAGS = -O2 -Wall -Wpedantic -Wextra -Wformat-truncation=0 -I./include
LDFLAGS = -s -L./lib -ltls -lssl -lcrypto -lm -lpthread -lanl -ldl -lpthread
# MUSL
#CFLAGS = -O2 -Wall -Wpedantic -Wextra -Wformat-truncation=0 -I./include -D__MUSL__
#LDFLAGS = -s -static -L./lib -ltls -lssl -lcrypto -lm -lpthread
# MacOS
#CFLAGS = -O2 -Wall -Wpedantic -Wextra -I./include
#LDFLAGS = -s -L./lib -ltls -lssl -lcrypto -lm -lpthread -ldl

FLAGS = -DTERMINAL_IMG_VIEWER -DHIDE_HOME

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
	rm -rf vgmi ${OBJS}
