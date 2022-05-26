/* See LICENSE file for copyright and license details. */
#include <stdlib.h>
#ifdef __linux__
#define OPENSSL_API_COMPAT 0x10101000L
#endif
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
#include <fcntl.h>
#ifdef __linux__
#include <sys/random.h>
#include <bsd/string.h>
#endif
#include "gemini.h"
#include "sandbox.h"
int config_folder = -1;

char homefolder[1024];
int homepath_cached = 0;
int gethomefolder(char* path, size_t len) {
	if (homepath_cached)
		return strlcpy(path, homefolder, len);
	struct passwd *pw = getpwuid(geteuid());
	if (!pw) return 0;
	size_t length = strlcpy(path, pw->pw_dir, len);
       	if (length >= len || length <= 0) return -1;
	homepath_cached = 1;
	strlcpy(homefolder, path, sizeof(homefolder));
	return length;
}

char downloadfolder[1024];
int downloadpath_cached = 0;
int getdownloadfolder(char* path, size_t len) {
	if (downloadpath_cached)
		return strlcpy(path, downloadfolder, len);
	size_t length = 0;
	if (!homepath_cached)
		length = gethomefolder(downloadfolder, sizeof(downloadfolder));
	else
		length = strlcpy(downloadfolder, homefolder, sizeof(downloadfolder));
	if (length <= 0) return -1;
	length += strlcpy(&downloadfolder[length], 
			  "/Downloads", sizeof(downloadfolder) - length);
        struct stat _stat;
        if (stat(downloadfolder, &_stat) && mkdir(downloadfolder, 0700))
		return -1;
	downloadpath_cached = 1;
	strlcpy(path, downloadfolder, len);
	return length;
}

char configfolder[1024];
int configpath_cached = 0;
int getconfigfolder(char* path, size_t len) {
	if (configpath_cached)
		return strlcpy(path, configfolder, len);
	int ret = gethomefolder(path, len);
	if (ret == -1) return -1;
	size_t length = ret;
	if (length >= len) return -1;
	length += strlcpy(&path[length], "/.config/vgmi", len - length);
        if (length >= len) return -1;
        struct stat _stat;
        if (stat(path, &_stat) && mkdir(path, 0700)) return -1;
	configpath_cached = 1;
	strlcpy(configfolder, path, sizeof(configfolder));
	config_folder = open(path, 0);
	if (config_folder < 0) return -1;
        return length;
}

int cert_getpath(char* host, char* crt, size_t crt_len, char* key, size_t key_len) {
	int len = strnlen(host, 1024);
	if (strlcpy(crt, host, crt_len) >= crt_len - 4)
		goto getpath_overflow;
	if (strlcpy(key, host, key_len) >= key_len - 4)
		goto getpath_overflow;
        if (strlcpy(&crt[len], ".crt", crt_len - len) + len >= crt_len)
		goto getpath_overflow;
        if (strlcpy(&key[len], ".key", key_len - len) + len >= key_len)
		goto getpath_overflow;
	return len+4;
getpath_overflow:
	snprintf(client.tabs[client.tab].error,
		 sizeof(client.tabs[client.tab].error),
		 "The hostname is too long %s", host);
	return -1;
}

