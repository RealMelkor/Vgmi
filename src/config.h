/*
 * ISC License
 * Copyright (c) 2023 RMF <rawmonk@firemail.cc>
 */
#define CONFIG_STRING_LENGTH 1024
struct config {
	unsigned int	maximumBodyLength;
	unsigned int	maximumDisplayLength;
	int		certificateLifespan;
	int		certificateBits;
	int		maximumRedirects;
	int		maximumCachedPages;
	int		imageParserScratchPad;
	int		enableHistory;
	int		enableSandbox;
	int		enableImage;
	int		enableHexViewer;
	int		enableXdg;
	char		searchEngineURL[CONFIG_STRING_LENGTH];
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
	{"image.scratchpad", VALUE_INT, &config.imageParserScratchPad, 1},
	{"sandbox.enabled", VALUE_INT, &config.enableSandbox, 1},
	{"xdg.enabled", VALUE_INT, &config.enableXdg, 1},
	{"history.enabled", VALUE_INT, &config.enableHistory, 0},
	{"certificate.bits", VALUE_INT, &config.certificateBits, 0},
	{"certificate.expiration", VALUE_INT, &config.certificateLifespan, 0},
	{"request.maxbody", VALUE_INT, &config.maximumBodyLength, 0},
	{"request.maxdisplay", VALUE_INT, &config.maximumDisplayLength, 0},
	{"request.maxredirects", VALUE_INT, &config.maximumRedirects, 0},
	{"request.cachedpages", VALUE_INT, &config.maximumCachedPages, 0},
	{"search.url", VALUE_STRING, &config.searchEngineURL, 0},
};
#endif

int config_load();
int config_save();
int config_set_field(int id, const char *value);
int config_correction();
