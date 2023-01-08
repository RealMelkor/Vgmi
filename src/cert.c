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
#include <strings.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef __linux__
#include <sys/random.h>
#endif
#include "gemini.h"
#include "sandbox.h"
#include "str.h"
#include "xdg.h"

char home_path[1024];
char download_path[1024];
char config_path[1024];
const char* download_str = "Downloads";

int home_fd = -1;
int gethomefd() {
	if (home_fd > -1)
		return home_fd;
	struct passwd *pw = getpwuid(geteuid());
	if (!pw) return -1;
	home_fd = open(pw->pw_dir, O_DIRECTORY);
	strlcpy(home_path, pw->pw_dir, sizeof(home_path));
#ifndef DISABLE_XDG
	if (!xdg_path(download_path, sizeof(download_path))) {
		return home_fd;
	}
#endif
	snprintf(download_path, sizeof(download_path), "%s/%s",
		 home_path, download_str);
	return home_fd;
}

int download_fd = -1;
int getdownloadfd() {
	if (download_fd > -1)
		return download_fd;
	if (home_fd == -1 && gethomefd() == -1)
		return -1;
#ifndef DISABLE_XDG
	download_fd = open(download_path, O_DIRECTORY);
	if (download_fd > -1) return download_fd;
#endif
	download_fd = openat(home_fd, download_str, O_DIRECTORY);
	if (download_fd > -1) return download_fd;

	if (mkdirat(home_fd, download_str, 0700))
		return -1;

	download_fd = openat(home_fd, download_str, O_DIRECTORY);
	if (download_fd > -1) return download_fd;

	return -1;
}

int config_fd = -1;
int getconfigfd() {
	if (config_fd > -1)
		return config_fd;
	if (home_fd == -1 && gethomefd() == -1)
		return -1;
	// check if .config exists first
	int fd = openat(home_fd, ".config", O_DIRECTORY);
	if (fd < 0) {
		mkdirat(home_fd, ".config", 0700);
		fd = openat(home_fd, ".config", O_DIRECTORY);
		if (fd < 0)
			return -1;
	}
	config_fd = openat(fd, "vgmi", O_DIRECTORY);
	if (config_fd < 0) {
		mkdirat(fd, "vgmi", 0700);
		config_fd = openat(fd, "vgmi", O_DIRECTORY);
		if (config_fd < 0)
			return -1;
	}
	close(fd);
	snprintf(config_path, sizeof(config_path), "%s/%s",
		 home_path, "/.config/vgmi");
	return config_fd;
}

int cert_getpath(const char* host, char* crt, size_t crt_len,
		 char* key, size_t key_len) {
	int len = strnlen(host, 1024);
	if (strlcpy(crt, host, crt_len) >= crt_len - 4)
		goto getpath_overflow;
	if (strlcpy(key, host, key_len) >= key_len - 4)
		goto getpath_overflow;
        if (strlcpy(&crt[len], ".crt", crt_len - len) + len >= crt_len)
		goto getpath_overflow;
        if (strlcpy(&key[len], ".key", key_len - len) + len >= key_len)
		goto getpath_overflow;
	return len + 4;
getpath_overflow:
	snprintf(client.tab->error,
		 sizeof(client.tab->error),
		 "The hostname is too long %s", host);
	return -1;
}

#ifdef SANDBOX_SUN
#undef cert_create
#endif
int cert_create(char* host, char* error, int errlen) {
	FILE* f = NULL;
	int fd;
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
#ifdef __linux__
	getrandom(&id, sizeof(id), GRND_RANDOM);
#else
	arc4random_buf(&id, sizeof(id));
#endif
	if (ASN1_INTEGER_set(X509_get_serialNumber(x509), id) != 1)
		goto failed;

	X509_gmtime_adj(X509_getm_notBefore(x509), 0);
	X509_gmtime_adj(X509_getm_notAfter(x509), 157680000L);

	if (X509_set_pubkey(x509, pkey) != 1) goto failed;

	X509_NAME* name = X509_get_subject_name(x509);
	if (X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
	   (unsigned char*)host, -1, -1, 0) != 1)
		goto failed;
	
	if (X509_set_issuer_name(x509, name) != 1) goto failed;
	if (X509_sign(x509, pkey, EVP_sha1()) == 0) goto failed;

	char key[1024];
	char crt[1024];
	if (cert_getpath(host, crt, sizeof(crt), key, sizeof(key)) == -1)
		goto skip_error;

	// Key
	fd = openat(config_fd, key, O_CREAT|O_WRONLY, 0600);
	if (fd < 0) {
		snprintf(error, errlen, "Failed to open %s : %s",
			 key, strerror(errno));
		goto skip_error;
	}
	f = fdopen(fd, "wb");
