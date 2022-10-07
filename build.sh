#!/bin/sh

download=wget
if ! hash $download 2>/dev/null; then # FreeBSD
        download=fetch
fi
if ! hash $download 2>/dev/null; then # OpenBSD
        download=ftp
fi
if ! hash $download 2>/dev/null; then
        download=curl
if ! hash $download 2>/dev/null; then
        echo "No download program found"
        exit
fi
        download="curl -O"
fi

hash_file () {
	file=$1
	if command -v sha256 > /dev/null # OpenBSD
	then
		actual_hash="$(sha256 $file | rev | cut -d ' ' -f 1 | rev)"
		return 1
	fi
	if command -v shasum > /dev/null # NetBSD
	then
		actual_hash="$(shasum -a 256 $file | cut -d ' ' -f 1)"
		return 1
	fi
	if command -v sha256sum > /dev/null # Linux
	then
		actual_hash="$(sha256sum $file | cut -d ' ' -f 1)"
		return 1
	fi
	echo "No sha256 program found"
	exit
}

check_hash () {
	expected_hash=$1
	url=$2
	$download $url
	hash_file "$(echo $url | rev | cut -d '/' -f 1 | rev)"
	if [ "$actual_hash" != "$expected_hash" ]; then
		echo "$file: unexpected hash"
		echo "$actual_hash != $expected_hash"
		exit
	fi
}

mkdir -p include
mkdir -p build
cd build
mkdir -p ../lib

# LibreSSL 3.5.3
# OpenBSD already has libressl
if [ "$(uname)" != OpenBSD ] ;
then
	h="3ab5e5eaef69ce20c6b170ee64d785b42235f48f2e62b095fca5d7b6672b8b28"
	check_hash $h "https://ftp.openbsd.org/pub/OpenBSD/LibreSSL/libressl-3.5.3.tar.gz"
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
	cd ../
fi

# Termbox2 2.0.0
h="da2e8c2f30d784e9b3cdfea4332257beef471976556b9ddb6e994ca5adf389af"
check_hash $h "https://github.com/termbox/termbox2/archive/refs/tags/v2.0.0.zip"
unzip v2.0.0.zip
cp termbox2-2.0.0/termbox.h ../include/

# stb_image 2.27
h="91f435e0fc6a620018b878b9859c74dff60d28046f87e649191ad6f35a98c722"
check_hash $h "https://raw.githubusercontent.com/nothings/stb/master/stb_image.h"
cp stb_image.h ../include/

cd ../
if [ "$(uname)" == SunOS ] ;
then
	sed -i -e "/CC/s/^#//" Makefile
	sed -i -e "/LIBS/s/^#//" Makefile
	sed -i -e "/CFLAGS/s/^#//" Makefile
fi
make
