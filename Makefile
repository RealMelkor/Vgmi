SHELL = /bin/sh

CC=cc
PREFIX=/usr/local
CFLAGS=-ansi -Wall -Wextra -std=c89 -pedantic -O2 -D_POSIX_C_SOURCE=200809L
LIBS=-s -lm -ltls

# uncomment to build on Illumos
#CFLAGS=-Wall -Wextra -pedantic -O2 -Wformat-truncation=0
#CC=gcc

build: src/*
	${CC} ${CFLAGS} src/*.c -o vgmi ${LIBS}

install:
	cp vgmi ${PREFIX}/bin/
	chmod 755 ${PREFIX}/bin/vgmi

uninstall:
	rm ${PREFIX}/bin/vgmi

clean:
	rm -f vgmi
