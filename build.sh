#!/bin/sh
mkdir include
mkdir build
cd build
git clone https://github.com/termbox/termbox2.git
git clone https://github.com/nothings/stb.git
cp termbox2/termbox.h ../include/
cp stb/stb_image.h ../include/
# No need to on OpenBSD
if [ "$(uname)" != OpenBSD ] ;
then
mkdir ../lib
download=wget
if ! hash $download 2>/dev/null; then
        download=fetch
fi
if ! hash $download 2>/dev/null; then
        download=curl
if ! hash $download 2>/dev/null; then
        echo "No download program found"
        exit
fi
        download="curl -O"
fi
$download https://ftp.openbsd.org/pub/OpenBSD/LibreSSL/libressl-3.5.3.tar.gz
tar -zxf libressl-3.5.3.tar.gz
cd libressl-3.5.3
if [ "$(uname)" == SunOS ] ;
then
	CC=gcc MAKE=gmake ./configure
	gmake -j 4
else
	./configure
	make -j 4
fi
cp include/*.h ../../include/
cp -R include/compat ../../include/
cp -R include/openssl ../../include/
cp tls/.libs/libtls.a ../../lib
cp crypto/.libs/libcrypto.a ../../lib
cp ssl/.libs/libssl.a ../../lib
cd ../../
else
cd ../
fi
if [ "$(uname)" == SunOS ] ;
then
	sed -i -e "/CC/s/^#//" Makefile
	sed -i -e "/LIBS/s/^#//" Makefile
	sed -i -e "/CFLAGS/s/^#//" Makefile
fi
make
