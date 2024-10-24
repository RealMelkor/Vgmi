# Linux makefile
SHELL = /bin/sh

PREFIX = /usr
CC = cc

CFLAGS = -ansi -std=c89 -O2 -Wall -Wpedantic -Wextra \
		-I./include -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE
LDFLAGS = -s -L./lib -ltls -lssl -lcrypto -lm -lpthread -ldl

FLAGS = -DENABLE_SECCOMP_FILTER #-DENABLE_PKEY

OS := $(shell uname -s)
ifeq ($(OS),SunOS) # Solaris, Illumos
	CFLAGS=-I./include -Wall -Wextra -pedantic -O2 -Wformat-truncation=0
	LDFLAGS=-s -L./lib -L/usr/local/lib -lm -ltls -lssl -lcrypto -lpthread -lsocket
	CC=gcc
endif
ifeq ($(OS),Darwin) # MacOS
	CFLAGS = -O2 -Wall -Wpedantic -Wextra -I./include
endif

# Uncomment for GPM support on Linux
#LDFLAGS+=-lgpm
#FLAGS+=-DENABLE_GPM

SRC_DIR = src
OBJ_DIR = obj
HEADERS = $(wildcard $(SRC_DIR)/*.h)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(wildcard $(SRC_DIR)/*.c))

vgmi: ${OBJS} obj/stb_image.o
	${CC} -o $@ obj/stb_image.o ${OBJS} ${LDFLAGS}

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(HEADERS)
	mkdir -p obj
	$(CC) $(FLAGS) $(CFLAGS) -c $< -o $@

obj/stb_image.o: stb_image/stb_image.c
	mkdir -p obj
	${CC} -O2 -c -o obj/stb_image.o -I./include $<

install:
	cp vgmi ${PREFIX}/bin
	chmod 755 ${PREFIX}/bin/vgmi

uninstall:
	rm ${PREFIX}/bin/vgmi

clean:
	rm -r vgmi ${OBJ_DIR}
