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

typedef struct {
	char model[64];
	int  usb_vendor;
	int  usb_product;
} FujitsuCamera;

void fujitsu_debug_print(FujitsuData *fd, char *message);
