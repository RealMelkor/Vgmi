#include "gemini.h"
#include <netinet/in.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
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
struct gmi_link* gmi_history = NULL;
int gmi_code = 0;

int gmi_goto(int id) {
	id--;
	if (id < 0 || id >= gmi_links_count) {
		snprintf(gmi_error, sizeof(gmi_error), "Invalid link number, %d/%ld", id, gmi_links_count);
		return -1;
	}
	return gmi_nextlink(gmi_url, sizeof(gmi_url), gmi_links[id], sizeof(gmi_links[id]));
}

int gmi_nextlink(char* url, size_t url_len, char* link, size_t link_len) {
	if (gmi_history && gmi_history->next)
		gmi_cleanforward();
	if (link[0] == '/' && link[1] == '/') {
		int ret = gmi_request(&link[2]);
		if (ret < 1) return ret;
		gmi_load(gmi_data, ret);
		return ret;
	} else if (link[0] == '/') {
		char* ptr = strchr(&gmi_url[GMI], '/');
		if (ptr) *ptr = '\0';
		char urlbuf[MAX_URL];
		strncpy(urlbuf, gmi_url, sizeof(urlbuf));
		strncat(urlbuf, link, sizeof(urlbuf)-strlen(link)-1);
		int ret = gmi_request(urlbuf);
		if (ret < 1) return ret;
		gmi_load(gmi_data, ret);
		return ret;
	} else if (strstr(link, "gemini://")) {
		int ret = gmi_request(link);
		if (ret < 1) return ret;
		gmi_load(gmi_data, ret);
		return ret;
	} else {
		char* ptr = strchr(&gmi_url[GMI], '/');
		if (ptr) *ptr = '\0';
		char urlbuf[MAX_URL];
		strncpy(urlbuf, gmi_url, sizeof(urlbuf));
		strncat(urlbuf, "/", sizeof(urlbuf)-strnlen(urlbuf, MAX_URL)-1);
		strncat(urlbuf, link, sizeof(urlbuf)-strlen(link)-1);
		int ret = gmi_request(urlbuf);
		if (ret < 1) return ret;
		gmi_load(gmi_data, ret);
		return ret;
	}
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
			char* url = (char*)&data[c];
			for (; data[c]!=' '; c++) ;
			data[c] = '\0';
			if (gmi_links_count >= gmi_links_len) {
				gmi_links_len+=1024;
				if (gmi_links)
					gmi_links = realloc(gmi_links, sizeof(char*) * gmi_links_len);
				else
					gmi_links = malloc(sizeof(char*) * gmi_links_len);
				bzero(&gmi_links[gmi_links_len-1024], 1024 * sizeof(char*));
			}
			if (gmi_links[gmi_links_count]) free(gmi_links[gmi_links_count]);
			if (strlen(url) == 0) {
				gmi_links[gmi_links_count] = NULL;
				data[c] = ' ';
				continue;
			}
			gmi_links[gmi_links_count] = malloc(strlen(url)+1);
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
			if (line-1>=(y>=0?y:0) && (line-y <= tb_height()-2)) {
				char buf[32];
				snprintf(buf, sizeof(buf), "[%d]", links+1);
				tb_print(x+2, line-1-y, TB_BLUE, TB_DEFAULT, buf);
				x += strlen(buf);
			}
			c += 2;
			for (; data[c]==' '; c++) ;
			for (; data[c]!=' '; c++) ;
			for (; data[c]==' '; c++) ;
			x+=3;
			if ((links+1)/10) x--;
			if ((links+1)/100) x--;
			if ((links+1)/1000) x--;
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
		uint32_t ch = 0;
		c += tb_utf8_char_to_unicode(&ch, &data[c])-1;

		if (line-1>=(y>=0?y:0) && (line-y <= tb_height()-2)) 
			tb_set_cell(x+2, line-1-y, ch, color, TB_DEFAULT);
		x++;
	}
	return line;
}

void gmi_cleanforward() {
	struct gmi_link* ptr = gmi_history;
	while (ptr && ptr->next) {
		struct gmi_link* next = ptr->next;
		if (gmi_history != ptr)
			free(ptr);
		if (!next) break;
		ptr = next;
	}
	gmi_history->next = NULL;
}

