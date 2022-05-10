# Linux makefile
SHELL = /bin/sh

PREFIX = /usr
CFLAGS = -O2 -Wall -Wpedantic -Wextra -Wformat-truncation=0
FLAGS = -DTERMINAL_IMG_VIEWER
CC = cc
LIBS = -ltls -lcrypto -lbsd -lm

vgmi: src
	${CC} $(wildcard src/*.c) ${FLAGS} ${CFLAGS} ${INCLUDES} ${LIBSPATH} ${LIBS} -o $@

install:
	cp vgmi ${PREFIX}/bin
	chmod 755 ${PREFIX}/bin/vgmi

uninstall:
	rm ${PREFIX}/bin/vgmi

clean:
	rm -f vgmi
