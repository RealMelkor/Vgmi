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
wget https://ftp.openbsd.org/pub/OpenBSD/LibreSSL/libressl-3.5.3.tar.gz
tar -zxf libressl-3.5.3.tar.gz
cd libressl-3.5.3
./configure
make -j 4
cp include/*.h ../../include/
cp -R include/compat ../../include/
cp -R include/ssl ../../include/
cp tls/.libs/libtls.a ../../lib
cp crypto/.libs/libcrypto.a ../../lib
cp ssl/.libs/libssl.a ../../lib
cd ../../
else
cd ../
fi
make
