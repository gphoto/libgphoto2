typedef enum {
	SIERRA_MODEL_DEFAULT,   
	SIERRA_MODEL_EPSON,
	SIERRA_MODEL_OLYMPUS,
	SIERRA_MODEL_CAM_DESC,
} SierraModel;

typedef enum {
	SIERRA_WRAP_USB	 = 1<<0,
	SIERRA_NO_51	 = 1<<1,
	SIERRA_LOW_SPEED = 1<<2,
} SierraFlags;

struct _CameraPrivateLibrary {
	SierraModel model;
	int folders;
	int speed;
	int first_packet;
	SierraFlags flags;
	int use_extended_protocol:1;
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
