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

#ifdef CANON_EXPERIMENTAL_CAPTURE
#warning COMPILING WITH EXPERIMENTAL CAPTURE FEATURE
#endif

#ifdef CANON_EXPERIMENTAL_UPLOAD
#warning COMPILING WITH EXPERIMENTAL UPLOAD FEATURE
#endif

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
	CANON_PS_A30,
	CANON_PS_A40,
	CANON_EOS_D30,
	CANON_PS_PRO90_IS,
	CANON_PS_S330,
	CANON_PS_S200,
	CANON_EOS_D60,
	CANON_PS_A100,
	CANON_PS_A200
} canonCamModel;

#define CON_CHECK_PARAM_NULL(param) \
	if (param == NULL) { \
		gp_context_error (context, "NULL param \"%s\" in %s line %i", #param, __FILE__, __LINE__); \
		return GP_ERROR_BAD_PARAMETERS; \
	}

#define CHECK_PARAM_NULL(param) \
	if (param == NULL) { \
		GP_LOG (GP_LOG_ERROR, "NULL param \"%s\" in %s line %i", #param, __FILE__, __LINE__); \
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

#ifdef CANON_EXPERIMENTAL_CAPTURE
	int capturing; /* whether we are capturing or not
			  hack to speed up usb_dialogue 
			  when not capturing [no sleep(2)] */
#endif /* CANON_EXPERIMENTAL_CAPTURE */

	/* driver settings
	 * leave these as int, as gp_widget_get_value sets them as int!
	 */
	int list_all_files; /* whether to list all files, not just know types */

#ifdef CANON_EXPERIMENTAL_UPLOAD
	int upload_keep_filename; /* 0=DCIF compatible filenames (AUT_*), 
				     1=keep original filename */
#endif /* CANON_EXPERIMENTAL_UPLOAD */

	char *cached_drive;	/* usually something like C: */
	int cached_ready;       /* whether the camera is ready to rock */

/*
 * Directory access may be rather expensive, so we cached some information.
 * This is now done by libgphoto2, so we are continuously removing this stuff.
 * So the following variables are OBSOLETE.
 */

	int cached_disk;
	int cached_capacity;
	int cached_available;
};

/* A minimum dirent is :
 * 2    attributes + 0x00
 * 4    file date (UNIX localtime)
 * 4    file size
 * 1    empty path '' plus NULL byte
 */
#define CANON_MINIMUM_DIRENT_SIZE	11

/* offsets of fields of direntry in bytes */
#define CANON_DIRENT_ATTRS 0
#define CANON_DIRENT_SIZE  2
#define CANON_DIRENT_TIME  6
#define CANON_DIRENT_NAME 10

/* what to list in canon_int_list_directory */
#define CANON_LIST_FILES (1 << 1)
#define CANON_LIST_FOLDERS (1 << 2)

#define DIR_CREATE 0
#define DIR_REMOVE 1

/* These contain the default label for all the 
 * switch (camera->port->type) statements
 */
#define GP_PORT_DEFAULT_RETURN_INTERNAL(return_statement) \
		default: \
			gp_context_error (context, "Don't know how to handle " \
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
int canon_int_ready(Camera *camera, GPContext *context);

/*
 * 
 */
char *canon_int_get_disk_name(Camera *camera, GPContext *context);

/*
 *
 */
int canon_int_get_battery(Camera *camera, int *pwr_status, int *pwr_source, GPContext *context);


#ifdef CANON_EXPERIMENTAL_CAPTURE
/*
 *
 */
int canon_int_capture_image (Camera *camera, CameraFilePath *path, GPContext *context);
#endif

/*
 *
 */
int canon_int_get_disk_name_info(Camera *camera, const char *name,int *capacity,int *available, GPContext *context);

/*
 *
 */
int canon_int_list_directory (Camera *camera, const char *folder, CameraList *list, const int flags, GPContext *context);

int canon_int_get_file(Camera *camera, const char *name, unsigned char **data, int *length, GPContext *context);
int canon_int_get_thumbnail(Camera *camera, const char *name, unsigned char **retdata, int *length, GPContext *context);
int canon_int_put_file(Camera *camera, CameraFile *file, char *destname, char *destpath, GPContext *context);
int canon_int_set_file_attributes(Camera *camera, const char *file, const char *dir, unsigned char attrs, GPContext *context);
int canon_int_delete_file(Camera *camera, const char *name, const char *dir, GPContext *context);
int canon_serial_end(Camera *camera);
int canon_serial_off(Camera *camera);
int canon_int_get_time(Camera *camera, time_t *camera_time, GPContext *context);
int canon_int_set_time(Camera *camera, time_t date, GPContext *context);
int canon_int_directory_operations(Camera *camera, const char *path, int action, GPContext *context);
int canon_int_identify_camera(Camera *camera, GPContext *context);
int canon_int_set_owner_name(Camera *camera, const char *name, GPContext *context);

/* path conversion - needs drive letter, and can therefor not be moved to util.c */
const char *canon2gphotopath(Camera *camera, const char *path);
const char *gphoto2canonpath(Camera *camera, const char *path, GPContext *context);

const char *canon_int_filename2thumbname (Camera *camera, const char *filename);

int canon_int_extract_jpeg_thumb (unsigned char *data, const unsigned int datalen, unsigned char **retdata, unsigned int *retdatalen, GPContext *context);

/* for the macros abbreviating gp_log* */
#define GP_MODULE "canon"

#endif /* _CANON_H */

/*
 * Local Variables:
 * c-file-style:"linux"
 * indent-tabs-mode:t
 * End:
 */
