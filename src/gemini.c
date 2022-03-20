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
#include <ctype.h>

struct tls_config* config;
struct tls* ctx;

#define GMI 9
char** gmi_links = NULL;
size_t gmi_links_count = 0;
size_t gmi_links_len = 0;
char gmi_url[MAX_URL];
char gmi_host[256];
char* gmi_data = NULL;
int gmi_lines = 0;
char gmi_error[256];
char gmi_input[256];
char* gmi_history[1024]; // linked list
int gmi_code = 0;

int gmi_goto(int id) {
	id--;
	if (id < 0 || id >= gmi_links_count) {
		snprintf(gmi_error, sizeof(gmi_error), "Invalid link number");
		return -1;
	}
	if (gmi_links[id][0] == '/') {
		char* ptr = strchr(&gmi_url[GMI], '/');
		if (ptr) *ptr = '\0';
		char urlbuf[MAX_URL];
		strncpy(urlbuf, gmi_url, sizeof(urlbuf));
		strncat(urlbuf, gmi_links[id], sizeof(urlbuf)-strlen(gmi_links[id])-1);
		int ret = gmi_request(urlbuf);
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

int gmi_render(char* data, size_t len, int y) {
	int line = 0;
	int x = 0;
	int links = 0;
	uintattr_t color = TB_DEFAULT;
	for (int c = 0; c < len; c++) {
		if (x == 0 && data[c] == ' ' && c+1 < len) c++;
		if (data[c] == '\t') {
			x+=4;
			continue;
		}
		if (data[c] == '\r') continue;
		for (int i=0; x == 0 && data[c+i] == '#' && i<3; i++) {
			if (data[c+i+1] == ' ') {
				color = TB_RED + i;
				break;
			}
		}
		if (x == 0 && data[c] == '*' && data[c+1] == ' ') {
			color = TB_ITALIC|TB_CYAN;
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
		if (data[c] == '\n' || data[c] == ' ' || x+4 >= tb_width()) {
			if (x+4>=tb_width()) c--;
			int newline = (data[c] == '\n' || x+4 >= tb_width());
			for (int i=1; 1; i++) {
				if (i > tb_width() - 4) {
					newline = 0;
					break;
				}
				if (data[c+i] == ' ' || data[c+i] == '\n' ||
				    data[c+i] == '\0' || c+i >= len)
					break;
				if (tb_width()-4<=x+i) newline = 1;
			}
			if (newline) {
				line++;
				if (data[c] == '\n')
					color = TB_DEFAULT;
				x = 0;
				continue;
			}
		}
		if (line-1>=(y>=0?y:0) && (line-y <= tb_height()-2)) 
			tb_set_cell(x+2, line-1-y, data[c], color, TB_DEFAULT);
		x++;
	}
	return line;
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
	bzero(gmi_history, sizeof(gmi_history));
	
	return 0;
}

#define PROTO_GEMINI 0
#define PROTO_HTTP 1
#define PROTO_HTTPS 2
#define PROTO_GOPHER 3

int gmi_parseurl(const char* url, char* host, int host_len, char* urlbuf, int url_len) {
	int proto = PROTO_GEMINI;
	char* proto_ptr = strstr(url, "://");
	char* ptr = (char*)url;
	if (!proto_ptr) {
		goto skip_proto;			
	}
	char proto_buf[16];
	for(; proto_ptr!=ptr; ptr++) {
		if (!((*ptr > 'a' && *ptr < 'z') || (*ptr > 'A' && *ptr < 'Z'))) goto skip_proto;
		if (ptr-url >= sizeof(proto_buf)) goto skip_proto;
		proto_buf[ptr-url] = tolower(*ptr);
	}
	proto_buf[ptr-url] = '\0';
	ptr+=3;
	proto_ptr+=3;
	if (!strcmp(proto_buf,"gemini")) goto skip_proto;
	else if (!strcmp(proto_buf,"http")) proto = PROTO_HTTP;
	else if (!strcmp(proto_buf,"https")) proto = PROTO_HTTPS;
	else if (!strcmp(proto_buf,"gopher")) proto = PROTO_GOPHER;
	else {
		return -1; // unknown protocol
	}
skip_proto:;
     	if (!proto_ptr) proto_ptr = ptr;
	char* host_ptr = strchr(ptr, '/');
	if (!host_ptr) host_ptr = ptr+strlen(ptr);
	for(; host_ptr!=ptr; ptr++) {
		if (host_len <= host_ptr-ptr) {
			return -1;
		}
		if (!((*ptr >= 'a' && *ptr <= 'z') || (*ptr >= 'A' && *ptr <= 'Z')
		|| (*ptr >= '0' && *ptr <= '9') || *ptr == '.' || *ptr == '-')) {
			return -1;
		}
		host[ptr-proto_ptr] = *ptr;
	}
	host[ptr-proto_ptr] = '\0';
//skip_url:;
	if (url_len < 16) return -1; // buffer too small
	switch (proto) {
	case PROTO_GEMINI:
		strncpy(urlbuf, "gemini://", url_len);
		break;
	case PROTO_HTTP:
		strncpy(urlbuf, "http://", url_len);
		break;
	case PROTO_HTTPS:
		strncpy(urlbuf, "http://", url_len);
		break;
	case PROTO_GOPHER:
		strncpy(urlbuf, "gopher://", url_len);
		break;
	default:
		return -1;
	}
	strncat(urlbuf, host, url_len);
	if (host_ptr)
		strncat(urlbuf, host_ptr, url_len);
	if (urlbuf[strnlen(urlbuf, url_len) - 1] == '/')
		urlbuf[strnlen(urlbuf, url_len) - 1] = '\0';
	return proto;
}

int gmi_request(const char* url) {
	
	if (gmi_parseurl(url, gmi_host, sizeof(gmi_host), gmi_url, sizeof(gmi_url)) < 0) {
		snprintf(gmi_error, sizeof(gmi_error), "Invalid url: %s", url);
		return -1;
	}
	if (ctx) {
		tls_close(ctx);
		ctx = NULL;
	}
	ctx = tls_client();
	if (!ctx) {
		snprintf(gmi_error, sizeof(gmi_error), "Failed to initialize TLS");
		return -1;
	}
	if (tls_configure(ctx, config)) {
		snprintf(gmi_error, sizeof(gmi_error), "Failed to configure TLS");
		return -1;
	}
	if (tls_connect(ctx, gmi_host, "1965")) {
		snprintf(gmi_error, sizeof(gmi_error), "Unable to connect to: %s", gmi_host);
		return -1;
	}
	char urlbuf[MAX_URL];
	strncpy(urlbuf, gmi_url, MAX_URL);
	strncat(urlbuf, "\r\n", MAX_URL-1);
	if (tls_write(ctx, urlbuf, strnlen(urlbuf, MAX_URL)) < strnlen(urlbuf, MAX_URL)) {
		snprintf(gmi_error, sizeof(gmi_error), "Failed to send data to: %s", gmi_host);
		return -2;
	}
	char buf[1024];
	int recv = -2;
	while (recv==-2)
		recv = tls_read(ctx, buf, sizeof(buf));
	if (recv < 2) {
		snprintf(gmi_error, sizeof(gmi_error), "[%d] Invalid data to from: %s", recv, gmi_host);
		return -1;
	}
	char* ptr = strchr(buf, ' ');
	if (!ptr) {
		snprintf(gmi_error, sizeof(gmi_error), "Invalid data to from: %s", gmi_host);
		return -1;
	}
	*ptr = '\0';
	gmi_code = atoi(buf);
	switch (gmi_code) {
	case 10:
	case 11:
		ptr++;
		strncpy(gmi_input, ptr, sizeof(gmi_input));
		ptr = strchr(gmi_input, '\n');
		if (ptr) *ptr = '\0';
	case 20:
		break;
	case 30:
		snprintf(gmi_error, sizeof(gmi_error), "Redirect temporary");
		return -1;
	case 31:
		snprintf(gmi_error, sizeof(gmi_error), "Redirect permanent");
		return -1;
	case 40:
		snprintf(gmi_error, sizeof(gmi_error), "Temporary failure");
		return -1;
	case 41:
		snprintf(gmi_error, sizeof(gmi_error), "Server unavailable");
		return -1;
	case 42:
		snprintf(gmi_error, sizeof(gmi_error), "CGI error");
		return -1;
	case 43:
		snprintf(gmi_error, sizeof(gmi_error), "Proxy error");
		return -1;
	case 44:
		snprintf(gmi_error, sizeof(gmi_error), "Slow down: server above rate limit");
		return -1;
	case 50:
		snprintf(gmi_error, sizeof(gmi_error), "Permanent failure");
		return -1;
	case 51:
		snprintf(gmi_error, sizeof(gmi_error), "Not found");
		return -1;
	case 52:
		snprintf(gmi_error, sizeof(gmi_error), "Resource gone");
		return -1;
	case 53:
		snprintf(gmi_error, sizeof(gmi_error), "Proxy request refused");
		return -1;
	case 59:
		snprintf(gmi_error, sizeof(gmi_error), "Bad request");
		return -1;
	case 60:
		snprintf(gmi_error, sizeof(gmi_error), "Client certificate required");
		return -1;
	case 61:
		snprintf(gmi_error, sizeof(gmi_error), "Client not authorised");
		return -1;
	case 62:
		snprintf(gmi_error, sizeof(gmi_error), "Client certificate not valid");
		return -1;
	default:
		snprintf(gmi_error, sizeof(gmi_error), "Unknown status code: %d", gmi_code);
		return -1;
	}
	*ptr = ' ';
	if (!gmi_data) free(gmi_data);
	gmi_data = malloc(recv);
	strncpy(gmi_data, buf, recv);
	while (1) {
		int bytes = tls_read(ctx, buf, sizeof(buf));
		if (bytes == -1) {
			snprintf(gmi_error, sizeof(gmi_error), "Invalid data to from: %s", gmi_host);
			return -1;
		}
		gmi_data = realloc(gmi_data, recv+bytes);
		strncpy(&gmi_data[recv], buf, bytes);
		recv += bytes;
		if (bytes == 0) break;
	}
	return recv;
}
