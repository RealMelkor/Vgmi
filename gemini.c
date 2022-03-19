#include "gemini.h"
#include <netinet/in.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <tls.h>
#include <poll.h>
#include <fcntl.h>
#include <errno.h>
#include <termbox.h>

struct tls_config* config;
struct tls* ctx;

#define GMI 9
char** gmi_links = NULL;
size_t gmi_links_count = 0;
size_t gmi_links_len = 0;
#define MAX_URL 1024
char gmi_url[MAX_URL];
char gmi_host[256];
char* gmi_data = NULL;
int gmi_lines = 0;
char gmi_error[256];

int gmi_goto(int id) {
	id--;
	if (id < 0 || id >= gmi_links_count) {
		snprintf(gmi_error, sizeof(gmi_error), "Invalid link number");
		return -1;
	}
	if (gmi_links[id][0] == '/') {
		char* ptr = strchr(&gmi_url[GMI], '/');
		if (ptr) *ptr = '\0';
		strncat(gmi_url, gmi_links[id], sizeof(gmi_url)-strlen(gmi_links[id])-1);
		int ret = gmi_request(gmi_url);
		if (ret < 1) return ret;
		gmi_load(gmi_data, ret);
		return ret;
	}
	return 0;
}

void gmi_load(char* data, size_t len) {
	gmi_links_count = 0;
	gmi_lines = 0;
	for (int c = 0; c < len; c++) {
		if (data[c] == '\n') {
			gmi_lines++;
			continue;
		}
		if (data[c] == '=' && data[c+1] == '>') {
			c += 2;
			for (; data[c]==' '; c++) ;
			char* url = &data[c];
			for (; data[c]!=' '; c++) ;
			data[c] = '\0';
			if (gmi_links_count >= gmi_links_len) {
				gmi_links_len+=1024;
				if (gmi_links)
					gmi_links = realloc(gmi_links, sizeof(char*) * gmi_links_len);
				else
					gmi_links = malloc(sizeof(char*) * gmi_links_len);
				bzero(&gmi_links[gmi_links_len-1024], 1024);
			}
			if (gmi_links[gmi_links_count]) free(gmi_links[gmi_links_count]);
			gmi_links[gmi_links_count] = malloc(strlen(url));
			strcpy(gmi_links[gmi_links_count], url);
			gmi_links_count++;
			data[c] = ' ';
		}
	}
}

void gmi_render(char* data, size_t len, int y) {
	int line = 0;
	int x = 0;
	int links = 0;
	uintattr_t color = TB_DEFAULT;
	for (int c = 0; c < len; c++) {
		if (data[c] == '\n' || x+4>=tb_width() || (data[c] == ' ' && x+16>=tb_width())) {
			line++;
			color = TB_DEFAULT;
			x = 0;
			if (line-y > tb_height()-2) break;
			continue;
		}
		for (int i=0; x == 0 && data[c+i] == '#' && i<3; i++) {
			if (data[c+i+1] == ' ') {
				color = TB_RED + i;
				break;
			}
		}
		if (x == 0 && data[c] == '>' && data[c+1] == ' ') {
			color = TB_ITALIC|TB_MAGENTA;
		}
		if (x == 0 && data[c] == '=' && data[c+1] == '>') {
			if (line-1>=y) {
				char buf[32];
				snprintf(buf, sizeof(buf), "[%d]", links+1);
				tb_print(x+2, line-1-y, TB_BLUE, TB_DEFAULT, buf);
				x += strlen(buf);
			}
			c += 2;
			for (; data[c]==' '; c++) ;
			for (; data[c]!=' '; c++) ;
			links++;
		}
		if (line-1<(y>=0?y:0)) continue;
		tb_set_cell(x+2, line-1-y, data[c], color, TB_DEFAULT);
		x++;
	}
}

struct tls_config* config;
int gmi_init() {
	if (tls_init()) {
		printf("Failed to initialize TLS\n");
		return -1;
	}
	config = tls_config_new();
	if (!config) {
		printf("Failed to initialize TLS config\n");
		return -1;
	}
	tls_config_insecure_noverifycert(config);
	ctx = NULL;
	bzero(gmi_error, sizeof(gmi_error));
	
	return 0;
}

int gmi_request(const char* url) {
	
	int url_len = strlen(url);
	if (url_len <= GMI || memcmp(url, "gemini://", GMI)) {
		snprintf(gmi_error, sizeof(gmi_error), "Invalid url: %s", url);
		return -3;
	}
	char host[256];
	bzero(host, sizeof(host));
	char* ptr = strchr(&url[GMI], '/');
	if (ptr > &url[GMI]) {
		if (ptr - &url[GMI] >= sizeof(host)) {
			snprintf(gmi_error, sizeof(gmi_error), "Too large hostname: %s", url);
			return -4;
		}
		memcpy(host, &url[GMI], ptr - &url[GMI]);
	} else {
		if (url_len-GMI >= sizeof(host)) {
			snprintf(gmi_error, sizeof(gmi_error), "Too large hostname: %s", url);
			return -4;
		}
		memcpy(host, &url[GMI], url_len-GMI);
	}
	if (ctx) {
		tls_close(ctx);
		ctx = NULL;
	}
	ctx = tls_client();
	if (!ctx) {
		return -1;
	}
	if (tls_configure(ctx, config)) {
		return -1;
	}
	if (tls_connect(ctx, host, "1965")) {
		snprintf(gmi_error, sizeof(gmi_error), "Unable to connect to: %s", host);
		return -1;
	}
	char urlbuf[MAX_URL+2];
	strncpy(urlbuf, url, url_len);
	urlbuf[url_len] = '\r';
	urlbuf[url_len+1] = '\n';
	urlbuf[url_len+2] = '\0';
	strncpy(gmi_host, host, sizeof(gmi_host));
	if (tls_write(ctx, urlbuf, strnlen(urlbuf, MAX_URL+2)) < strnlen(urlbuf, MAX_URL+2)) {
		snprintf(gmi_error, sizeof(gmi_error), "Failed to send data to: %s", host);
		return -2;
	}
	char buf[1024];
	int recv = tls_read(ctx, buf, sizeof(buf));
	if (recv < 2) {
		snprintf(gmi_error, sizeof(gmi_error), "Invalid data to from: %s", host);
		return -1;
	}
	ptr = strchr(buf, ' ');
	if (!ptr) {
		snprintf(gmi_error, sizeof(gmi_error), "Invalid data to from: %s", host);
		return -1;
	}
	*ptr = '\0';
	int code = atoi(buf);
	if (code < 1) {
		snprintf(gmi_error, sizeof(gmi_error), "Invalid data to from: %s", host);
		return -1;
	}
	*ptr = ' ';
	if (!gmi_data) free(gmi_data);
	gmi_data = malloc(recv);
	strncpy(gmi_data, buf, recv);
	if (recv != 1024) return recv; 
	while (1) {
		int bytes = tls_read(ctx, buf, 1024);
		if (bytes == -1) {
			snprintf(gmi_error, sizeof(gmi_error), "Invalid data to from: %s", host);
			return -1;
		}
		gmi_data = realloc(gmi_data, recv+bytes);
		strncpy(&gmi_data[recv], buf, bytes);
		recv += bytes;
		if (bytes != 1024) break;
	}
	return recv;
}
