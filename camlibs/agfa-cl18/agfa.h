#ifndef agfa_H
#define agfa_H

#include <gphoto2.h>

#include <gphoto2-port.h>

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


/* agfa protocol primitives */
struct agfa_command {
	unsigned int length;
	unsigned int command;
	unsigned int argument;
};

struct agfa_file_command {
   unsigned int length;
   char filename[12];
};

struct agfa_device {
	gp_port *gpdev;

	int num_pictures;
	char *file_list;

	/* These parameters are only significant for serial support */
	int portspeed;

	int deviceframesize;
};

extern int (*agfa_send)(struct agfa_device *dev, void *buffer, int buflen);
extern int (*agfa_read)(struct agfa_device *dev, void *buffer, int buflen);

/* commands.c */
void agfa_reset(struct agfa_device *dev);

int agfa_get_status(struct agfa_device *dev, int *taken,
	int *available, int *rawcount);
int agfa_photos_taken(struct agfa_device *dev,int *taken);

int agfa_get_file_list(struct agfa_device *dev);

int agfa_delete_picture(struct agfa_device *dev, const char *filename);

int agfa_get_thumb_size(struct agfa_device *dev, const char *filename);
int agfa_get_thumb(struct agfa_device *dev, const char *filename, 
		   unsigned char *data,int size);
int agfa_get_pic_size(struct agfa_device *dev, const char *filename);
int agfa_get_pic(struct agfa_device *dev, const char *filename, 
		   unsigned char *data,int size);
int agfa_capture(struct agfa_device *dev, const char *path);


/* usb.c */
int agfa_usb_open(struct agfa_device *dev, Camera *camera);

struct model {
   char *name;
   unsigned short idVendor;
   unsigned short idProduct;
   char serial;
} models[];

#endif