int cert_create(char* host, char* error, int errlen) {
	FILE* f = NULL;
	int fd = -1;
	int ret = 1;
	EVP_PKEY* pkey;
	pkey = EVP_PKEY_new();
	RSA* rsa = RSA_new();
	BIGNUM* bne = BN_new();
	X509* x509 = X509_new();
	if (BN_set_word(bne, 65537) != 1) goto failed;
	if (RSA_generate_key_ex(rsa, 2048, bne, NULL) != 1) goto failed;

	EVP_PKEY_assign_RSA(pkey, rsa);
	int id;
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
	arc4random_buf(&id, sizeof(id));
#else
	getrandom(&id, sizeof(id), GRND_RANDOM);
#endif
	if (ASN1_INTEGER_set(X509_get_serialNumber(x509), id) != 1)
		goto failed;

	X509_gmtime_adj(X509_getm_notBefore(x509), 0);
	X509_gmtime_adj(X509_getm_notAfter(x509), 157680000L);

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

	// Key
	fd = openat(config_folder, key, O_CREAT|O_WRONLY, 0600);
	if (fd < 0) {
		snprintf(error, errlen, "Failed to open %s : %s", key, strerror(errno));
		goto skip_error;
	}
	f = fdopen(fd, "wb");
#ifdef __FreeBSD__
	if (makefd_writeonly(fd)) {
		snprintf(error, errlen, "Failed to limit %s", key);
		goto skip_error;
	}
#endif
	if (!f) {
		snprintf(error, errlen, "Failed to write to %s : %s",
			 key, strerror(errno));
		goto skip_error;
	}
	if (PEM_write_PrivateKey(f, pkey, NULL, NULL, 0, NULL, NULL) != 1)
		goto failed;
	fclose(f);

	// Certificate
	fd = openat(config_folder, crt, O_CREAT|O_WRONLY, 0600);
	if (fd < 0) {
		snprintf(error, errlen, "Failed to open %s", crt);
		goto skip_error;
	}
	f = fdopen(fd, "wb");
#ifdef __FreeBSD__
	if (makefd_writeonly(fd)) {
		snprintf(error, errlen, "Failed to limit %s", crt);
		goto skip_error;
	}
#endif
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
	BN_free(bne);
	EVP_PKEY_free(pkey);
	X509_free(x509);
	//RSA_free(rsa);
	//if (ret) client.input.error = 1;
	return ret;
}

int fatalI();
void fatal();

struct cert;
struct cert {
	struct cert* next;
	char hash[256];
	char host[1024];
	unsigned long long start;
	unsigned long long end;
};
struct cert* first_cert = NULL;
struct cert* last_cert = NULL;

void cert_add(char* host, const char* hash, unsigned long long start, unsigned long long end) {
	struct cert* cert_ptr = malloc(sizeof(struct cert));
	if (!cert_ptr) {
		fatal();
		return;
	}
	cert_ptr->next = NULL;
	if (!cert_ptr) {
		fatal();
		return;
	}
	if (!first_cert) {
		last_cert = first_cert = cert_ptr;
	} else {
		last_cert->next = cert_ptr;
		last_cert = cert_ptr;
	}
	strlcpy(last_cert->hash, hash, sizeof(first_cert->hash));
	strlcpy(last_cert->host, host, sizeof(first_cert->host));
	last_cert->start = start;
	last_cert->end = end;
}

int cert_load() {
	char path[1024];
	int len = getconfigfolder(path, sizeof(path));
	if (len < 1) return -1;
	strlcpy(&path[len], "/known_hosts", sizeof(path)-len);
	FILE* f = fopen(path, "r");
	if (!f) {
		return 0;
	}
	fseek(f, 0, SEEK_END);
	size_t length = ftell(f);
	fseek(f, 0, SEEK_SET);
	char* data = malloc(length);
	if (!data) return fatalI();
	if (fread(data, 1, length, f) != length) {
		fclose(f);
		return -1;
	}
	fclose(f);
	char* ptr = data;
	char* host = ptr;
	char* hash = NULL;
	char* start = NULL;
	char* end = NULL;
	while (ptr < data + length) {
		if (*ptr == ' ' || *ptr == '\t' || (host?(*ptr == '\n'):0)) {
			*ptr = '\0';
			ptr++;
			while (*ptr == ' ' || *ptr == '\t' || *ptr == '\n') ptr++;
			if (!hash) {
				hash = ptr;
				ptr++;
				continue;
			}
			if (!start) {
				start = ptr;
				ptr++;
				continue;
			}
			if (!end) {
				end = ptr;
				ptr++;
				continue;
			}

			cert_add(host, hash, atoi(start), atoi(end));
			host = ptr;
			hash = NULL;
		} else if (*ptr == '\n') {
			host = ptr+1;
			hash = NULL;
		}
		ptr++;
	}
	free(data);
	return 0;
}

