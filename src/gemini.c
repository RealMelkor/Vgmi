/* See LICENSE file for copyright and license details. */
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
struct gmi_client client;

int gmi_goto(int id) {
	id--;
	struct gmi_page* page = &client.tabs[client.tab].page;
	if (id < 0 || id >= page->links_count) {
		snprintf(client.error, sizeof(client.error), "Invalid link number, %d/%d", id, page->links_count);
		client.input.error = 1;
		return -1;
	}
	gmi_cleanforward(&client.tabs[client.tab]);
	int ret = gmi_nextlink(client.tabs[client.tab].url, page->links[id]);
	if (ret < 1) return ret;
	client.tabs[client.tab].page.data_len = ret;
	gmi_load(page);
	return ret;
}

int gmi_nextlink(char* url, char* link) {
	int url_len = strnlen(url, MAX_URL);
	int link_len = strnlen(url, MAX_URL);
	if (link[0] == '/' && link[1] == '/') {
		int ret = gmi_request(&link[2]);
		if (ret < 1) return ret;
		return ret;
	} else if (link[0] == '/') {
		if (url_len > GMI && strstr(url, "gemini://")) {
			char* ptr = strchr(&url[GMI], '/');
			if (ptr) *ptr = '\0';
		}
		char urlbuf[MAX_URL];
		strncpy(urlbuf, url, sizeof(urlbuf));
		strncat(urlbuf, link, sizeof(urlbuf)-strlen(link)-1);
		int ret = gmi_request(urlbuf);
		return ret;
	} else if (strstr(link, "gemini://")) {
		int ret = gmi_request(link);
		return ret;
	} else {
		char* ptr = strchr(&url[GMI], '/');
		if (ptr) *ptr = '\0';
		char urlbuf[MAX_URL];
		strncpy(urlbuf, url, sizeof(urlbuf));
		strncat(urlbuf, "/", sizeof(urlbuf)-strnlen(urlbuf, MAX_URL)-1);
		strncat(urlbuf, link, sizeof(urlbuf)-strlen(link)-1);
		int ret = gmi_request(urlbuf);
		return ret;
	}
}

void gmi_load(struct gmi_page* page) {
	for (int i=0; i<page->links_count; i++)
		free(page->links[i]);
	free(page->links);
	page->links = NULL;
	page->links_count = 0;
	page->lines = 0;
	for (int c = 0; c < page->data_len; c++) {
		if (page->data[c] == '\n') {
			page->lines++;
			continue;
		}
		if (page->data[c] == '=' && page->data[c+1] == '>') {
			c += 2;
			for (; page->data[c]==' '; c++) ;
			char* url = (char*)&page->data[c];
			for (; page->data[c]!=' '; c++) ;
			page->data[c] = '\0';
			if (page->links)
				page->links = realloc(page->links, sizeof(char*) * (page->links_count+1));
			else
				page->links = malloc(sizeof(char*));
			if (url[0] == '\0') {
				page->links[page->links_count] = NULL;
				page->data[c] = ' ';
				continue;
			}
			int len = strnlen(url, MAX_URL);
			page->links[page->links_count] = malloc(len+2);
			memcpy(page->links[page->links_count], url, len+1);
			page->links_count++;
			page->data[c] = ' ';
		}
	}
}

