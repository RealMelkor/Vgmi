#!/bin/sh

download=wget
if ! hash $download 2>/dev/null; then # FreeBSD
        download=fetch
fi
if [ "$(uname)" = "OpenBSD" ] ; then # OpenBSD
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
	if command -v sha256 > /dev/null # OpenBSD, FreeBSD
	then
		actual_hash="$(sha256 $file | rev | cut -d ' ' -f 1 | rev)"
		return 1
	fi
	if command -v shasum > /dev/null # NetBSD, MacOS
	then
		actual_hash="$(shasum -a 256 $file | cut -d ' ' -f 1)"
		return 1
	fi
	if command -v sha256sum > /dev/null # Linux, Illumos
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

# LibreSSL 4.1.0
# OpenBSD already has libressl
if [ "$(uname)" != "OpenBSD" ] ;
then
	ssl_version="4.1.0"
	h="0f71c16bd34bdaaccdcb96a5d94a4921bfb612ec6e0eba7a80d8854eefd8bb61"
	check_hash $h "https://ftp.openbsd.org/pub/OpenBSD/LibreSSL/libressl-$ssl_version.tar.gz"
	tar -zxf libressl-$ssl_version.tar.gz
	cd libressl-$ssl_version
	if [ "$(uname)" = "SunOS" ] ;
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

# Termbox2 2.5.0
h="b40eb22acee787b143b1ce5fd0510809e87413697d9f53af9476a8bb59149a98"
check_hash $h "https://raw.githubusercontent.com/termbox/termbox2/ed68bcc27ef4389c6f32b8229fbf234242d23a6a/termbox2.h"
cp termbox2.h ../include/termbox.h

# stb_image 2.30
h="594c2fe35d49488b4382dbfaec8f98366defca819d916ac95becf3e75f4200b3"
check_hash $h "https://raw.githubusercontent.com/nothings/stb/013ac3beddff3dbffafd5177e7972067cd2b5083/stb_image.h"
cp stb_image.h ../include/

cd ../
if [ "$(uname)" = "SunOS" ] ;
then
	gmake
	exit
fi
make
