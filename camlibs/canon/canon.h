/*
 * canon.h - Canon "native" operations.
 *
 * Written 1999 by Wolfgang G. Reissnegger and Werner Almesberger
 *
 * $Id$
 *
 */

#ifndef _CANON_H
#define _CANON_H

/* Defines for error handling */
#define NOERROR		0
#define ERROR_RECEIVED	1
#define ERROR_ADDRESSED	2
#define FATAL_ERROR	3
#define ERROR_LOWBATT	4

/* Battery status values:

 * hopefully obsolete values first - we now just use the bit 
 * that makes the difference

  obsolete #define CAMERA_ON_AC       16
  obsolete #define CAMERA_ON_BATTERY  48
*/

#define CAMERA_POWER_OK     6
#define CAMERA_POWER_BAD    4

#define CAMERA_MASK_BATTERY  32


struct canon_dir {
  const char *name; /* NULL if at end */
  unsigned int size;
  time_t date;
  unsigned char attrs; /* File attributes, see "Protocols" for details */
  int is_file;
  void *user;	/* user-specific data */
};


/**
 * Various camera types
 */
typedef enum {
	CANON_PS_A5,
	CANON_PS_A5_ZOOM,
	CANON_PS_A50,
	CANON_PS_S10,
	CANON_PS_S20,
	CANON_PS_S30,
	CANON_PS_S40,
	CANON_PS_A70,
	CANON_PS_S100,
	CANON_PS_S300,
	CANON_PS_G1,
	CANON_PS_G2,
	CANON_PS_A10,
	CANON_PS_A20,
	CANON_EOS_D30,
	CANON_PS_PRO90_IS
} canonCamModel;

/**
 * models:
 *
 * Contains list of all camera models currently supported with their
 * respective USD IDs and a flag denoting RS232 serial support.
 **/
static const struct
{
	char *name;
	canonCamModel model;
	unsigned short idVendor;
	unsigned short idProduct;
	char serial;
}
models[] =
{
	/* *INDENT-OFF* */
	{"DE300 Canon Inc.",		CANON_PS_A5,		0, 0, 1},
	{"Canon PowerShot A5 Zoom",	CANON_PS_A5_ZOOM,	0, 0, 1},
	{"Canon PowerShot A50",		CANON_PS_A50,		0, 0, 1},
	{"Canon PowerShot Pro70",	CANON_PS_A70,		0, 0, 1},
	{"Canon PowerShot S10",		CANON_PS_S10,		0x04A9, 0x3041, 1},
	{"Canon PowerShot S20",		CANON_PS_S20,		0x04A9, 0x3043, 1},
	{"Canon EOS D30",		CANON_EOS_D30,		0x04A9, 0x3044, 0},
	{"Canon PowerShot S100",	CANON_PS_S100,		0x04A9, 0x3045, 0},
	{"Canon IXY DIGITAL",		CANON_PS_S100,		0x04A9, 0x3046, 0},
	{"Canon Digital IXUS",		CANON_PS_S100,		0x04A9, 0x3047, 0},
	{"Canon PowerShot S110",	CANON_PS_S100,		0x04A9, 0x3051, 0},
	{"Canon DIGITAL IXUS v",	CANON_PS_S100,		0x04A9, 0x3052, 0},
	{"Canon PowerShot G1",		CANON_PS_G1,		0x04A9, 0x3048, 1},
	{"Canon PowerShot Pro90 IS",	CANON_PS_PRO90_IS,	0x04A9, 0x3049, 1},
	{"Canon IXY DIGITAL 300",	CANON_PS_S300,		0x04A9, 0x304B, 0},
	{"Canon PowerShot S300",	CANON_PS_S300,		0x04A9, 0x304C, 0},
	{"Canon Digital IXUS 300",	CANON_PS_S300,		0x04A9, 0x304D, 0},
	{"Canon PowerShot A20",		CANON_PS_A20,		0x04A9, 0x304E, 0},
	{"Canon PowerShot A10",		CANON_PS_A10,		0x04A9, 0x304F, 0},
	{"Canon PowerShot G2",		CANON_PS_G2,		0x04A9, 0x3055, 0},
	{"Canon PowerShot S40",		CANON_PS_S40,		0x4A9, 0x3056, 0},
	{"Canon PowerShot S30",		CANON_PS_S40,		0x4A9, 0x3057, 0},
	{NULL}
	/* *INDENT-ON* */
};

