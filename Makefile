# OpenBSD makefile
SHELL = /bin/sh

PREFIX = /usr/local
CFLAGS = -O2 -Wall -Wpedantic -Wextra
CC = cc
FLAGS = -DTERMINAL_IMG_VIEWER
LIBSPATH = -L/usr/local/lib
INCLUDES = -I/usr/local/include
LIBS = -ltls -lcrypto -lpthread -lm
SRC = src/main.c src/cert.c src/display.c src/gemini.c src/img.c src/input.c

vgmi: src/*
	${CC} ${SRC} ${FLAGS} ${CFLAGS} ${INCLUDES} ${LIBSPATH} ${LIBS} -o $@

install:
	cp vgmi ${PREFIX}/bin
	chmod 755 ${PREFIX}/bin/vgmi

uninstall:
	rm ${PREFIX}/bin/vgmi

clean:
	rm -f vgmi
