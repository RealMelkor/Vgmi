/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@rmf-dev.com>
 */
#define CONFIG_STRING_LENGTH 1024
struct config {
	unsigned int	maximumBodyLength;
	unsigned int	maximumDisplayLength;
	int		certificateLifespan;
	int		certificateBits;
	int		maximumRedirects;
	int		maximumCachedPages;
	int		maximumHistorySize;
	int		maximumHistoryCache;
	int		imageParserScratchPad;
	int		enableMouse;
	int		enableHistory;
	int		enableSandbox;
#ifdef __linux__
	int		enableLandlock;
#endif
	int		enableImage;
	int		enableHexViewer;
	int		enableXdg;
	int		launcherTerminal;
	char		searchEngineURL[CONFIG_STRING_LENGTH];
	char		downloadsPath[CONFIG_STRING_LENGTH];
	char		proxyHttp[CONFIG_STRING_LENGTH];
	char		launcher[CONFIG_STRING_LENGTH];
};
extern struct config config;

#ifdef CONFIG_INTERNAL
enum {
	VALUE_STRING,
	VALUE_INT
};

struct field {
	char name[128];
	int type;
	void *ptr;
	int restart;
};

static struct field fields[] = {
	{"hexviewer.enabled", VALUE_INT, &config.enableHexViewer, 0},
	{"image.enabled", VALUE_INT, &config.enableImage, 1},
	{"mouse.enabled", VALUE_INT, &config.enableMouse, 0},
#ifdef STATIC_ALLOC
	{"image.scratchpad", VALUE_INT, &config.imageParserScratchPad, 1},
#endif
	{"sandbox.enabled", VALUE_INT, &config.enableSandbox, 1},
#ifdef __linux
	{"sandbox.landlock.enabled", VALUE_INT, &config.enableLandlock, 1},
#endif
	{"xdg.enabled", VALUE_INT, &config.enableXdg, 1},
	{"history.enabled", VALUE_INT, &config.enableHistory, 0},
	{"certificate.bits", VALUE_INT, &config.certificateBits, 0},
	{"certificate.expiration", VALUE_INT, &config.certificateLifespan, 0},
	{"request.maxbody", VALUE_INT, &config.maximumBodyLength, 0},
	{"request.maxdisplay", VALUE_INT, &config.maximumDisplayLength, 0},
	{"request.maxredirects", VALUE_INT, &config.maximumRedirects, 0},
	{"request.cachedpages", VALUE_INT, &config.maximumCachedPages, 0},
	{"history.maxentries", VALUE_INT, &config.maximumHistorySize, 0},
	{"history.maxcache", VALUE_INT, &config.maximumHistoryCache, 0},
	{"search.url", VALUE_STRING, &config.searchEngineURL, 0},
	{"downloads.path", VALUE_STRING, &config.downloadsPath, 1},
	{"proxy.http", VALUE_STRING, &config.proxyHttp, 0},
	{"launcher.executable", VALUE_STRING, &config.launcher, 0},
	{"launcher.terminal", VALUE_INT, &config.launcherTerminal, 0},
};
#endif

int config_load(void);
int config_save(void);
int config_set_field(int id, const char *value);
int config_correction(void);
