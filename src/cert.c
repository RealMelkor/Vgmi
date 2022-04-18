#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <stdio.h>
#include <tls.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <string.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/random.h>
#ifdef __linux__
#include <bsd/string.h>
#endif
#include "gemini.h"

int gethomefolder(char* path, size_t len) {
	struct passwd *pw = getpwuid(geteuid());
	if (!pw) return 0;
	int length = strlcpy(path, pw->pw_dir, len);
       	if (length >= len) return -1;
	return length;
}

int getcachefolder(char* path, size_t len) {
	int length = gethomefolder(path, len);
	if (length == -1) return -1;
	if (length >= len) return -1;
	length += strlcpy(&path[length], "/.cache/vgmi", len - length);
        if (length >= len) return -1;
        return length;
}

int cert_getpath(char* host, char* crt, int crt_len, char* key, int key_len) {
	char path[1024];
        int len = getcachefolder(path, sizeof(path));
	if (len < 1) {
		snprintf(client.error, sizeof(client.error),
		"Failed to get cache directory");
		return -1;
	}
        struct stat _stat;
        if (stat(path, &_stat) && mkdir(path, 0700)) {
		snprintf(client.error, sizeof(client.error),
		"Failed to create cache directory at %s", path);
		return -1;
        }
	path[len] = '/';
	path[++len] = '\0';
        len += strlcpy(&path[len], host, sizeof(path) - len);
	if (len >= sizeof(path))
		goto getpath_overflow;
	if (strlcpy(crt, path, crt_len) >= crt_len)
		goto getpath_overflow;
	if (strlcpy(key, path, key_len) >= key_len)
		goto getpath_overflow;
	if (crt_len - len < 4 || key_len - len < 4) {
		snprintf(client.error, sizeof(client.error),
		"The path to the certificate is too long");
		return -1;
	}
        if (strlcpy(&crt[len], ".crt", crt_len - len) + len >= crt_len)
		goto getpath_overflow;
        if (strlcpy(&key[len], ".key", key_len - len) + len >= key_len)
		goto getpath_overflow;
	return len+4;
getpath_overflow:
	snprintf(client.error, sizeof(client.error),
	"The cache folder path is too long %s", path);
	return -1;
}

int cert_create(char* host) {
	FILE *f = NULL;
	int ret = 1;
	EVP_PKEY* pkey;
	pkey = EVP_PKEY_new();
	RSA* rsa = RSA_new();
	BIGNUM* bne = BN_new();
	if (BN_set_word(bne, 65537) != 1) goto failed;
	if (RSA_generate_key_ex(rsa, 2048, bne, NULL) != 1) goto failed;

	EVP_PKEY_assign_RSA(pkey, rsa);
	X509* x509 = X509_new();
	int id;
	getrandom(&id, sizeof(id), GRND_RANDOM);
	if (ASN1_INTEGER_set(X509_get_serialNumber(x509), id) != 1)
		goto failed;

	X509_gmtime_adj(X509_get_notBefore(x509), 0);
	X509_gmtime_adj(X509_get_notAfter(x509), 157680000L);

	if (X509_set_pubkey(x509, pkey) != 1) goto failed;

	X509_NAME* name = X509_get_subject_name(x509);
	if (X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
	   (unsigned char *)host, -1, -1, 0) != 1) 
		goto failed;
	
	if (X509_set_issuer_name(x509, name) != 1) goto failed;
	if (X509_sign(x509, pkey, EVP_sha1()) == 0) goto failed;

	char key[1024];
	char crt[1024];
	if (cert_getpath(host, crt, sizeof(crt), key, sizeof(key)) == -1)
		goto skip_error;

	f = fopen(key, "wb");
	if (!f) {
		snprintf(client.error, sizeof(client.error), "Failed to write to %s", key);
		goto skip_error;
	}
	if (PEM_write_PrivateKey(f, pkey, NULL, NULL, 0, NULL, NULL) != 1)
		goto failed;
	fclose(f);

	f = fopen(crt, "wb");
	if (!f) {
		snprintf(client.error, sizeof(client.error), "Failed to write to %s", crt);
		goto skip_error;
	}
	if (PEM_write_X509(f, x509) != 1)
		goto failed;
	fclose(f);
	ret = 0;
	goto skip_error;
failed:
	snprintf(client.error, sizeof(client.error), "Failed to generate certificate");
skip_error:
	if (f) fclose(f);
	BN_free(bne);
	EVP_PKEY_free(pkey);
	X509_free(x509);
	RSA_free(rsa);
	if (ret) client.input.error = 1;
	return ret;
}
