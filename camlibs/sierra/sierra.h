
typedef enum {
	SIERRA_MODEL_DEFAULT,   
	SIERRA_MODEL_EPSON,
	SIERRA_MODEL_OLYMPUS
} SierraModel;

struct _CameraPrivateLibrary {
	SierraModel model;
	int folders;
	int speed;
	int first_packet;
	int usb_wrap;  /* 0: packets sent "raw", 1: see sierra-usbwrap.h */
	char folder[128];
};

typedef struct {
	char model[64];
	SierraModel sierra_model;
	int  usb_vendor;
	int  usb_product;
	int  usb_wrap;
} SierraCamera;

#define CHECK(result)					\
{							\
	int res = (result);				\
							\
	if (res < 0) {					\
		gp_log (GP_LOG_DEBUG, "sierra",		\
			"Operation failed (%i)!", res);	\
		return (res);				\
	}						\
}

