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
};

static struct field fields[] = {
	{"hexviewer.enabled", VALUE_INT, &config.enableHexViewer},
	{"image.enabled", VALUE_INT, &config.enableImage},
	{"image.scratchpad", VALUE_INT, &config.imageParserScratchPad},
	{"sandbox.enabled", VALUE_INT, &config.enableSandbox},
	{"history.enabled", VALUE_INT, &config.enableHistory},
	{"certificate.bits", VALUE_INT, &config.certificateBits},
	{"certificate.expiration", VALUE_INT, &config.certificateLifespan},
	{"request.maxbody", VALUE_INT, &config.maximumBodyLength},
	{"request.maxdisplay", VALUE_INT, &config.maximumDisplayLength},
	{"request.maxredirects", VALUE_INT, &config.maximumRedirects},
	{"request.cachedpages", VALUE_INT, &config.maximumCachedPages},
	{"search.url", VALUE_STRING, &config.searchEngineURL},
};
#endif

int config_load();
int config_save();