int gmi_render(struct gmi_tab* tab) {
	int line = 0;
	int x = 0;
	int links = 0;
	uintattr_t color = TB_DEFAULT;
	int start = 1;
	for (int c = 0; c < tab->page.data_len; c++) {
		if (tab->page.data[c] == '\t') {
			x+=4;
			continue;
		}
		if (tab->page.data[c] == '\r') continue;
		for (int i=0; start && tab->page.data[c+i] == '#' && i<3; i++) {
			if (tab->page.data[c+i+1] == ' ') {
				color = TB_RED + i;
				break;
			}
		}
		if (start && tab->page.data[c] == '*' && tab->page.data[c+1] == ' ') {
			color = TB_ITALIC|TB_CYAN;
		}
		if (start && tab->page.data[c] == '>' && tab->page.data[c+1] == ' ') {
			color = TB_ITALIC|TB_MAGENTA;
		}
		if (start && tab->page.data[c] == '=' && tab->page.data[c+1] == '>') {
			if (line-1>=(tab->scroll>=0?tab->scroll:0) && (line-tab->scroll <= tb_height()-2)) {
				char buf[32];
				snprintf(buf, sizeof(buf), "[%d]", links+1);
				tb_print(x+2, line-1-tab->scroll, links+1 == tab->selected?TB_RED:TB_BLUE, TB_DEFAULT, buf);
				x += strlen(buf);
			}
			c += 2;
			for (; tab->page.data[c]==' '; c++) ;
			for (; tab->page.data[c]!=' '; c++) ;
			for (; tab->page.data[c]==' '; c++) ;
			x+=3;
			if ((links+1)/10) x--;
			if ((links+1)/100) x--;
			if ((links+1)/1000) x--;
			links++;
		}
		if (tab->page.data[c] == '\n' || tab->page.data[c] == ' ' || x+4 >= tb_width()) {
			int end = 0;
			if (x+4>=tb_width()) {
				c--;
				end = 1;
			}
			int newline = (tab->page.data[c] == '\n' || x+4 >= tb_width());
			for (int i=1; 1; i++) {
				if (x+i == tb_width() - 1) break;
				if (i > tb_width() - 4) {
					newline = 0;
					break;
				}
				if (tab->page.data[c+i] == ' ' || tab->page.data[c+i] == '\n' ||
				    tab->page.data[c+i] == '\0' || c+i >= tab->page.data_len)
					break;
				if (tb_width()-4<=x+i) newline = 1;
			}
			if (newline) {
				line++;
				if (tab->page.data[c] == '\n') {
					color = TB_DEFAULT;
					start = 1;
				}
				x = 0;
				continue;
			} else if (end) {
				c++;
				x++;
			}
		}
		uint32_t ch = 0;
		c += tb_utf8_char_to_unicode(&ch, &tab->page.data[c])-1;

		if (line-1>=(tab->scroll>=0?tab->scroll:0) && (line-tab->scroll <= tb_height()-2)) 
			tb_set_cell(x+2, line-1-tab->scroll, ch, color, TB_DEFAULT);
		x++;
		start = 0;
	}
	return line;
}

void gmi_cleanforward(struct gmi_tab* tab) {
	struct gmi_link* ptr = tab->history;
	while (ptr && ptr->next) {
		struct gmi_link* next = ptr->next;
		if (tab->history != ptr)
			free(ptr);
		if (!next) break;
		ptr = next;
	}
	if (tab->history)
		tab->history->next = NULL;
}

