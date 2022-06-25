#!/bin/sh
mkdir include
mkdir build
mkdir lib
cd build
git clone https://github.com/termbox/termbox2.git
git clone https://github.com/nothings/stb.git
wget https://causal.agency/libretls/libretls-3.5.2.tar.gz
tar -xf libretls-3.5.2.tar.gz
cp termbox2/termbox.h ../include/
cp stb/stb_image.h ../include/
cd libretls-3.5.2
./configure
make -j 4
cp include/tls.h ../../include/
cp -R include/compat ../../include/
cp .libs/libtls.a ../../lib
cd ../../
make
