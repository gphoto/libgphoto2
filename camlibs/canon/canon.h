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

#ifdef CANON_EXPERIMENTAL_UPLOAD
# ifdef __GCC__
#  warning COMPILING WITH EXPERIMENTAL UPLOAD FEATURE
# endif
#endif

/**
 * canonPowerStatus:
 * @CAMERA_POWER_BAD: Value returned if power source is bad
 *   (i.e. battery is low).
 * @CAMERA_POWER_OK: Value returned if power source is OK.
 *
 * Battery status values
 *
 */
typedef enum {
	CAMERA_POWER_BAD = 4,
	CAMERA_POWER_OK  = 6
} canonPowerStatus;
/* #define CAMERA_POWER_OK     6 */
/* #define CAMERA_POWER_BAD    4 */

/* #define CAMERA_ON_AC       16 obsolete; we now just use*/
/* #define CAMERA_ON_BATTERY  48 the bit that makes the difference */

#define CAMERA_MASK_BATTERY  32

/**
 * canonJpegMarkerCode:
 * @JPEG_ESC: Byte value to flag possible JPEG code.
 * @JPEG_BEG: Byte value which, immediately after %JPEG_ESC, marks the start
 *  of JPEG image data in a JFIF file.
 * @JPEG_SOS: Byte value to flag a JPEG SOS marker.
 * @JPEG_A50_SOS: Byte value to flag a JPEG SOS marker in a file from
 *   a PowerShot A50 camera.
 * @JPEG_END: Byte code to mark the end of a JPEG image?
 *
 * Flags to find important points in JFIF or EXIF files
 *
 */
typedef enum {
	JPEG_ESC     = 0xFF,
	JPEG_BEG     = 0xD8,
	JPEG_SOS     = 0xDB,
	JPEG_A50_SOS = 0xC4,
	JPEG_END     = 0xD9
} canonJpegMarkerCode;

/* #define JPEG_ESC        0xFF */
/* #define JPEG_BEG        0xD8 */
/* #define JPEG_SOS        0xDB */
/* #define JPEG_A50_SOS    0xC4 */
/* #define JPEG_END        0xD9 */

/**
 * canonCamModel
 * @CANON_PS_A5: PowerShot A5
 * @CANON_PS_A5_ZOOM: PowerShot A5 Zoom
 * @CANON_PS_A50: PowerShot A50
 * @CANON_PS_A70: PowerShot Pro70
 * @CANON_PS_S10: PowerShot S10
 * @CANON_PS_S20: PowerShot S20
 * @CANON_EOS_D30: EOS D30
 * @CANON_PS_S100: PowerShot S100, PowerShot S110, IXY DIGITAL,
 *     Digital IXUS, DIGITAL IXUS v
 * @CANON_PS_G1: PowerShot G1
 * @CANON_PS_PRO90_IS: PowerShot Pro90 IS
 * @CANON_PS_S300: PowerShot S300, Digital IXUS 300
 * @CANON_PS_A20: PowerShot A20
 * @CANON_PS_A10: PowerShot A10
 * @CANON_PS_G2: PowerShot G2
 * @CANON_PS_S40: PowerShot S40
 * @CANON_PS_S30: PowerShot S30
 * @CANON_PS_A40: PowerShot A40
 * @CANON_PS_A30: PowerShot A30
 * @CANON_EOS_D60: EOS D60
 * @CANON_PS_A100: PowerShot A100
 * @CANON_PS_A200: PowerShot A200
 * @CANON_PS_S200: PowerShot S200, Digital IXUS v2
 * @CANON_PS_S330: Digital IXUS 330
 * @CANON_PS_S45: PowerShot S45
 * @CANON_PS_G3: PowerShot G3
 * @CANON_PS_S230: PowerShot S230, Digital IXUS v3
 *
 * Enumeration of all camera types currently supported.
 *
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
	CANON_PS_A200,
	CANON_PS_S45,
	CANON_PS_S230,
	CANON_PS_G3
} canonCamModel;

/**
 * CON_CHECK_PARAM_NULL
 * @param: value to check for NULL
 *
 * Checks if the given parameter is NULL. If so, reports through
 *  gp_context_error() (assuming that "context" is defined) and returns
 *  %GP_ERROR_BAD_PARAMETERS from the enclosing function.
 *
 */
