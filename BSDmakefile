SHELL = /bin/sh

PREFIX = /usr/local
CFLAG = -O2 -Wall -Wpedantic -Wextra
CC = cc
LIBSPATH = -L/usr/local/lib
INCLUDES = -I/usr/local/include
LIBS = -ltls -lcrypto -lpthread

vgmi: src
	${CC} ${:!ls src/*.c!} ${CFLAGS} ${INCLUDES} ${LIBSPATH} ${LIBS} -o $@

install:
	cp vgmi ${PREFIX}/bin
	chmod 755 ${PREFIX}/bin/vgmi

uninstall:
	rm ${PREFIX}/bin/vgmi

clean:
	rm -f vgmi
