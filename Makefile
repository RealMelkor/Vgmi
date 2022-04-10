SHELL = /bin/sh

PREFIX = /usr
CFLAG = -O2 -Wall -Wpedantic -Wextra
CC = cc
LIBS = -ltls -lcrypto

ifeq ($(shell uname -s),FreeBSD)
bsd = yes
endif

ifeq ($(shell uname -s),OpenBSD)
bsd = yes
endif

ifeq ($(shell uname -s),NetBSD)
bsd = yes
endif

ifdef bsd
PREFIX = /usr/local
LIBSPATH = -L/usr/local/lib
INCLUDES = -I/usr/local/include
LIBS = -ltls -lcrypto -lpthread -luv
endif

vgmi: src
	${CC} $(wildcard src/*.c) ${CFLAGS} ${INCLUDES} ${LIBSPATH} ${LIBS} -o $@

install:
	cp vgmi ${PREFIX}/bin
	chmod 755 ${PREFIX}/bin/vgmi

uninstall:
	rm ${PREFIX}/bin/vgmi

clean:
	rm -f vgmi
