typedef enum {
	SIERRA_MODEL_DEFAULT,
	SIERRA_MODEL_EPSON,
	SIERRA_MODEL_OLYMPUS,
	SIERRA_MODEL_CAM_DESC,
} SierraModel;

typedef enum {
	/* 2 bits USB wrap type ... none = 0 */
	SIERRA_WRAP_USB_NONE	= 0<<0,
	SIERRA_WRAP_USB_OLYMPUS	= 1<<0,
	SIERRA_WRAP_USB_NIKON	= 2<<0,
	SIERRA_WRAP_USB_PENTAX	= 3<<0,
	SIERRA_WRAP_USB_MASK	= 3<<0,

	SIERRA_NO_51	 	= 1<<2,
	SIERRA_LOW_SPEED	= 1<<3,	/* serial line 9600 -> 38400 */
	SIERRA_EXT_PROTO	= 1<<4,
	SIERRA_SKIP_INIT	= 1<<5,
	SIERRA_NO_USB_CLEAR	= 1<<6,
	SIERRA_NO_REGISTER_40	= 1<<7,
	SIERRA_MID_SPEED	= 1<<8,	/* serial line 9600 -> 57600 */
} SierraFlags;

struct _CameraPrivateLibrary {
	SierraModel model;
	int folders;
	int speed;
	int first_packet;
	SierraFlags flags;
	struct CameraDesc const *cam_desc;
	char folder[128];
};

struct CameraDescriptor;

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
