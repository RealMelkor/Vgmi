SHELL = /bin/sh

PREFIX = /usr
CFLAGS = -O2 -Wall -Wpedantic -Wextra
CC = cc
LIBS = -ltls -lcrypto -lbsd

vgmi: src
	${CC} $(wildcard src/*.c) ${CFLAGS} ${INCLUDES} ${LIBSPATH} ${LIBS} -o $@

install:
	cp vgmi ${PREFIX}/bin
	chmod 755 ${PREFIX}/bin/vgmi

uninstall:
	rm ${PREFIX}/bin/vgmi

clean:
	rm -f vgmi
