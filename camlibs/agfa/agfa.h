#ifndef agfa_H
#define agfa_H

#define AGFA_RESET          0x0001
#define AGFA_TAKEPIC1       0x0004
#define AGFA_TAKEPIC2       0x0094
#define AGFA_TAKEPIC3       0x0092
#define AGFA_DELETE         0x0100
#define AGFA_GET_PIC        0x0101
#define AGFA_GET_PIC_SIZE   0x0102
#define AGFA_GET_NUM_PICS   0x0103
#define AGFA_DELETE_ALL2    0x0105
#define AGFA_END_OF_THUMB   0x0106   /* delete all one? */
#define AGFA_GET_NAMES      0x0108
#define AGFA_GET_THUMB_SIZE 0x010A
#define AGFA_GET_THUMB      0x010B
#define AGFA_DONE_PIC       0x01FF
#define AGFA_STATUS	    0x0114

struct _CameraPrivateLibrary {
	GPPort *gpdev;

	int num_pictures;
	char *file_list;

	/* These parameters are only significant for serial support */
	int portspeed;

	int deviceframesize;
};

/* commands.c */
int agfa_reset(CameraPrivateLibrary *dev);

int agfa_get_status(CameraPrivateLibrary *dev, int *taken,
	int *available, int *rawcount);

int agfa_photos_taken(CameraPrivateLibrary *dev);

int agfa_get_file_list(CameraPrivateLibrary *dev);

int agfa_delete_picture(CameraPrivateLibrary *dev, const char *filename);

int agfa_get_thumb_size(CameraPrivateLibrary *dev, const char *filename);
int agfa_get_thumb(CameraPrivateLibrary *dev, const char *filename, 
		   unsigned char *data,int size);
int agfa_get_pic_size(CameraPrivateLibrary *dev, const char *filename);
int agfa_get_pic(CameraPrivateLibrary *dev, const char *filename, 
		   unsigned char *data,int size);
int agfa_capture(CameraPrivateLibrary *dev, CameraFilePath *path);


/* usb.c */
int agfa_usb_open(CameraPrivateLibrary *dev, Camera *camera);


#endif

