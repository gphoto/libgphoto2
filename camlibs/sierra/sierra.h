
typedef enum {
	SIERRA_MODEL_DEFAULT,   
	SIERRA_MODEL_EPSON,
	SIERRA_MODEL_OLYMPUS,
	SIERRA_MODEL_CAM_DESC,
} SierraModel;

struct _CameraPrivateLibrary {
	SierraModel model;
	int folders;
	int speed;
	int first_packet;
	int usb_wrap:1;  /* 0: packets sent "raw", 1: see sierra-usbwrap.h */
	int use_extended_protocol:1;
	struct CameraDesc const *cam_desc;
	char folder[128];
};

struct CameraDescriptor;

typedef struct {
	char model[64];
	SierraModel sierra_model;
	int  usb_vendor;
	int  usb_product;
	int  usb_wrap;
	struct CameraDesc const *cam_desc;
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
