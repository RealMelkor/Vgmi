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

int config_load();
int config_save();
