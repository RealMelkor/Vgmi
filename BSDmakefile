# FreeBSD makefile
SHELL = /bin/sh

PREFIX = /usr/local
CC = cc
CFLAGS = -O2 -Wall -Wpedantic -Wextra -I/usr/local/include
LDFLAGS = -L/usr/local/lib -ltls -lcrypto -lpthread -lm
FLAGS = -DTERMINAL_IMG_VIEWER
SRC = ${:!ls src/*.c!}
OBJ = ${SRC:.c=.o}

.c.o:
	${CC} -c ${CFLAGS} ${FLAGS} $< -o ${<:.c=.o}

vgmi: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

install:
	cp vgmi ${PREFIX}/bin
	chmod 755 ${PREFIX}/bin/vgmi

uninstall:
	rm ${PREFIX}/bin/vgmi

clean:
	rm -f vgmi
	rm ${OBJ}
