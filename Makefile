SHELL = /bin/sh

PREFIX = /usr
CFLAG = -O2 -Wall -Wpedantic -Wextra
CC = cc
LIBS = -ltls -lcrypto

# FreeBSD
#PREFIX = /usr/local
#LIBSPATH = -L/usr/local/lib
#INCLUDES = -I/usr/local/include

vgmi: src
	${CC} $(wildcard src/*.c) ${CFLAGS} ${INCLUDES} ${LIBSPATH} ${LIBS} -o $@

install:
	cp vgmi ${PREFIX}/bin
	chmod 755 ${PREFIX}/bin/vgmi

uninstall:
	rm ${PREFIX}/bin/vgmi

clean:
	rm -f vgmi