void gmi_freehistory(struct gmi_tab* tab) {
	struct gmi_link* ptr = tab->history->prev;
	free(tab->history);
	while ((ptr = ptr->prev)) {
		tab->history = ptr;
		ptr = ptr->prev;
		free(tab->history);
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
	//if (tls_config_set_keypair_file(config, "cert.crt", "cert.key")) {
	if (tls_config_set_keypair_file(config, "txt.rmf-dev.com.crt", "txt.rmf-dev.com.key")) {
		printf("Failed to load keypair\n");
		return -1;
	}
	tls_config_insecure_noverifycert(config);
	ctx = NULL;
	bzero(&client, sizeof(client));
		
	return 0;
}

char* home_page = 
"20\r\n# Home Page - Vgmi client\n\n" \
"Welcome\n";

void gmi_newtab() {
	int tab = client.tabs_count;
	client.tabs_count++;
	if (client.tabs) 
		client.tabs = realloc(client.tabs, sizeof(struct gmi_tab) * client.tabs_count);
	else
		client.tabs = malloc(sizeof(struct gmi_tab));
	bzero(&client.tabs[tab], sizeof(struct gmi_tab));
	client.tabs[tab].scroll = -1;
	strncpy(client.tabs[tab].url, "about:home", sizeof(client.tabs[tab].url)); 
	client.tabs[tab].page.code = 20;
	int len = strlen(home_page);
	client.tabs[tab].page.data = malloc(len + 1);
	strncpy(client.tabs[tab].page.data, home_page, len);
	client.tabs[tab].page.data_len = len;
	gmi_load(&client.tabs[tab].page);
}

void gmi_newtab_url(char* url) {
	int tab = client.tab;
	client.tab++;
	if (client.tabs) 
		client.tabs = realloc(client.tabs, sizeof(struct gmi_tab) * client.tab);
	else
		client.tabs = malloc(sizeof(struct gmi_tab));
	bzero(&client.tabs[tab], sizeof(struct gmi_tab));
	strncpy(client.tabs[tab].url, "about:home", sizeof(client.tabs[tab].url)); 
	client.tabs[tab].page.code = 20;
	int len = strlen(home_page);
	client.tabs[tab].page.data = malloc(len + 1);
	strncpy(client.tabs[tab].page.data, home_page, len);
	client.tabs[tab].page.data_len = len;
	gmi_load(&client.tabs[tab].page);

}

#define PROTO_GEMINI 0
#define PROTO_HTTP 1
#define PROTO_HTTPS 2
#define PROTO_GOPHER 3
#define PROTO_FILE 4

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
	else if (!strcmp(proto_buf,"file")) proto = PROTO_FILE;
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
	case PROTO_FILE:
		strncpy(urlbuf, "file://", url_len);
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
	struct gmi_tab* tab = &client.tabs[client.tab];
	tab->selected = 0;
	int recv = TLS_WANT_POLLIN;
	int sockfd = -1;
	char gmi_host[256];
	int proto = gmi_parseurl(url, gmi_host, sizeof(gmi_host), tab->url, sizeof(tab->url));
	if (proto == PROTO_FILE) {
		if (gmi_loadfile(&tab->url[P_FILE]) > 0)
			gmi_load(&tab->page);
	}
	if (proto != PROTO_GEMINI) {
		snprintf(client.error, sizeof(client.error), "Invalid url: %s", url);
		client.input.error = 1;
		return -1;
	}
	if (ctx) {
		tls_close(ctx);
		ctx = NULL;
	}
	ctx = tls_client();
	if (!ctx) {
		snprintf(client.error, sizeof(client.error), "Failed to initialize TLS");
		client.input.error = 1;
		return -1;
	}
	if (tls_configure(ctx, config)) {
		snprintf(client.error, sizeof(client.error), "Failed to configure TLS");
		client.input.error = 1;
		return -1;
	}

	// Manually create socket to set a timeout
	struct addrinfo hints, *result;
	memset (&hints, 0, sizeof (hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags |= AI_CANONNAME;
	if (getaddrinfo(gmi_host, NULL, &hints, &result)) {
		snprintf(client.error, sizeof(client.error), "Unknown domain name: %s", gmi_host);
		goto request_error;
		//client.input.error = 1;
		//return -1;
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
		snprintf(client.error, sizeof(client.error), "Connection to %s timed out", gmi_host);
		//close(sockfd);
		goto request_error;
		//return -1;
	}

	if (tls_connect_socket(ctx, sockfd, gmi_host)) {
		snprintf(client.error, sizeof(client.error), "Unable to connect to: %s", gmi_host);
		goto request_error;
	}
	if (tls_handshake(ctx)) {
		snprintf(client.error, sizeof(client.error), "Failed to handshake: %s", gmi_host);
		goto request_error;
	}
	char urlbuf[MAX_URL];
	strncpy(urlbuf, tab->url, MAX_URL);
	size_t len = strnlen(urlbuf, MAX_URL)+2;
	strncat(urlbuf, "\r\n", MAX_URL-len);
	char* ptr = urlbuf;
	while (len) {
		size_t ret;

		ret = tls_write(ctx, ptr, len);
		if (ret == TLS_WANT_POLLIN || ret == TLS_WANT_POLLOUT)
			continue;
		if (ret == -1) {
			snprintf(client.error, sizeof(client.error), "Failed to send request: %s", gmi_host);
			goto request_error;
		}
		ptr += ret;
		len -= ret;	
	}
	if (tls_write(ctx, urlbuf, strnlen(urlbuf, MAX_URL)) < strnlen(urlbuf, MAX_URL)) {
		snprintf(client.error, sizeof(client.error), "Failed to send data to: %s", gmi_host);
		goto request_error;
	}
	char buf[1024];
	while (recv==TLS_WANT_POLLIN || recv==TLS_WANT_POLLOUT || recv == 0)
		recv = tls_read(ctx, buf, sizeof(buf));
	if (recv <= 0) {
		snprintf(client.error, sizeof(client.error), "[%d] Invalid data from: %s", recv, gmi_host);
		goto request_error;
	}
	ptr = strchr(buf, ' ');
	if (!ptr) {
		snprintf(client.error, sizeof(client.error), "Invalid data to from: %s", gmi_host);
		goto request_error;
	}
	*ptr = '\0';
	tab->page.code = atoi(buf);
	switch (tab->page.code) {
	case 10:
	case 11:
		ptr++;
		strncpy(client.input.label, ptr, sizeof(client.input.label));
		ptr = strchr(client.input.label, '\n');
		if (ptr) *ptr = '\0';
		ptr = strchr(client.input.label, '\r');
		if (ptr) *ptr = '\0';
		goto exit;
	case 20:
		break;
	case 30:
		snprintf(client.error, sizeof(client.error), "Redirect temporary");
		break;
	case 31:
		snprintf(client.error, sizeof(client.error), "Redirect permanent");
		break;
	case 40:
		snprintf(client.error, sizeof(client.error), "Temporary failure");
		goto request_error;
	case 41:
		snprintf(client.error, sizeof(client.error), "Server unavailable");
		goto request_error;
	case 42:
		snprintf(client.error, sizeof(client.error), "CGI error");
		goto request_error;
	case 43:
		snprintf(client.error, sizeof(client.error), "Proxy error");
		goto request_error;
	case 44:
		snprintf(client.error, sizeof(client.error), "Slow down: server above rate limit");
		goto request_error;
	case 50:
		snprintf(client.error, sizeof(client.error), "Permanent failure");
		goto request_error;
	case 51:
		snprintf(client.error, sizeof(client.error), "Not found");
		goto request_error;
	case 52:
		snprintf(client.error, sizeof(client.error), "Resource gone");
		goto request_error;
	case 53:
		snprintf(client.error, sizeof(client.error), "Proxy request refused");
		goto request_error;
	case 59:
		snprintf(client.error, sizeof(client.error), "Bad request");
		goto request_error;
	case 60:
		snprintf(client.error, sizeof(client.error), "Client certificate required");
		goto request_error;
	case 61:
		snprintf(client.error, sizeof(client.error), "Client not authorised");
		goto request_error;
	case 62:
		snprintf(client.error, sizeof(client.error), "Client certificate not valid");
		goto request_error;
	default:
		snprintf(client.error, sizeof(client.error), "Unknown status code: %d", tab->page.code);
		goto request_error;
	}
	*ptr = ' ';
	if (!tab->page.data) free(tab->page.data);
	tab->page.data = malloc(recv+1);
	memcpy(tab->page.data, buf, recv);
	while (1) {
		int bytes = tls_read(ctx, buf, sizeof(buf));
		if (bytes == 0) break;
		if (bytes == TLS_WANT_POLLIN || bytes == TLS_WANT_POLLOUT)
			continue;
		if (bytes < 1) {
			snprintf(client.error, sizeof(client.error), "Invalid data to from: %s", gmi_host);
			goto request_error;
		}
		tab->page.data = realloc(tab->page.data, recv+bytes+1);
		memcpy(&tab->page.data[recv], buf, bytes);
		recv += bytes;
	}
exit:
	if (!tab->history || (tab->history && !tab->history->next)) {
		struct gmi_link* link = malloc(sizeof(struct gmi_link));
		link->next = NULL;
		link->prev = tab->history;
		strncpy(link->url, tab->url, sizeof(link->url));
		tab->history = link;
		if (link->prev)
			link->prev->next = tab->history;
	}
	if (0) {
request_error:
		if (tab->history) {
			strncpy(tab->url, tab->history->url, sizeof(tab->url));
		} else {
			strncpy(tab->url, "about:home", sizeof(tab->url));
		}
		client.input.error = 1;
		recv = -1;
	}
	if (sockfd != -1)
		close(sockfd);
	if (ctx) {
		tls_close(ctx);
		tls_free(ctx);
		ctx = NULL;
	}
	if (tab->page.code == 11 || tab->page.code == 10) {
		return tab->page.data?strlen(tab->page.data):0;
	}
	if (tab->page.code == 31 || tab->page.code == 30) {
		char* ptr = strchr(tab->page.data, ' ');
		if (!ptr) return -1;
		char* ln = strchr(ptr+1, ' ');
		if (ln) *ln = '\0';
		ln = strchr(ptr, '\n');
		if (ln) *ln = '\0';
		ln = strchr(ptr, '\r');
		if (ln) *ln = '\0';
		return gmi_nextlink(tab->url, ptr+1);
	}
	if (recv > 0) {
		tab->page.data_len = recv;
		gmi_load(&tab->page);
	}
	return recv;
}

int gmi_loadfile(char* path) {
	struct gmi_tab* tab = &client.tabs[client.tab];
	FILE* f = fopen(path, "rb");
	if (!f) {
		snprintf(client.error, sizeof(client.error), "Failed to open %s", path);
		client.input.error = 1;
		return -1;
	}
	fseek(f, 0, SEEK_END);
	int len = ftell(f);
	fseek(f, 0, SEEK_SET);
	char* data = malloc(len+1);
	data[0] = '\n';
	if (len != fread(&data[1], 1, len, f)) {
		fclose(f);
		free(data);
		return -1;
	}
	fclose(f);
	tab->page.code = 20;
	tab->page.data_len = len;
	tab->page.data = data;
	snprintf(tab->url, sizeof(tab->url), "file://%s/", path);
	return len;
}
