typedef struct {
	int folders;
	int speed;
	int first_packet;
	int type;
	gp_port* dev;
	char folder[128];
	CameraFilesystem *fs;
} SierraData;

typedef struct {
	char model[64];
	int  usb_vendor;
	int  usb_product;
	int  usb_inep;
	int  usb_outep;
} SierraCamera;

void sierra_debug_print(SierraData *fd, char *message);