#ifdef SANDBOX_FREEBSD
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
	fd = openat(config_fd, crt, O_CREAT|O_WRONLY, 0600);
	if (fd < 0) {
		snprintf(error, errlen, "Failed to open %s", crt);
		goto skip_error;
	}
	f = fdopen(fd, "wb");
#ifdef SANDBOX_FREEBSD
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

struct cert {
	struct cert* next;
	char hash[256];
	char host[1024];
	unsigned long long start;
	unsigned long long end;
};
struct cert* first_cert = NULL;
struct cert* last_cert = NULL;

void cert_add(char* host, const char* hash, unsigned long long start,
	      unsigned long long end) {
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
	config_fd = getconfigfd();
	if (config_fd < 0) return -1;
	int known_hosts = openat(config_fd, "known_hosts", 0);
	if (known_hosts < 0) return 0;
	FILE* f = fdopen(known_hosts, "r");
	if (!f)
		return -1;
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
		if (ptr == data + length - 1) goto add;
		if (*ptr == ' ' || *ptr == '\t' || (host?(*ptr == '\n'):0)) {
			*ptr = '\0';
			ptr++;
			while (*ptr == ' ' || *ptr == '\t' || *ptr == '\n')
				ptr++;
add:
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
			cert_add(host, hash,
				 strtoull(start, NULL, 10),
				 strtoull(end, NULL, 10));
			host = ptr;
			end = start = hash = NULL;
		} else if (*ptr == '\n') {
			host = ptr+1;
			end = start = hash = NULL;
		}
		ptr++;
	}
	free(data);
	return 0;
}

int cert_loadcert(const char* host, struct cert_cache* cert) {
        char crt[1024];
        char key[1024];
        if (cert_getpath(host, crt, sizeof(crt), key, sizeof(key)) == -1) {
                return -1;
        }
        size_t crt_pos = 0;
        size_t key_pos = 0;
        int crt_fd = openat(config_fd, crt, 0);
        if (crt_fd < 0) {
                return -2;
        }
        int key_fd = openat(config_fd, key, 0);
        if (key_fd < 0) {
                close(key_fd);
                return -2;
        }
        FILE* crt_f = fdopen(crt_fd, "rb");
        FILE* key_f = fdopen(key_fd, "rb");
#ifdef SANDBOX_FREEBSD
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

        cert->crt = malloc(crt_pos);
        if (!cert->crt) return fatalI();
        cert->key = malloc(key_pos);
        if (!cert->key) return fatalI();

        fseek(crt_f, 0, SEEK_SET);
        fseek(key_f, 0, SEEK_SET);
        if (fread(cert->crt, 1, crt_pos, crt_f) != crt_pos ||
            fread(cert->key, 1, key_pos, key_f) != key_pos) {
                fclose(crt_f);
                fclose(key_f);
                return -3;
        }

        fclose(crt_f);
        fclose(key_f);
        cert->crt[crt_pos - 1] = '\0';
        cert->key[key_pos - 1] = '\0';
        cert->crt_len = crt_pos;
        cert->key_len = key_pos;
        strlcpy(cert->host, host, sizeof(cert->host));
        return 0;
}

