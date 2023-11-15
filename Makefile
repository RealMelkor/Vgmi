SHELL = /bin/sh

CC=cc
PREFIX=/usr/local
CFLAGS=-I./include -I/usr/local/include -ansi -Wall -Wextra -std=c89 \
       -pedantic -O2 -D_POSIX_C_SOURCE=200809L
LDFLAGS=-s -L/usr/local/lib -lm -ltls -lcrypto -lssl -lpthread

# uncomment to build on Illumos
#CFLAGS=-Wall -Wextra -pedantic -O2 -Wformat-truncation=0
#LDFLAGS=-s -L/usr/local/lib -lm -ltls -lcrypto -lssl -lpthread -lsocket
#CC=gcc

build: src/*
	${CC} -O2 -c -o stb_image/stbimage.o -I./include \
		stb_image/libstbimage.c
	${CC} ${CFLAGS} src/*.c stb_image/stbimage.o -o vgmi ${LDFLAGS}

install:
	cp vgmi ${PREFIX}/bin/
	chmod 755 ${PREFIX}/bin/vgmi

uninstall:
	rm ${PREFIX}/bin/vgmi

clean:
	rm -f vgmi
