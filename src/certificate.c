/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@rmf-dev.com>
 */
#include <stdlib.h>
#include <string.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <fcntl.h>
#ifdef __linux__
#include <sys/random.h>
#endif
#include "storage.h"
#include "strscpy.h"
#include "config.h"

int certificate_getpath(const char *host, char *crt, size_t crt_len,
				char *key, size_t key_len) {
	int len = strnlen(host, 1024);
	if (strscpy(crt, host, crt_len) >= crt_len - 4) return -1;
	if (strscpy(key, host, key_len) >= key_len - 4) return -1;
        if (strscpy(&crt[len], ".crt", crt_len - len) + len >= crt_len)
		return -1;
        if (strscpy(&key[len], ".key", key_len - len) + len >= key_len)
		return -1;
	return len + 4;
}

int certificate_create(char *host, char *error, int errlen) {

	char key[1024];
	char crt[1024];
	FILE* f = NULL;
	int fd, id, ret = 1;

	X509_NAME *name;
#ifdef USE_OPENSSL
	EVP_PKEY *pkey = EVP_RSA_gen(CERTIFICATE_BITS);
	X509 *x509 = X509_new();
#else /* LibreSSL */
	EVP_PKEY *pkey = EVP_PKEY_new();
	RSA *rsa = RSA_new();
	BIGNUM *bne = BN_new();
	X509 *x509 = X509_new();
	if (BN_set_word(bne, 65537) != 1) goto failed;
	if (RSA_generate_key_ex(rsa, config.certificateBits, bne, NULL) != 1)
		goto failed;

	EVP_PKEY_assign_RSA(pkey, rsa);
#endif

#ifdef __linux__
	if (getrandom(&id, sizeof(id), GRND_RANDOM) == -1) goto failed;
#else
	arc4random_buf(&id, sizeof(id));
#endif
	if (id < 0) id = -id;
	if (ASN1_INTEGER_set(X509_get_serialNumber(x509), id) != 1)
		goto failed;

	X509_gmtime_adj(X509_getm_notBefore(x509), 0);
	X509_gmtime_adj(X509_getm_notAfter(x509), config.certificateLifespan);

	if (X509_set_pubkey(x509, pkey) != 1) goto failed;

	name = X509_get_subject_name(x509);
	if (X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
			(unsigned char*)host, -1, -1, 0) != 1)
		goto failed;

	if (X509_set_issuer_name(x509, name) != 1) goto failed;
	if (X509_sign(x509, pkey, EVP_sha1()) == 0) goto failed;

	if (certificate_getpath(host, crt, sizeof(crt),
				key, sizeof(key)) == -1)
		goto failed;

	/* Key */
	fd = storage_open(key, O_CREAT|O_WRONLY|O_TRUNC, 0600);
	if (fd < 0) {
		snprintf(error, errlen, "Failed to open %s : %s",
				key, strerror(errno));
		goto skip_error;
	}
	f = fdopen(fd, "wb");

	if (!f) {
		snprintf(error, errlen, "Failed to write to %s : %s",
			 key, strerror(errno));
		goto skip_error;
	}
	if (PEM_write_PrivateKey(f, pkey, NULL, NULL, 0, NULL, NULL) != 1)
		goto failed;
	fclose(f);
	f = NULL;

	/* Certificate */
	fd = storage_open(crt, O_CREAT|O_WRONLY|O_TRUNC, 0600);
	if (fd < 0) {
		snprintf(error, errlen, "Failed to open %s", crt);
		goto skip_error;
	}
	f = fdopen(fd, "wb");

	if (!f) {
		snprintf(error, errlen, "Failed to write to %s", crt);
		goto skip_error;
	}
	if (PEM_write_X509(f, x509) != 1)
		goto failed;
	fclose(f);
	f = NULL;

	ret = 0;
	goto skip_error;
failed:
	snprintf(error, errlen, "Failed to generate certificate");
skip_error:
	if (f) fclose(f);
	EVP_PKEY_free(pkey);
	X509_free(x509);
#ifndef USE_OPENSSL
	BN_free(bne);
#endif
	return ret;
}
