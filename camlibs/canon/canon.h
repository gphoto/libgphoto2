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

#define CHECK_PARAM_NULL(param) \
	if (param == NULL) { \
		gp_camera_set_error (camera, "NULL param \"%s\" in %s line %i", #param, __FILE__, __LINE__); \
		return GP_ERROR_BAD_PARAMETERS; \
	}

struct canonCamModelData
{
	char *id_str;
	canonCamModel model;
	unsigned short usb_vendor;
	unsigned short usb_product;
	char serial_support;
	unsigned int max_picture_size;
	unsigned int max_thumbnail_size;
};

extern const struct canonCamModelData models[];

struct _CameraPrivateLibrary
{
	struct canonCamModelData *md;
	int speed;        /* The speed we're using for this camera */
	char ident[32];   /* Model ID string given by the camera */
	char owner[32];   /* Owner name */
	char firmwrev[4]; /* Firmware revision */
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


#define DIR_CREATE 0
#define DIR_REMOVE 1

/* These contain the default label for all the 
 * switch (camera->port->type) statements
 */
#define GP_PORT_DEFAULT_RETURN_INTERNAL(return_statement) \
		default: \
			gp_camera_set_error (camera, "Don't know how to handle " \
					     "camera->port->type value %i aka 0x%x" \
					     "in %s line %i.", camera->port->type, \
					     camera->port->type, __FILE__, __LINE__); \
			return_statement; \
			break;

#define GP_PORT_DEFAULT_RETURN_EMPTY   GP_PORT_DEFAULT_RETURN_INTERNAL(return)
#define GP_PORT_DEFAULT_RETURN(RETVAL) GP_PORT_DEFAULT_RETURN_INTERNAL(return RETVAL)
#define GP_PORT_DEFAULT                GP_PORT_DEFAULT_RETURN(GP_ERROR_BAD_PARAMETERS)

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
int canon_int_set_time(Camera *camera, time_t date);
int canon_int_directory_operations(Camera *camera, const char *path, int action);
int canon_int_identify_camera(Camera *camera);
int canon_int_set_owner_name(Camera *camera, const char *name);

/* path conversion - needs drive letter, and can therefor not be moved to util.c */
const char *canon2gphotopath(Camera *camera, const char *path);
char *gphoto2canonpath(Camera *camera, char *path);

/* for the macros abbreviating gp_log* */
#define GP_MODULE "canon"

#endif /* _CANON_H */

/*
 * Local Variables:
 * c-file-style:"linux"
 * indent-tabs-mode:t
 * End:
 */
