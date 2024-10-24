SHELL = /bin/sh

PREFIX = /usr/local
CC = cc
CFLAGS = -O2 -Wall -Wpedantic -Wextra -I/usr/local/include -I./include
LDFLAGS = -s -L./lib -L/usr/local/lib -ltls -lssl -lcrypto -lpthread -lm
FLAGS =
HEADERS != (ls src/*.h)
SRC != (ls src/*.c)
OBJ = ${SRC:.c=.o}

OS != (uname -s)
.if ${OS} == "FreeBSD"
LDFLAGS += -lcasper -lcap_net
.endif

.c.o: ${HEADERS}
	${CC} -c ${CFLAGS} ${FLAGS} $< -o ${<:.c=.o}

vgmi: ${OBJ} stb_image/stb_image.o
	${CC} -o $@ ${OBJ} stb_image/stb_image.o ${LDFLAGS}

stb_image/stb_image.o: stb_image/stb_image.c
	${CC} -O2 -c -o stb_image/stb_image.o -I./include stb_image/stb_image.c

install:
	cp vgmi ${PREFIX}/bin/
	chmod 755 ${PREFIX}/bin/vgmi

uninstall:
	rm ${PREFIX}/bin/vgmi

clean:
	rm -f vgmi
	rm ${OBJ}
