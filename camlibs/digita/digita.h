#ifndef DIGITA_H
#define DIGITA_H

#include <gphoto2.h>

#include <gpio.h>

#define DIGITA_GET_PRODUCT_INFO         0x01
#define DIGITA_GET_IMAGE_SPECS          0x02
#define DIGITA_GET_CAMERA_STATUS        0x03
#define DIGITA_SET_PRODUCT_INFO         0x05
#define DIGITA_GET_CAMERA_CAPABILITIES  0x10
#define DIGITA_GET_CAMERA_STATE         0x11
#define DIGITA_SET_CAMERA_STATE         0x12
#define DIGITA_GET_CAMERA_DEFAULTS      0x13
#define DIGITA_SET_CAMERA_DEFAULTS      0x14
#define DIGITA_RESTORE_CAMERA_STATES    0x15
#define DIGITA_GET_SCENE_ANALYSIS       0x18
#define DIGITA_GET_POWER_MODE           0x19
#define DIGITA_SET_POWER_MODE           0x1A
#define DIGITA_GET_S1_MODE              0x1D
#define DIGITA_SET_S1_MODE              0x1E
#define DIGITA_START_CAPTURE            0x1F
#define DIGITA_GET_FILE_LIST            0x40
#define DIGITA_GET_NEW_FILE_LIST        0x41
#define DIGITA_GET_FILE_DATA            0x42
#define DIGITA_ERASE_FILE               0x43
#define DIGITA_GET_STORAGE_STATUS       0x44
#define DIGITA_SET_FILE_DATA            0x47
#define DIGITA_GET_FILE_TAG             0x48
#define DIGITA_SET_USER_FILE_TAG        0x49
#define DIGITA_GET_CLOCK                0x70
#define DIGITA_SET_CLOCK                0x71
#define DIGITA_GET_ERROR                0x78
#define DIGITA_GET_INTERFACE_TIMEOUT    0x90
#define DIGITA_SET_INTERFACE_TIMEOUT    0x91

/* Digita protocol primitives */
struct digita_command {
	unsigned int length;
	unsigned int unknown;
	unsigned short command;
	unsigned short status;
};

struct partial_tag {
	unsigned int offset;
	unsigned int length;
	unsigned int filesize;
};

struct filename {
	unsigned int driveno;
	unsigned char path[32];
	unsigned char dosname[16];
};

struct file_item {
	struct filename fn;

	int length;
	unsigned int filestatus;
};

/* Digita protocol commands */

/* DIGITA_GET_FILE_DATA */
/*
 *  sent - digita_command
 *         get_file_data_send
 */
/*
 *  received - get_file_data_receive
 *             data
 */
struct get_file_data_send {
	struct digita_command cmd;

	struct filename fn;

	unsigned int dataselector;

	struct partial_tag tag;
};

struct get_file_data_receive {
	struct digita_command cmd;

	struct partial_tag tag;
};

/* DIGITA_GET_FILE_LIST */
/*  sent - get_file_list */
/*
 *  received - digita_command
 *             data
 */
struct get_file_list {
	struct digita_command cmd;

	unsigned int listorder;
};

/* DIGITA_ERASE_FILE */
/*  sent - erase_file */
/*  received - nothing */
struct erase_file {
	struct digita_command cmd;

	struct filename fn;
};

struct digita_device {
	gpio_device *gpdev;

	int debug;

	int num_pictures;
	struct file_item *file_list;

	/* These parameters are only significant for serial support */
	int portspeed;

	int deviceframesize;
};

extern int (*digita_send)(struct digita_device *dev, void *buffer, int buflen);
extern int (*digita_read)(struct digita_device *dev, void *buffer, int buflen);

/* commands.c */
int digita_get_storage_status(struct digita_device *dev, int *taken,
	int *available, int *rawcount);
int digita_get_file_list(struct digita_device *dev);
int digita_get_file_data(struct digita_device *dev, int thumbnail,
	struct filename *filename, struct partial_tag *tag, void *buffer);
int digita_delete_picture(struct digita_device *dev, struct filename *filename);

/* serial.c */
int digita_serial_open(struct digita_device *dev, Camera *camera,
		CameraInit *init);

/* usb.c */
int digita_usb_open(struct digita_device *dev, Camera *camera,
		CameraInit *init);

#endif