void gmi_freehistory() {
	struct gmi_link* ptr = gmi_history->prev;
	free(gmi_history);
	while ((ptr = ptr->prev)) {
		gmi_history = ptr;
		ptr = ptr->prev;
		free(gmi_history);
	}
}

struct tls_config* config;
#include <time.h>
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
	if (tls_config_set_keypair_file(config, "txt.rmf-dev.com.crt", "txt.rmf-dev.com.key")) {
		printf("Failed to load keypair\n");
		return -1;
	}
	tls_config_insecure_noverifycert(config);
	ctx = NULL;
	bzero(gmi_error, sizeof(gmi_error));
	gmi_history = NULL;
	gmi_data = NULL;
	gmi_links = NULL;
	gmi_links_count = 0;
	gmi_links_len = 0;
	
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
	return proto;
}

int gmi_request(const char* url) {
	int recv = TLS_WANT_POLLIN;
	int sockfd = -1;
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

	// Manually create socket to set a timeout
	struct addrinfo hints, *result;
	memset (&hints, 0, sizeof (hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags |= AI_CANONNAME;
	if (getaddrinfo(gmi_host, NULL, &hints, &result)) {
		snprintf(gmi_error, sizeof(gmi_error), "Unknown domain name: %s", gmi_host);
		return -1;
	}

	struct sockaddr_in addr4;
	struct sockaddr_in6 addr6;
	sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	struct timeval tv;
	tv.tv_sec = 3;
	tv.tv_usec = 0;
	int value = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv);
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
	struct sockaddr* addr;
	if (result->ai_family == AF_INET) {
		addr4.sin_addr = ((struct sockaddr_in*) result->ai_addr)->sin_addr;
		addr4.sin_family = AF_INET;
		addr4.sin_port = htons(1965);
		addr = (struct sockaddr*)&addr4;
	}
	else if (result->ai_family == AF_INET6) {
		addr6.sin6_addr = ((struct sockaddr_in6*) result->ai_addr)->sin6_addr;
		addr6.sin6_family = AF_INET6;
		addr6.sin6_port = htons(1965);
		addr = (struct sockaddr*)&addr6;
	}
	int family = result->ai_family;
	freeaddrinfo(result);
	
	if (connect(sockfd, addr, (family == AF_INET)?sizeof(addr4):sizeof(addr6)) != 0) {
		snprintf(gmi_error, sizeof(gmi_error), "Connection to %s timed out", gmi_host);
		close(sockfd);
		return -1;
	}

	if (tls_connect_socket(ctx, sockfd, gmi_host)) {
		snprintf(gmi_error, sizeof(gmi_error), "Unable to connect to: %s", gmi_host);
		goto request_error;
	}
	if (tls_handshake(ctx)) {
		snprintf(gmi_error, sizeof(gmi_error), "Failed to handshake: %s", gmi_host);
		goto request_error;
	}
	char urlbuf[MAX_URL];
	strncpy(urlbuf, gmi_url, MAX_URL);
	size_t len = strnlen(urlbuf, MAX_URL)+2;
	strncat(urlbuf, "\r\n", MAX_URL-len);
	char* ptr = urlbuf;
	while (len) {
		size_t ret;

		ret = tls_write(ctx, ptr, len);
		if (ret == TLS_WANT_POLLIN || ret == TLS_WANT_POLLOUT)
			continue;
		if (ret == -1) {
			snprintf(gmi_error, sizeof(gmi_error), "Failed to send request: %s", gmi_host);
			goto request_error;
		}
		ptr += ret;
		len -= ret;	
	}
	if (tls_write(ctx, urlbuf, strnlen(urlbuf, MAX_URL)) < strnlen(urlbuf, MAX_URL)) {
		snprintf(gmi_error, sizeof(gmi_error), "Failed to send data to: %s", gmi_host);
		goto request_error;
	}
	char buf[1024];
	while (recv==TLS_WANT_POLLIN || recv==TLS_WANT_POLLOUT || recv == 0)
		recv = tls_read(ctx, buf, sizeof(buf));
	if (recv <= 0) {
		snprintf(gmi_error, sizeof(gmi_error), "[%d] Invalid data from: %s", recv, gmi_host);
		goto request_error;
	}
	ptr = strchr(buf, ' ');
	if (!ptr) {
		snprintf(gmi_error, sizeof(gmi_error), "Invalid data to from: %s", gmi_host);
		goto request_error;
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
		ptr = strchr(gmi_input, '\r');
		if (ptr) *ptr = '\0';
		goto exit;
	case 20:
		break;
	case 30:
		snprintf(gmi_error, sizeof(gmi_error), "Redirect temporary");

		break;
	case 31:
		snprintf(gmi_error, sizeof(gmi_error), "Redirect permanent");
		break;
	case 40:
		snprintf(gmi_error, sizeof(gmi_error), "Temporary failure");
		goto request_error;
	case 41:
		snprintf(gmi_error, sizeof(gmi_error), "Server unavailable");
		goto request_error;
	case 42:
		snprintf(gmi_error, sizeof(gmi_error), "CGI error");
		goto request_error;
	case 43:
		snprintf(gmi_error, sizeof(gmi_error), "Proxy error");
		goto request_error;
	case 44:
		snprintf(gmi_error, sizeof(gmi_error), "Slow down: server above rate limit");
		goto request_error;
	case 50:
		snprintf(gmi_error, sizeof(gmi_error), "Permanent failure");
		goto request_error;
	case 51:
		snprintf(gmi_error, sizeof(gmi_error), "Not found");
		goto request_error;
	case 52:
		snprintf(gmi_error, sizeof(gmi_error), "Resource gone");
		goto request_error;
	case 53:
		snprintf(gmi_error, sizeof(gmi_error), "Proxy request refused");
		goto request_error;
	case 59:
		snprintf(gmi_error, sizeof(gmi_error), "Bad request");
		goto request_error;
	case 60:
		snprintf(gmi_error, sizeof(gmi_error), "Client certificate required");
		goto request_error;
	case 61:
		snprintf(gmi_error, sizeof(gmi_error), "Client not authorised");
		goto request_error;
	case 62:
		snprintf(gmi_error, sizeof(gmi_error), "Client certificate not valid");
		goto request_error;
	default:
		snprintf(gmi_error, sizeof(gmi_error), "Unknown status code: %d", gmi_code);
		goto request_error;
	}
	*ptr = ' ';
	if (!gmi_data) free(gmi_data);
	gmi_data = malloc(recv+1);
	memcpy(gmi_data, buf, recv);
	while (1) {
		int bytes = tls_read(ctx, buf, sizeof(buf));
		if (bytes == 0) break;
		if (recv == TLS_WANT_POLLIN || recv == TLS_WANT_POLLOUT)
			continue;
		if (bytes == -1) {
			snprintf(gmi_error, sizeof(gmi_error), "Invalid data to from: %s", gmi_host);
			goto request_error;
		}
		gmi_data = realloc(gmi_data, recv+bytes+1);
		memcpy(&gmi_data[recv], buf, bytes);
		recv += bytes;
	}
exit:
	if (!gmi_history || (gmi_history && !gmi_history->next)) {
		struct gmi_link* link = malloc(sizeof(struct gmi_link));
		link->next = NULL;
		link->prev = gmi_history;
		strncpy(link->url, gmi_url, sizeof(link->url));
		gmi_history = link;
		if (link->prev)
			link->prev->next = gmi_history;
	}
	if (0) {
request_error:
		recv = -1;
	}
	if (sockfd != -1)
		close(sockfd);
	if (ctx) {
		tls_close(ctx);
		tls_free(ctx);
		ctx = NULL;
	}
	if (gmi_code == 11 || gmi_code == 10) {
		return gmi_data?strlen(gmi_data):0;
	}
	if (gmi_code == 31 || gmi_code == 30) {
		char* ptr = strchr(gmi_data, ' ');
		if (!ptr) return -1;
		char* ln = strchr(ptr+1, ' ');
		if (ln) *ln = '\0';
		ln = strchr(ptr, '\n');
		if (ln) *ln = '\0';
		ln = strchr(ptr, '\r');
		if (ln) *ln = '\0';
		return gmi_nextlink(gmi_url, sizeof(gmi_url), ptr+1, strlen(ptr+1));//gmi_request(urlbuf);
	}
	return recv;
}
