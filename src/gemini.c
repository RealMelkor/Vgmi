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
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#include <pthread.h>
#include <sys/socket.h>
#endif
#include "cert.h"

#define TIMEOUT 3

struct tls_config* config;
struct tls* ctx;
struct gmi_client client;

int gmi_goto(int id) {
	id--;
	struct gmi_page* page = &client.tabs[client.tab].page;
	if (id < 0 || id >= page->links_count) {
		snprintf(client.error, sizeof(client.error),
			 "Invalid link number, %d/%d", id, page->links_count);
		client.input.error = 1;
		return -1;
	}
	gmi_cleanforward(&client.tabs[client.tab]);
	int ret = gmi_nextlink(client.tabs[client.tab].url, page->links[id]);
	if (ret < 1) return ret;
	client.tabs[client.tab].page.data_len = ret;
	gmi_load(page);
	client.tabs[client.tab].scroll = -1;
	return ret;
}

int gmi_goto_new(int id) {
	id--;
	struct gmi_page* page = &client.tabs[client.tab].page;
	if (id < 0 || id >= page->links_count) {
		snprintf(client.error, sizeof(client.error),
			 "Invalid link number, %d/%d", id, page->links_count);
		client.input.error = 1;
		return -1;
	}
	int old_tab = client.tab;
	gmi_newtab();
	client.tab = client.tabs_count - 1;
	page = &client.tabs[old_tab].page;
	int ret = gmi_nextlink(client.tabs[old_tab].url, page->links[id]);
	if (ret < 1) return ret;
	client.tabs[client.tab].page.data_len = ret;
	gmi_load(&client.tabs[client.tab].page);
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
		char* ptr = strrchr(&url[GMI], '/');
		if (ptr) *ptr = '\0';
		char urlbuf[MAX_URL];
		strncpy(urlbuf, url, sizeof(urlbuf));
		if (url[strlen(url)-1] != '/')
			strncat(urlbuf, "/", sizeof(urlbuf)-strnlen(urlbuf, MAX_URL)-1);
		strncat(urlbuf, link, sizeof(urlbuf)-strlen(link)-1);
		int l = strnlen(urlbuf, MAX_URL);
		if (urlbuf[l-1] == '/') urlbuf[l-1] = '\0';
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
	int x = 0;
	for (int c = 0; c < page->data_len; c++) {
		if (x == 0 && page->data[c] == '=' && page->data[c+1] == '>') {
			c += 2;
			char* url = (char*)&page->data[c];
			int nospace = c;
			for (; page->data[c]==' '; c++) {
				if (page->data[c] == '\n' || page->data[c] == '\0') {
					c = nospace;
					break;
				}
			}
			url = (char*)&page->data[c];
			for (; page->data[c]!=' '; c++) {
				if (page->data[c] == '\n' || page->data[c] == '\0') {
					break;
				}
			}
			char save = page->data[c];
			page->data[c] = '\0';
			if (page->links)
				page->links = realloc(page->links, sizeof(char*) 
						      * (page->links_count+1));
			else
				page->links = malloc(sizeof(char*));
			if (url[0] == '\0') {
				page->links[page->links_count] = NULL;
				page->data[c] = save;
				continue;
			}
			int len = strnlen(url, MAX_URL);
			page->links[page->links_count] = malloc(len+2);
			memcpy(page->links[page->links_count], url, len+1);
			page->links_count++;
			page->data[c] = save;
		}
		if (page->data[c] == '\n') {
			page->lines++;
			x = 0;
			continue;
		}
		x++;
	}
}

