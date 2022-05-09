SHELL = /bin/sh

PREFIX = /usr/local
CFLAGS = -O2 -Wall -Wpedantic -Wextra
CC = cc
FLAGS = -DTERMINAL_IMG_VIEWER
LIBSPATH = -L/usr/local/lib
INCLUDES = -I/usr/local/include
LIBS = -ltls -lcrypto -lpthread -lm

vgmi: src
	${CC} ${:!ls src/*.c!} ${FLAGS} ${CFLAGS} ${INCLUDES} ${LIBSPATH} ${LIBS} -o $@

install:
	cp vgmi ${PREFIX}/bin
	chmod 755 ${PREFIX}/bin/vgmi

uninstall:
	rm ${PREFIX}/bin/vgmi

clean:
	rm -f vgmi
