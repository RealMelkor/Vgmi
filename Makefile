SHELL = /bin/sh

PREFIX = /usr
CFLAG = -O2 -Wall
CC = cc
LIBS = -ltls

# FreeBSD
#PREFIX = /usr/local
#LIBSPATH = -L/usr/local/lib
#INCLUDES = -I/usr/local/include

vgmi: main.c gemini.c
	${CC} -c ${CFLAGS} *.c ${INCLUDES}
	mv *.o obj/
	${CC} $(wildcard obj/*.o) ${LIBSPATH} ${LIBS} -o $@

install:
	cp vgmi ${PREFIX}/bin
	chmod 755 ${PREFIX}/bin/vgmi

uninstall:
	rm ${PREFIX}/bin/vgmi

clean:
	rm -f vgmi
