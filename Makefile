# NetBSD and OpenBSD makefile
SHELL = /bin/sh

PREFIX = /usr/local
CFLAGS = -O2 -Wall -Wpedantic -Wextra
CC = cc
FLAGS = -DTERMINAL_IMG_VIEWER -DHIDE_HOME
LIBSPATH = -L./lib -L/usr/local/lib -L/usr/pkg/lib
INCLUDES = -I./include -I/usr/local/include -I/usr/pkg/include
LIBS = -s -static -ltls -lcrypto -lssl -lpthread -lm
SRC = src/main.c src/cert.c src/display.c src/gemini.c src/img.c src/input.c src/wcwidth.c src/sandbox.c src/str.c src/url.c src/punycode.c src/xdg.c

vgmi: src/*
	${CC} ${SRC} ${FLAGS} ${CFLAGS} ${INCLUDES} ${LIBSPATH} ${LIBS} -o $@

install:
	cp vgmi ${PREFIX}/bin
	chmod 755 ${PREFIX}/bin/vgmi

uninstall:
	rm ${PREFIX}/bin/vgmi

clean:
	rm -f vgmi
