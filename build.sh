#!/bin/sh

download=wget
if ! hash $download 2>/dev/null; then # FreeBSD
        download=fetch
fi
if [ "$(uname)" == OpenBSD ] ; then # OpenBSD
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

# LibreSSL 3.9.1
# OpenBSD already has libressl
if [ "$(uname)" != OpenBSD ] ;
then
	mkdir -p lib
	mkdir -p build
	cd build
	h="6da0b954695f7ee62b03f64200a8a4f02af93717b60cce04ab6c8df262c07a51"
	ssl_version="3.9.1"
	remote_dir="https://ftp.openbsd.org/pub/OpenBSD/LibreSSL/"
	check_hash $h "$remote_dir/libressl-$ssl_version.tar.gz"
	tar -zxf libressl-$ssl_version.tar.gz
	cd libressl-$ssl_version
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
fi

# stb_image 2.28
h="38e08c1c5ab8869ae8d605ddaefa85ad3fea24a2964fd63a099c0c0f79c70bcc"
check_hash $h "https://raw.githubusercontent.com/nothings/stb/5736b15f7ea0ffb08dd38af21067c314d6a3aae9/stb_image.h"
mv stb_image.h include/

if [ "$(uname)" == SunOS ] ;
then
	sed -i -e "/CC/s/^#//" Makefile
	sed -i -e "/LDFLAGS/s/^#//" Makefile
	sed -i -e "/CFLAGS/s/^#//" Makefile
fi
if [ "$(uname)" == Darwin ] ;
then
	sed -i -e "/CFLAGS/s/^#//" GNUmakefile
fi
make -j4