int cert_getcert(char* host, int reload) {
        int index = 0;
        while (client.certs && index < client.certs_size) {
                if (!strcmp(client.certs[index].host, host)) {
                        if (reload) {
                                reload = 2;
                                break;
                        }
                        return index;
                }
                index++;
        }
        if (reload != 2) reload = 0;

        struct cert_cache cert = {0};
#ifdef SANDBOX_SUN
	if (send(rd_pair[1], &RD_CERTIFICATE, sizeof(SBC), 0) != sizeof(SBC))
		return -1;
	uint16_t i16 = strlen(host);
	if (send(rd_pair[1], &i16, sizeof(i16), 0) != sizeof(i16))
		return -1;
	if (send(rd_pair[1], host, i16, 0) != i16)
		return -1;
	char c;
	if (recv(rd_pair[1], &c, sizeof(c), 0) != 1 || c)
		return -1;

	if (recv(rd_pair[1], &cert.crt_len, sizeof(cert.crt_len), 0) !=
	    sizeof(cert.crt_len))
		return -1;
	if (recv(rd_pair[1], &cert.key_len, sizeof(cert.key_len), 0) !=
	    sizeof(cert.key_len))
		return -1;
	cert.crt = malloc(cert.crt_len);
	if (!cert.crt) return -1;
	cert.key = malloc(cert.key_len);
	if (!cert.key) {
		free(cert.crt);
		return -1;
	}
	if (recv(rd_pair[1], cert.crt, cert.crt_len, 0) != (signed)cert.crt_len
	|| recv(rd_pair[1], cert.key, cert.key_len, 0) != (signed)cert.key_len)
	{
		free(cert.crt);
		free(cert.key);
		return -1;
	}
#else
        if (cert_loadcert(host, &cert))
                return -1;
#endif

        if (!reload) {
                client.certs = realloc(client.certs,
                                       sizeof(*client.certs) * (index + 1));
                if (!client.certs) return fatalI();
                bzero(&client.certs[index], sizeof(*client.certs));
        } else {
                free(client.certs[index].crt);
                free(client.certs[index].key);
        }
        client.certs[index] = cert;
        if (!reload)
                client.certs_size++;
        return index;
}

#ifdef SANDBOX_SUN
int cert_rewrite() {
	int fd = wr_pair[1];
	if (send(fd, &WR_KNOWNHOSTS, sizeof(SBC), 0) != sizeof(SBC))
		return -3;
#else
int cert_rewrite() {
	int cfd = getconfigfd();
	if (cfd < 0) return -1;

	int fd = openat(cfd, "known_hosts",
			O_CREAT|O_WRONLY|O_CLOEXEC|O_TRUNC, 0600);
	if (fd == -1)
		return -2;
#ifdef SANDBOX_FREEBSD
        if (makefd_writeonly(fd))
                return -3;
#endif

#endif
	char buf[2048];
	for (struct cert* cert = first_cert; cert; cert = cert->next) {
		int len = snprintf(buf, 2048, "%s %s %llu %llu\n",
				   cert->host, cert->hash,
				   cert->start, cert->end);
		if (write(fd, buf, len) != len) {
#ifdef SANDBOX_SUN
			send(fd, &WR_END, sizeof(SBC), 0);
#else
			close(fd);
#endif
			return -1;
		}
	}
#ifdef SANDBOX_SUN
	send(fd, &WR_END, sizeof(SBC), 0);
#else
	close(fd);
#endif
	return 0;
}

int cert_forget(char* host) {
	struct cert* prev = NULL;
	for (struct cert* cert = first_cert; cert; cert = cert->next) {
		if (strcmp(host, cert->host)) {
			prev = cert;
			continue;
		}
		if (prev)
			prev->next = cert->next;
		if (cert->next == NULL)
			last_cert = prev;
		free(cert);
		return cert_rewrite();
	}
	return -1;
}

int cert_verify(char* host, const char* hash,
		unsigned long long start,
		unsigned long long end) {
	struct cert* found = NULL;
	for (struct cert* cert = first_cert; cert; cert = cert->next) {
		if (strcmp(host, cert->host)) continue;
		found = cert;
		break;
	}
	unsigned long long now = time(NULL);
	if (found) {
		if (found->start < now && found->end > now)
			return strcmp(found->hash, hash) ? -6 : 0;
		return -5; // expired
	}
#ifdef SANDBOX_SUN
	int fd = wr_pair[1];
	if (send(fd, &WR_KNOWNHOST_ADD, sizeof(SBC), 0) != sizeof(SBC))
		return -3;
#else
	int cfd = getconfigfd();
	if (cfd < 0) return -1;

	int fd = openat(cfd, "known_hosts", O_CREAT|O_APPEND|O_WRONLY, 0600);
	if (fd == -1)
		return -2;
	if (!fdopen(fd, "a")) return -3;
#ifdef SANDBOX_FREEBSD
        if (makefd_writeonly(fd))
                return -3;
#endif
#endif
	char buf[2048];
	int len = snprintf(buf, 2048, "%s %s %lld %lld\n",
			   host, hash, start, end);
	if (write(fd, buf, len) != len) {
		close(fd);
		return -4;
	}

#ifdef SANDBOX_SUN
	send(fd, &WR_END, sizeof(SBC), 0);
#else
	close(fd);
#endif
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
	if (config_fd > 0)
		close(config_fd);
	if (download_fd > 0)
		close(download_fd);
	if (home_fd > 0)
		close(home_fd);
}
