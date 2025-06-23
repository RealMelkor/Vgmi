#!/bin/sh

MAKE=make
if [ "$(uname)" = SunOS ]; then
	export CC=gcc MAKE=gmake
fi

download () {
	if command -v wget >/dev/null; then
		wget "$url" -O "$file"
	elif command -v fetch >/dev/null; then # FreeBSD
		fetch "$url"
	elif [ "$(uname)" = 'OpenBSD' ] && command -v ftp >/dev/null; then # OpenBSD
		ftp "$url"
	elif command -v curl >/dev/null; then
		curl -O "$url"
	else
		printf 'No download program found\n'
		exit 1
	fi
}

hash_file () {
	file="$1"

	if command -v sha256 >/dev/null; then # OpenBSD, FreeBSD
		hasher='sha256 -r'
	elif command -v shasum >/dev/null; then # NetBSD, MacOS, Ubuntu
		hasher='shasum -a 256'
	elif command -v sha256sum >/dev/null; then # Linux, Illumos
		hasher='sha256sum'
	else
		printf 'No sha256 program found\n'
		exit 1
	fi

	actual_hash="$($hasher "$file" | cut -d ' ' -f 1)"
}

check_hash () {
	cd "$downloads"

	expected_hash="$1"
	url="$2"
	file="$(basename $url)"
	target_file="${3:-$file}"
	local_file="${target_file:-$file}"

	# do not download if it exists locally
	if [ -f "$target_file" ]; then
		hash_file "$target_file"
		if [ "${expected_hash:-X}" = "${actual_hash:-}" ]; then
			cd - >/dev/null
			return
		fi
	fi

	download
	hash_file "$file"
	if [ "${expected_hash:-X}" != "${actual_hash:-}" ]; then
		printf "${file}: unexpected hash\n"
		printf "$actual_hash != $expected_hash\n"
		exit 1
	fi
	[ "$file" != "$target_file" ] && mv "$file" "$target_file"
	cd - >/dev/null
}

# runs from vgmi root, so include/ and lib/ are created in root folder
root=$(cd "$(dirname "$0")" && pwd -P)
cd "$root"

downloads="${root}/.dependencies"
mkdir -p 'include' "$downloads"

# LibreSSL 4.1.0
# OpenBSD already has libressl
if [ "$(uname)" != 'OpenBSD' ]; then

	build_dir="$(mktemp -d /tmp/vgmi.build_XXXXXX)" || {
		printf "could not create temporary build folder\n"
		exit 1
	}
	trap 'rm -rf "$build_dir"; exit' INT
	mkdir -p 'lib'
	cd "$build_dir"

	ssl_version='4.1.0'
	expected_hash='0f71c16bd34bdaaccdcb96a5d94a4921bfb612ec6e0eba7a80d8854eefd8bb61'
	remote_dir='https://ftp.openbsd.org/pub/OpenBSD/LibreSSL'
	lib="libressl-${ssl_version}"
	archive="${lib}.tar.gz"

	check_hash "$expected_hash" "${remote_dir}/${archive}"
	tar -zxf "${downloads}/${archive}"
	cd "$lib"
	./configure
	$MAKE -j 4
	cp include/*.h "${root}/include/"
	cp -R 'include/compat' "${root}/include/"
	cp -R 'include/openssl' "${root}/include/"
	cp 'tls/.libs/libtls.a' "${root}/lib/"
	cp 'crypto/.libs/libcrypto.a' "${root}/lib/"
	cp 'ssl/.libs/libssl.a' "${root}/lib/"
	cd "$root"
	rm -rf "$build_dir"
fi

# stb_image 2.30
expected_hash='594c2fe35d49488b4382dbfaec8f98366defca819d916ac95becf3e75f4200b3'
remote_url='https://raw.githubusercontent.com/nothings/stb/013ac3beddff3dbffafd5177e7972067cd2b5083/stb_image.h'
check_hash "$expected_hash" "$remote_url" "${downloads}/stb_image.h"
cp "${downloads}/stb_image.h" "${root}/include/stb_image.h"

cd "$root"

$MAKE -j4