struct _CameraPrivateLibrary
{
	canonCamModel model;
	int speed;        /* The speed we're using for this camera */
	char ident[32];   /* Model ID string given by the camera */
	char owner[32];   /* Owner name */
	char firmwrev[4]; /* Firmware revision */
	char psa50_id[200];	/* some models may have a lot to report */
	int canon_comm_method;
	unsigned char psa50_eot[8];

	int receive_error;
	int first_init;  /* first use of camera   1 = yes 0 = no */
	int uploading;   /* 1 = yes ; 0 = no */
	int slow_send;   /* to send data via serial with a usleep(1) 
					  * between each byte 1 = yes ; 0 = no */ 

	unsigned char seq_tx;
	unsigned char seq_rx;

/*
 * Directory access may be rather expensive, so we cache some information.
 * The first variable in each block indicates whether the block is valid.
 */

	int cached_ready;
	int cached_disk;
	char *cached_drive; /* usually something like C: */
	int cached_capacity;
	int cached_available;
	int cached_dir;
	struct canon_dir *cached_tree;
	int cached_images;
	char **cached_paths; /* only used by A5 */
};

/* A minimum dirent is :
 * 2    attributes + 0x00
 * 4    file date (UNIX localtime)
 * 4    file size
 * 1    empty path '' plus NULL byte
 */
#define CANON_MINIMUM_DIRENT_SIZE	11



/*
 * Our driver now supports both USB and serial communications
 */
#define CANON_SERIAL_RS232 0
#define CANON_USB 1

#define DIR_CREATE 0
#define DIR_REMOVE 1


typedef unsigned long u32;
#ifndef byteswap32
#ifdef __sparc
#define byteswap32(val) ({ u32 x = val; x = (x << 24) | ((x << 8) & 0xff0000) | ((x >> 8) & 0xff00) | (x >> 24); x; })
#else
#define byteswap32(val) val
#endif
#endif


/*
 * All functions returning a pointer have malloc'ed the data. The caller must
 * free() it when done.
 */

/**
 * Switches the camera on, detects the model and sets its speed
 */
int canon_int_ready(Camera *camera);

/*
 * 
 */
char *canon_int_get_disk_name(Camera *camera);

/*
 *
 */
int canon_int_get_battery(Camera *camera, int *pwr_status, int *pwr_source);

/*
 *
 */
int canon_int_get_disk_name_info(Camera *camera, const char *name,int *capacity,int *available);

/*
 *
 */
int canon_int_list_directory (Camera *camera, struct canon_dir **result_dir, const char *path);
void canon_int_free_dir(Camera *camera, struct canon_dir *list);
int canon_int_get_file(Camera *camera, const char *name, unsigned char **data, int *length);
unsigned char *canon_int_get_thumbnail(Camera *camera, const char *name,int *length);
int canon_int_put_file(Camera *camera, CameraFile *file, char *destname, char *destpath);
int canon_int_set_file_attributes(Camera *camera, const char *file, const char *dir, unsigned char attrs);
int canon_int_delete_file(Camera *camera, const char *name, const char *dir);
int canon_serial_end(Camera *camera);
int canon_serial_off(Camera *camera);
time_t canon_int_get_time(Camera *camera);
int canon_int_set_time(Camera *camera);
int canon_int_directory_operations(Camera *camera, const char *path, int action);
int canon_int_identify_camera(Camera *camera);
int canon_int_set_owner_name(Camera *camera, const char *name);

/* path conversion - needs drive letter, and can therefor not be moved to util.c */
char *canon2gphotopath(char *path);
char *gphoto2canonpath(char *path);

/* for the macros abbreviating gp_log* */
#define GP_MODULE "canon"

#endif /* _CANON_H */