int gmi_render(struct gmi_tab* tab) {
	int line = 0;
	int x = 0;
	int links = 0;
	uintattr_t color = TB_DEFAULT;
	int start = 1;
	int ignore = 0;
	for (int c = 0; c < tab->page.data_len; c++) {
		if (tab->page.data[c] == '\t') {
			x+=4;
			continue;
		}
		if (tab->page.data[c] == '\r') continue;
		if (start && tab->page.data[c] == '`' &&
		    tab->page.data[c+1] == '`' && tab->page.data[c+2] == '`') 
			ignore = !ignore;	
		
		if (!ignore) {
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
				if (line-1>=(tab->scroll>=0?tab->scroll:0) &&
				   line-tab->scroll <= tb_height()-2) {
					char buf[32];
					snprintf(buf, sizeof(buf), "[%d]", links+1);
					tb_print(x+2, line-1-tab->scroll,
					links+1 == tab->selected?TB_RED:TB_BLUE, TB_DEFAULT, buf);
					x += strlen(buf);
				}
				c += 2;

				for (; tab->page.data[c]==' ' && 
				tab->page.data[c]!='\n' &&
				tab->page.data[c]!='\0'; c++) ;

				int initial = c;
				for (; tab->page.data[c]!=' ' &&
				tab->page.data[c]!='\n' &&
				tab->page.data[c]!='\0'; c++) ;

				for (; tab->page.data[c]==' ' && 
				tab->page.data[c]!='\n' &&
				tab->page.data[c]!='\0'; c++) ;

				if (tab->page.data[c]=='\n' || tab->page.data[c]=='\0') c = initial;
				x+=3;
				if ((links+1)/10) x--;
				if ((links+1)/100) x--;
				if ((links+1)/1000) x--;
				links++;
			}
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
				if (tab->page.data[c+1] == ' ') c++;
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

void gmi_freetab(struct gmi_tab* tab) {
	if (tab->history) {
		struct gmi_link* link = tab->history->next;
		while (link) {
			struct gmi_link* ptr = link->next;
			free(link);
			link = ptr;
		}
		link = tab->history->prev;
		while (link) {
			struct gmi_link* ptr = link->prev;
			free(link);
			link = ptr;
		}
		free(tab->history);
	}
	for (int i=0; i < tab->page.links_count; i++)
		free(tab->page.links[i]);
	free(tab->page.links);
	free(tab->page.data);
}

char* home_page = 
"20\r\n# Vgmi\n\n" \
"A Gemini client written in C with vim-like keybindings\n\n" \
"## Keybindings\n\n" \
"* h - Go back in the history\n" \
"* l - Go forward in the history\n" \
"* k - Scroll up\n" \
"* j - Scroll down\n" \
"* H - Switch to the previous tab\n" \
"* L - Switch to the next tab\n" \
"* gg - Go at the top of the page\n" \
"* G - Go at the bottom of the page\n" \
"* : - Open input mode\n" \
"* u - Open input mode with the current url\n" \
"* r - Reload the page\n" \
"* [number]Tab - Select a link\n" \
"* Tab - Follow the selected link\n" \
"* Shift+Tab - Open the selected link in a new tab\n" \
"\nYou can prefix a movement key with a number to repeat it.\n\n" \
"## Commands\n\n" \
"* :q - Close the current tab\n" \
"* :qa - Close all tabs, exit the program\n" \
"* :o [url] - Open an url\n" \
"* :s [search] - Search the Geminispace using geminispace.info\n" \
"* :nt [url] - Open a new tab, the url is optional\n" \
"* :[number] - Follow the link\n\n"
"## Links\n\n" \
"=>gemini://geminispace.info Geminispace\n" \
"=>gemini://gemini.rmf-dev.com Gemigit\n" \
;

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

int gmi_parseurl(const char* url, char* host, int host_len, char* urlbuf,
		 int url_len, unsigned short* port) {
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
	if (port && proto == PROTO_GEMINI) *port = 1965;
	if (!proto_ptr) proto_ptr = ptr;
	char* host_ptr = strchr(ptr, '/');
	if (!host_ptr) host_ptr = ptr+strlen(ptr);
	char* port_ptr = strchr(ptr, ':');
	if (port_ptr && port_ptr < host_ptr) {
		port_ptr++;	
		char c = *host_ptr;
		*host_ptr = '\0';
		if (port) {
			*port = atoi(port_ptr);
			if (*port < 1) {
				return -1; // invalid port
			}
		}
		*host_ptr = c;
		host_ptr = port_ptr - 1;
	}
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
	if (!urlbuf) return proto;
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

#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
struct conn {
	int connected;
	int socket;
	struct sockaddr* addr;
	int family;
	struct sockaddr_in addr4;
	struct sockaddr_in6 addr6;
	pthread_t tid;
	pthread_cond_t cond;
	pthread_mutex_t mutex;
};
struct conn conn;

void conn_thread() {
	if (pthread_mutex_init(&conn.mutex, NULL)) {
		snprintf(client.error, sizeof(client.error), 
			"Failed to initialize connection mutex\n");
		return;
	}
	pthread_mutex_lock(&conn.mutex);
	while (1) {
		pthread_cond_wait(&conn.cond, &conn.mutex);
		if (connect(conn.socket, conn.addr, 
		(conn.family == AF_INET)?sizeof(conn.addr4):sizeof(conn.addr6)) != 0) {
			conn.connected = -1;
			continue;
		}
		conn.connected = 1;
	}
}

struct dnsquery {
	struct addrinfo* result;
	int resolved;
	char* host;
	pthread_t tid;
	pthread_cond_t cond;
	pthread_mutex_t mutex;
};
struct dnsquery dnsquery;

void dnsquery_thread(int signal) {
	if (pthread_mutex_init(&dnsquery.mutex, NULL)) {
		snprintf(client.error, sizeof(client.error), 
			"Failed to initialize connection mutex\n");
		return;
	}
	pthread_mutex_lock(&dnsquery.mutex);
	while (1) {
		pthread_cond_wait(&dnsquery.cond, &dnsquery.mutex);
		struct addrinfo hints;
		memset (&hints, 0, sizeof (hints));
		hints.ai_family = PF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags |= AI_CANONNAME;
		if (getaddrinfo(dnsquery.host, NULL, &hints, &dnsquery.result)) {
			dnsquery.resolved = -1;
			continue;
		}
		dnsquery.resolved = 1;
	}
}

void signal_cb() {
	pthread_t thread = pthread_self();
	if (thread == conn.tid)
		pthread_mutex_destroy(&conn.mutex);
	if (thread == dnsquery.tid)
		pthread_mutex_destroy(&dnsquery.mutex);
	pthread_exit(NULL);
}

#endif

int gmi_request(const char* url) {
	char* data_ptr = NULL;
	struct gmi_tab* tab = &client.tabs[client.tab];
	tab->selected = 0;
	int recv = TLS_WANT_POLLIN;
	int sockfd = -1;
	char gmi_host[256];
	unsigned short port;
	int proto = gmi_parseurl(url, gmi_host, sizeof(gmi_host),
				 tab->url, sizeof(tab->url), &port);
	if (proto == PROTO_FILE) {
		if (gmi_loadfile(&tab->url[P_FILE]) > 0)
			gmi_load(&tab->page);
		return 0;
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

	int cert = 0;
	char crt[1024];
	char key[1024];
	if (cert_getpath(gmi_host, crt, sizeof(crt), key, sizeof(key)) == -1) {
		client.input.error = 1;
		cert = -1;
	}

	if (cert == 0) {
		FILE* f = fopen(crt, "rb");
		if (f) {
			cert = 1;
			fclose(f);
		}
	}

	if (cert == 1 && tls_config_set_keypair_file(config, crt, key)) {
		client.input.error = 1;
		snprintf(client.error, sizeof(client.error), "%s", tls_config_error(config));
		return -1;
	}

	if (tls_configure(ctx, config)) {
		snprintf(client.error, sizeof(client.error), "Failed to configure TLS");
		client.input.error = 1;
		return -1;
	}

	// Manually create socket to set a timeout
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
	dnsquery.host = gmi_host;
	dnsquery.resolved = 0;
	pthread_cond_signal(&dnsquery.cond);
	for (int i=0; i < TIMEOUT * 1000 && !dnsquery.resolved; i++)
		usleep(1000);
	
	if (dnsquery.resolved != 1 || dnsquery.result == NULL) {
		if (!dnsquery.resolved) {
			pthread_cancel(dnsquery.tid);
			pthread_kill(dnsquery.tid, SIGUSR1);
			pthread_join(dnsquery.tid, NULL);
			pthread_create(&dnsquery.tid, NULL, (void*)(void*)dnsquery_thread, NULL);
		}
		snprintf(client.error, sizeof(client.error), "Unknown domain name: %s", gmi_host);
		goto request_error;
	}
	struct addrinfo *result = dnsquery.result;
#else
	struct addrinfo hints, *result;
	memset (&hints, 0, sizeof (hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags |= AI_CANONNAME;
	if (getaddrinfo(gmi_host, NULL, &hints, &result)) {
		snprintf(client.error, sizeof(client.error), "Unknown domain name: %s", gmi_host);
		goto request_error;
	}
#endif

	struct sockaddr_in addr4;
	struct sockaddr_in6 addr6;
	sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	struct timeval tv;
	tv.tv_sec = TIMEOUT;
	tv.tv_usec = 0;
	int value = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv);
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
	struct sockaddr* addr;
	if (result->ai_family == AF_INET) {
		addr4.sin_addr = ((struct sockaddr_in*) result->ai_addr)->sin_addr;
		addr4.sin_family = AF_INET;
		addr4.sin_port = htons(port);
		addr = (struct sockaddr*)&addr4;
	}
	else if (result->ai_family == AF_INET6) {
		addr6.sin6_addr = ((struct sockaddr_in6*) result->ai_addr)->sin6_addr;
		addr6.sin6_family = AF_INET6;
		addr6.sin6_port = htons(port);
		addr = (struct sockaddr*)&addr6;
	}
	int family = result->ai_family;
	freeaddrinfo(result);
	
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
	conn.connected = 0;
	conn.addr = addr;
	conn.addr4 = addr4;
	conn.addr6 = addr6;
	conn.family = family;
	conn.socket = sockfd;
	pthread_cond_signal(&conn.cond);
	for (int i=0; i < TIMEOUT * 1000 && !conn.connected; i++)
		usleep(1000);
	if (conn.connected != 1) {
		if (!conn.connected) {
			pthread_cancel(conn.tid);
			pthread_kill(conn.tid, SIGUSR1);
			pthread_join(conn.tid, NULL);
			pthread_create(&conn.tid, NULL, (void*)(void*)conn_thread, NULL);
		}
#else
	if (connect(sockfd, addr, (family == AF_INET)?sizeof(addr4):sizeof(addr6)) != 0) {
#endif
		snprintf(client.error, sizeof(client.error), 
			 "Connection to %s timed out", gmi_host);
		goto request_error;
	}

	if (tls_connect_socket(ctx, sockfd, gmi_host)) {
		snprintf(client.error, sizeof(client.error), 
			 "Unable to connect to: %s", gmi_host);
		goto request_error;
	}
	if (tls_handshake(ctx)) {
		snprintf(client.error, sizeof(client.error),
			 "Failed to handshake: %s", gmi_host);
		goto request_error;
	}
	char urlbuf[MAX_URL];
	strncpy(urlbuf, tab->url, MAX_URL);
	size_t len = strnlen(urlbuf, MAX_URL)+2;
	strncat(urlbuf, "\r\n", MAX_URL-len);
	if (tls_write(ctx, urlbuf, strnlen(urlbuf, MAX_URL)) < strnlen(urlbuf, MAX_URL)) {
		snprintf(client.error, sizeof(client.error),
			 "Failed to send data to: %s", gmi_host);
		goto request_error;
	}
	char buf[1024];
	while (recv==TLS_WANT_POLLIN || recv==TLS_WANT_POLLOUT || recv == 0)
		recv = tls_read(ctx, buf, sizeof(buf));
	if (recv <= 0) {
		snprintf(client.error, sizeof(client.error),
			 "[%d] Invalid data from: %s", recv, gmi_host);
		goto request_error;
	}
	char* ptr = strchr(buf, ' ');
	if (!ptr) {
		snprintf(client.error, sizeof(client.error),
			 "Invalid data to from: %s", gmi_host);
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
		data_ptr = tab->page.data;	
		tab->page.data = NULL;
		snprintf(client.error, sizeof(client.error), "Redirect temporary");
		break;
	case 31:
		data_ptr = tab->page.data;	
		tab->page.data = NULL;
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
		snprintf(client.error, sizeof(client.error),
			 "Unknown status code: %d", tab->page.code);
		goto request_error;
	}
	*ptr = ' ';
	if (tab->page.data) free(tab->page.data);
	tab->page.data = malloc(recv+1);
	memcpy(tab->page.data, buf, recv);
	while (1) {
		int bytes = tls_read(ctx, buf, sizeof(buf));
		if (bytes == 0) break;
		if (bytes == TLS_WANT_POLLIN || bytes == TLS_WANT_POLLOUT) continue;
		if (bytes < 1) {
			snprintf(client.error, sizeof(client.error),
				 "Invalid data to from %s: %s", gmi_host, tls_error(ctx));
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
		return tab->page.data_len;
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
		int r = gmi_nextlink(tab->url, ptr+1);
		if (r < 1) return r;
		if (tab->page.code != 20) {
			free(tab->page.data);
			tab->page.data = data_ptr;
		} else {
			tab->page.data_len = r;
			gmi_load(&tab->page);
			tab->scroll = -1;
		}
		return r;
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
	if (tab->page.data) free(tab->page.data);
	tab->page.data = data;
	snprintf(tab->url, sizeof(tab->url), "file://%s/", path);
	return len;
}

int gmi_init() {
	srand(time(NULL));
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
	bzero(&client, sizeof(client));

#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
	bzero(&conn, sizeof(conn));
	if (pthread_cond_init(&conn.cond, NULL)) {                                    
		printf("Failed to initialize pthread condition\n");
		return -1;
	}
	if (pthread_create(&conn.tid, NULL, (void*)(void*)conn_thread, NULL)) {
		printf("Failed to initialize connection thread\n");
		return -1;
	}
	bzero(&dnsquery, sizeof(dnsquery));
	if (pthread_cond_init(&dnsquery.cond, NULL)) {                                    
		printf("Failed to initialize thread condition\n");
		return -1;
	}
	if (pthread_create(&dnsquery.tid, NULL, (void*)(void*)dnsquery_thread, NULL)) {
		printf("Failed to initialize dns query thread\n");
		return -1;
	}
	signal(SIGUSR1, signal_cb);
#endif
		
	return 0;
}

void gmi_free() {
	for (int i=0; i < client.tabs_count; i++) gmi_freetab(&client.tabs[i]);
	free(client.tabs);
	pthread_kill(conn.tid, SIGUSR1);
	pthread_join(conn.tid, NULL);
	pthread_kill(dnsquery.tid, SIGUSR1);
	pthread_join(dnsquery.tid, NULL);
}

