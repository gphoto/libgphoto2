typedef struct {
	int debug;
	int folders;
	int speed;
	int first_packet;
	int type;
	gpio_device* dev;
	char folder[128];
	CameraFilesystem *fs;
} FujitsuData;

void fujitsu_debug_print(FujitsuData *fd, char *message);
