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
mkdir lib
wget https://causal.agency/libretls/libretls-3.5.2.tar.gz
tar -zxf libretls-3.5.2.tar.gz
cd libretls-3.5.2
./configure
make -j 4
cp include/tls.h ../../include/
cp -R include/compat ../../include/
cp .libs/libtls.a ../../lib
cd ../../
else
cd ../
fi
make