int cert_getcert(char* host) {

	int index = 0;
	while (client.certs && index < client.certs_size) {
		if (!strcmp(client.certs[index].host, host)) return index;
		index++;
	}

        char crt[1024];
        char key[1024];
        if (cert_getpath(host, crt, sizeof(crt), key, sizeof(key)) == -1) {
		return -1;
        }
        size_t crt_pos = 0;
        size_t key_pos = 0;
	int crt_fd = openat(config_folder, crt, 0);
	if (crt_fd < 0) {
		return -2;
	}
	int key_fd = openat(config_folder, key, 0);
	if (key_fd < 0) {
		close(key_fd);
		return -2;
	}
	FILE* crt_f = fdopen(crt_fd, "rb");
	FILE* key_f = fdopen(key_fd, "rb");
#ifdef __FreeBSD__
	makefd_readonly(crt_fd);
	makefd_readonly(key_fd);
#endif
	if (!crt_f || !key_f) {
		close(crt_fd);
		close(key_fd);
		return -3;
	}
	fseek(crt_f, 0, SEEK_END);
	crt_pos = ftell(crt_f);
	fseek(key_f, 0, SEEK_END);
	key_pos = ftell(key_f);

	client.certs = realloc(client.certs, sizeof(*client.certs) * (index + 1));
	if (!client.certs) return fatalI();
	bzero(&client.certs[index], sizeof(*client.certs));
	client.certs[index].crt = malloc(crt_pos);
	if (!client.certs[index].crt) return fatalI();
	client.certs[index].key = malloc(key_pos);
	if (!client.certs[index].key) return fatalI();

	fseek(crt_f, 0, SEEK_SET);
	fseek(key_f, 0, SEEK_SET);
	if (fread(client.certs[index].crt, 1, crt_pos, crt_f) != crt_pos ||
	    fread(client.certs[index].key, 1, key_pos, key_f) != key_pos) {
		fclose(crt_f);
		fclose(key_f);
		return -3;
	}

	fclose(crt_f);
	fclose(key_f);
	client.certs[index].crt_len = crt_pos;
	client.certs[index].key_len = key_pos;
	client.certs_size++;
	return index;
}

int cert_verify(char* host, const char* hash, unsigned long long start, unsigned long long end) {
	struct cert* found = NULL;
	for (struct cert* cert = first_cert; cert; cert = cert->next) {
		if (!strcmp(host, cert->host)) {
			found = cert;
			break;
		}
	}
	unsigned long long now = time(NULL);
	if (found && found->start < now && found->end > now)
		return strcmp(found->hash, hash);
	char path[1024];
	int len = getconfigfolder(path, sizeof(path));
	if (len < 1) return -1;
	strlcpy(&path[len], "/known_hosts", sizeof(path)-len);

	int fd = openat(config_folder, "known_hosts", O_CREAT|O_APPEND|O_WRONLY, 0600);
	if (fd == -1)
		return -2;
	if (!fdopen(fd, "a")) return -3;
#ifdef __FreeBSD__
        if (makefd_writeonly(fd))
                return -3;
#endif
	char buf[2048];
	len = sprintf(buf, "%s %s %lld %lld\n", host, hash, start, end);
	if (write(fd, buf, len) != len) return -4;

	close(fd);
	cert_add(host, hash, start, end);
	return 0;
}

void cert_free() {
	struct cert *cert, *next_cert;
	cert = first_cert;
	while (cert) {
		next_cert = cert->next;
		free(cert);
		cert = next_cert;
	}
	for (int i = 0; i < client.certs_size; i++) {
		free(client.certs[i].crt);
		free(client.certs[i].key);
	}
	free(client.certs);
}
