typedef struct {
	int folders;
	int speed;
	int first_packet;
	int usb_wrap;  /* 0: packets sent "raw", 1: see sierra-usbwrap.h */
	char folder[128];
	CameraFilesystem *fs;
} SierraData;

typedef struct {
	char model[64];
	int  usb_vendor;
	int  usb_product;
	int  usb_inep;
	int  usb_outep;
	int  usb_wrap;
} SierraCamera;

#define CHECK(result) {int res; res = result; if (res != GP_OK) return (res);}