#define CON_CHECK_PARAM_NULL(param) \
	if (param == NULL) { \
		gp_context_error (context, "NULL param \"%s\" in %s line %i", #param, __FILE__, __LINE__); \
		return GP_ERROR_BAD_PARAMETERS; \
	}

/**
 * CHECK_PARAM_NULL
 * @param: value to check for NULL
 *
 * Checks if the given parameter is NULL. If so, returns
 *  %GP_ERROR_BAD_PARAMETERS from the enclosing function.
 *
 */
#define CHECK_PARAM_NULL(param) \
	if (param == NULL) { \
		GP_LOG (GP_LOG_ERROR, "NULL param \"%s\" in %s line %i", #param, __FILE__, __LINE__); \
		return GP_ERROR_BAD_PARAMETERS; \
	}


/**
 * canonCaptureSupport:
 * @CAP_NON: No support for capture with this camera
 * @CAP_SUP: Capture is fully supported for this camera
 * @CAP_EXP: Capture support for this camera is experimental, i.e. it
 *    has known problems
 *
 * State of capture support
 *  Non-zero if any support exists, but lets caller know
 *  if support is to be trusted.
 *
 */
typedef enum {
	CAP_NON = 0, /* no support */
	CAP_SUP,     /* supported */
	CAP_EXP      /* experimental support */
} canonCaptureSupport;


struct canonCamModelData
{
	char *id_str;
	canonCamModel model;
	unsigned short usb_vendor;
	unsigned short usb_product;
	canonCaptureSupport usb_capture_support;
	unsigned int max_picture_size;
	unsigned int max_thumbnail_size;
	char *serial_id_string; /* set to NULL if camera doesn't support serial connections */
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

	int capturing; /* whether we are capturing or not
			  hack to speed up usb_dialogue 
			  when not capturing [no sleep(2)] */

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

/*#define CANON_MINIMUM_DIRENT_SIZE	11*/

/**
 * canonDirentOffset:
 * @CANON_DIRENT_ATTRS: Attribute byte
 * @CANON_DIRENT_SIZE: 4 byte file size
 * @CANON_DIRENT_TIME: 4 byte Unix time
 * @CANON_DIRENT_NAME: Variable length ASCII path name
 * @CANON_MINIMUM_DIRENT_SIZE: Minimum size of a directory entry,
 *      including a null byte for an empty path name
 *
 * Offsets of fields of direntry in bytes.
 *  A minimum directory entry is:
 *  2 bytes attributes,
 *  4 bytes file date (UNIX localtime),
 *  4 bytes file size,
 *  1 byte empty path '' plus NULL byte.
 *
 * Wouldn't this be better as a struct?
 *
 */
typedef enum {
	CANON_DIRENT_ATTRS = 0,
	CANON_DIRENT_SIZE  = 2,
	CANON_DIRENT_TIME  = 6,
	CANON_DIRENT_NAME = 10,
	CANON_MINIMUM_DIRENT_SIZE
} canonDirentOffset;

/* #define CANON_DIRENT_ATTRS 0 */
/* #define CANON_DIRENT_SIZE  2 */
/* #define CANON_DIRENT_TIME  6 */
/* #define CANON_DIRENT_NAME 10 */

/**
 * canonDirentAttributeBits:
 * @CANON_ATTR_WRITE_PROTECTED: File is write-protected
 * @CANON_ATTR_UNKNOWN_2: 
 * @CANON_ATTR_UNKNOWN_4: 
 * @CANON_ATTR_UNKNOWN_8: 
 * @CANON_ATTR_NON_RECURS_ENT_DIR: This entry represents a directory
 *   that was not entered in this listing.
 * @CANON_ATTR_DOWNLOADED: This file has not yet been downloaded
 *   (the bit is cleared by the host software).
 * @CANON_ATTR_UNKNOWN_40: 
 * @CANON_ATTR_RECURS_ENT_DIR: This entry represents a directory
 *   that was entered in this listing; look for its contents
 *   later in the listing.
 *
 * Attribute bits in the %CANON_DIRENT_ATTRS byte in each directory
 *   entry.
 *
 */

typedef enum {
	CANON_ATTR_WRITE_PROTECTED    = 0x01,
	CANON_ATTR_UNKNOWN_2	      = 0x02,
	CANON_ATTR_UNKNOWN_4	      = 0x04,
	CANON_ATTR_UNKNOWN_8	      = 0x08,
	CANON_ATTR_NON_RECURS_ENT_DIR = 0x10,
	CANON_ATTR_DOWNLOADED	      = 0x20,
	CANON_ATTR_UNKNOWN_40	      = 0x40,
	CANON_ATTR_RECURS_ENT_DIR     = 0x80
} canonDirentAttributeBits;

/* #define CANON_ATTR_WRITE_PROTECTED		0x01 */
/* #define CANON_ATTR_UNKNOWN_2			0x02 */
/* #define CANON_ATTR_UNKNOWN_4			0x04 */
/* #define CANON_ATTR_UNKNOWN_8			0x08 */
/* #define CANON_ATTR_NON_RECURS_ENT_DIR		0x10 */
/* #define CANON_ATTR_DOWNLOADED			0x20 */
/* #define CANON_ATTR_UNKNOWN_40			0x40 */
/* #define CANON_ATTR_RECURS_ENT_DIR		0x80 */

/**
 * canonDirlistFunctionBits:
 * @CANON_LIST_FILES: List files
 * @CANON_LIST_FOLDERS: List folders
 *
 * Software bits to pass in "flags" argument to
 * canon_int_list_directory(), telling what to list. Bits may be ORed
 * together to list both files and folders.
 *
 */
typedef enum {
	CANON_LIST_FILES   = 2,
	CANON_LIST_FOLDERS = 4
} canonDirlistFunctionBits;

/* #define CANON_LIST_FILES (1 << 1) */
/* #define CANON_LIST_FOLDERS (1 << 2) */

/**
 * canonDirFunctionCode:
 * @DIR_CREATE: Create the specified directory
 * @DIR_REMOVE: Remove the specified directory
 *
 * Software code to pass to canon_int_directory_operations().
 *
 */
typedef enum {
	DIR_CREATE = 0,
	DIR_REMOVE = 1
} canonDirFunctionCode;

/* #define DIR_CREATE 0 */
/* #define DIR_REMOVE 1 */

/* These contain the default label for all the 
 * switch (camera->port->type) statements
 */

/**
 * GP_PORT_DEFAULT_RETURN_INTERNAL:
 * @return_statement: Statement to use for return
 *
 * Used only by GP_PORT_DEFAULT_RETURN_EMPTY(),
 *  GP_PORT_DEFAULT_RETURN(), and GP_PORT_DEFAULT()
 *
 */
#define GP_PORT_DEFAULT_RETURN_INTERNAL(return_statement) \
		default: \
			gp_context_error (context, "Don't know how to handle " \
					     "camera->port->type value %i aka 0x%x" \
					     "in %s line %i.", camera->port->type, \
					     camera->port->type, __FILE__, __LINE__); \
			return_statement; \
			break;

/**
 * GP_PORT_DEFAULT_RETURN_EMPTY:
 *
 * Return as a default case in switch (camera->port->type)
 * statements in functions returning void.
 *
 */
#define GP_PORT_DEFAULT_RETURN_EMPTY   GP_PORT_DEFAULT_RETURN_INTERNAL(return)
/**
 * GP_PORT_DEFAULT_RETURN
 * @RETVAL: Value to return from this function
 *
 * Return as a default case in switch (camera->port->type)
 * statements in functions returning a value.
 *
 */
#define GP_PORT_DEFAULT_RETURN(RETVAL) GP_PORT_DEFAULT_RETURN_INTERNAL(return RETVAL)

/**
 * GP_PORT_DEFAULT
 *
 * Return as a default case in switch (camera->port->type) statements
 * in functions returning a gphoto2 error code where this value of
 * camera->port->type is unexpected.
 *
 */
#define GP_PORT_DEFAULT                GP_PORT_DEFAULT_RETURN(GP_ERROR_BAD_PARAMETERS)

/**
 * IS_EOS
 * @cam: camera type from camera->pl->md->model
 *
 * Checks whether to treat camera as an EOS model; differences in key
 *  lock/unlock, no "get picture abilities", different interrupt
 *  sequence on remote capture.
 * @Returns: 1 if camera is treated as EOS, 0 if otherwise.
 *
 * Treat PS 230 as EOS just as a guess; I really don't know.
 *
 */
#define IS_EOS(cam) ( ((cam)==CANON_EOS_D30) \
                      || ((cam)==CANON_EOS_D60) \
                      || ((cam)==CANON_PS_S230 ) )

/*
 * All functions returning a pointer have malloc'ed the data. The caller must
 * free() it when done.
 */

int canon_int_ready(Camera *camera, GPContext *context);

char *canon_int_get_disk_name(Camera *camera, GPContext *context);

int canon_int_get_battery(Camera *camera, int *pwr_status, int *pwr_source, GPContext *context);


int canon_int_capture_image (Camera *camera, CameraFilePath *path, GPContext *context);

int canon_int_get_disk_name_info(Camera *camera, const char *name,int *capacity,int *available, GPContext *context);

int canon_int_list_directory (Camera *camera, const char *folder, CameraList *list, const canonDirlistFunctionBits flags, GPContext *context);

int canon_int_get_file(Camera *camera, const char *name, unsigned char **data, int *length, GPContext *context);
int canon_int_get_thumbnail(Camera *camera, const char *name, unsigned char **retdata, int *length, GPContext *context);
int canon_int_put_file(Camera *camera, CameraFile *file, char *destname, char *destpath, GPContext *context);
int canon_int_set_file_attributes(Camera *camera, const char *file, const char *dir, canonDirentAttributeBits attrs, GPContext *context);
int canon_int_delete_file(Camera *camera, const char *name, const char *dir, GPContext *context);
int canon_serial_end(Camera *camera);
int canon_serial_off(Camera *camera);
int canon_int_get_time(Camera *camera, time_t *camera_time, GPContext *context);
int canon_int_set_time(Camera *camera, time_t date, GPContext *context);
int canon_int_directory_operations(Camera *camera, const char *path, canonDirFunctionCode action, GPContext *context);
int canon_int_identify_camera(Camera *camera, GPContext *context);
int canon_int_set_owner_name(Camera *camera, const char *name, GPContext *context);

/*
 * introduced for capturing
 */
int
canon_int_get_picture_abilities (Camera *camera, GPContext *context);
int
canon_int_pack_control_subcmd (unsigned char *payload, int subcmd,
			       int word0, int word1,
			       char *desc);
int
canon_int_do_control_command (Camera *camera, int subcmd, int a, int b);



/* path conversion - needs drive letter, and therefore cannot be moved
 * to util.c */
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
