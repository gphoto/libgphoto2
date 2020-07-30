/*
 * canon.h - Canon "native" operations.
 *
 * Written 1999 by Wolfgang G. Reissnegger and Werner Almesberger
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
/* #define CAMERA_ON_AC       16 obsolete; we now just use*/
/* #define CAMERA_ON_BATTERY  48 the bit that makes the difference */

/**
 * CAMERA_MASK_BATTERY
 *
 * Bit mask to use to find battery/AC flag
 *
 */
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
	JPEG_A50_SOS = 0xC4,
	JPEG_BEG     = 0xD8,
	JPEG_SOI     = 0xD8,
	JPEG_END     = 0xD9,
	JPEG_SOS     = 0xDB,
	JPEG_APP1    = 0xE1,
} canonJpegMarkerCode;

/**
 * canonCamClass:
 * @CANON_CLASS_0: does not support key lock at all.
 *                 Only known models: G1, Pro 90is, S100, S110, IXUS
 *                 v, IXY DIGITAL, Digital IXUS, S10, S20
 *
 * @CANON_CLASS_1: supports lock, but not unlock. Supports (and
 *                 requires) "get picture abilities" before capture.
 *                 Examples: A5, A5 Zoom, A50, S30, S40, S200, S300,
 *                 S330, G2, A10, A20, A30, A40, A100, A200,
 *                 Optura200/MVX2i.
 *
 * @CANON_CLASS_2: like class 1, but doesn't support EXIF. Pro 70
 *		   is only known model.
 *
 * @CANON_CLASS_3: like class 1, but can't delete image. Only known models:
 *		   A5, A5 Zoom.
 *
 * @CANON_CLASS_4: supports lock/unlock. EOS D30 was first example; others
 *                 include D60, 10D, 300D, S230, S400. Doesn't support
 *		   "get picture abilities".
 *
 * @CANON_CLASS_5: supports lock, no unlock, but not "get picture abilities".
 *                 Examples: S45, G3.
 *
 * @CANON_CLASS_6: major protocol revision. Examples: EOS 20D and 350D.
 *
 * Enumeration of all camera types currently supported. Simplified so
 * that all cameras with similar behavior have the same code.
 *
 */
typedef enum {
	CANON_CLASS_NONE,
	CANON_CLASS_0,
	CANON_CLASS_1,
	CANON_CLASS_2,
	CANON_CLASS_3,
	CANON_CLASS_4,
	CANON_CLASS_5,
	CANON_CLASS_6
} canonCamClass;

/**
 * canonTransferMode:
 * @REMOTE_CAPTURE_THUMB_TO_PC: Transfer the thumbnail to the host
 * @REMOTE_CAPTURE_FULL_TO_PC: Transfer the full-size image directly to the host
 * @REMOTE_CAPTURE_THUMB_TO_DRIVE: Store the thumbnail on the camera storage
 * @REMOTE_CAPTURE_FULL_TO_DRIVE: Store the full-size image on the camera storage
 *
 * Hardware codes to control image transfer in a remote capture
 * operation. These are bits that may be OR'ed together to get
 * multiple things to happen at once. For example,
 * @REMOTE_CAPTURE_THUMB_TO_PC|@REMOTE_CAPTURE_FULL_TO_DRIVE is what
 * D30Capture uses to store the full image on the camera, but provide
 * an on-screen thumbnail.
 *
 */
typedef enum {
	REMOTE_CAPTURE_THUMB_TO_PC    = 0x0001,
	REMOTE_CAPTURE_FULL_TO_PC     = 0x0002,
	REMOTE_CAPTURE_THUMB_TO_DRIVE = 0x0004,
	REMOTE_CAPTURE_FULL_TO_DRIVE  = 0x0008
} canonTransferMode;

/**
 * canonDownloadImageType:
 * @CANON_DOWNLOAD_THUMB: Get just the thumbnail for the image
 * @CANON_DOWNLOAD_FULL: Get the full image
 * @CANON_DOWNLOAD_SECONDARY: Get the full secondary image
 *
 * Codes for "Download Captured Image" command to tell the camera to
 * download either the thumbnail or the full image for the most
 * recently captured image.
 *
 */
typedef enum {
	CANON_DOWNLOAD_THUMB = 1,
	CANON_DOWNLOAD_FULL  = 2,
	CANON_DOWNLOAD_SECONDARY  = 3
} canonDownloadImageType;

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
		gp_context_error (context, _("NULL parameter \"%s\" in %s line %i"), #param, __FILE__, __LINE__); \
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
		gp_log (GP_LOG_ERROR, "canon/canon.h", _("NULL parameter \"%s\" in %s line %i"), #param, __FILE__, __LINE__); \
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


/**
 * These ISO, shutter speed, aperture, etc. settings are correct for the
 * EOS 5D and some for the G1; unsure about other cameras.
 */
typedef enum {
	ISO_50 = 0x40,
	ISO_100 = 0x48,
	ISO_125 = 0x4b,
	ISO_160 = 0x4d,
	ISO_200 = 0x50,
	ISO_250 = 0x53,
	ISO_320 = 0x55,
	ISO_400 = 0x58,
	ISO_500 = 0x5b,
	ISO_640 = 0x5d,
	ISO_800 = 0x60,
	ISO_1000 = 0x63,
	ISO_1250 = 0x65,
	ISO_1600 = 0x68,
	ISO_3200 = 0x70
} canonIsoState;

struct canonIsoStateStruct {
	canonIsoState value;
	char *label;
};

struct canonShootingModeStateStruct {
	unsigned char value;
	char *label;
};

typedef enum {
	APERTURE_F1_2 = 0x0d,
	APERTURE_F1_4 = 0x10,
	APERTURE_F1_6 = 0x13,
	APERTURE_F1_8 = 0x15,
	APERTURE_F2_0 = 0x18,
	APERTURE_F2_2 = 0x1b,
	APERTURE_F2_5 = 0x1d,
	APERTURE_F2_8 = 0x20,
	APERTURE_F3_2 = 0x23,
	APERTURE_F3_5 = 0x25,
	APERTURE_F4_0 = 0x28,
	APERTURE_F4_5 = 0x2b,
	APERTURE_F5_0 = 0x2d,
	APERTURE_F5_6 = 0x30,
	APERTURE_F6_3 = 0x33,
	APERTURE_F7_1 = 0x35,
	APERTURE_F8 = 0x38,
	APERTURE_F9 = 0x3b,
	APERTURE_F10 = 0x3d,
	APERTURE_F11 = 0x40,
	APERTURE_F13 = 0x43,
	APERTURE_F14 = 0x45,
	APERTURE_F16 = 0x48,
	APERTURE_F18 = 0x4b,
	APERTURE_F20 = 0x4d,
	APERTURE_F22 = 0x50,
	APERTURE_F25 = 0x53,
	APERTURE_F29 = 0x55,
	APERTURE_F32 = 0x58,
} canonApertureState;

struct canonApertureStateStruct {
	canonApertureState value;
	char *label;
};

typedef enum {
	SHUTTER_SPEED_BULB = 0x04,
	SHUTTER_SPEED_30_SEC = 0x10,
	SHUTTER_SPEED_25_SEC = 0x13,
	SHUTTER_SPEED_20_SEC = 0x15,
	SHUTTER_SPEED_15_SEC = 0x18,
	SHUTTER_SPEED_13_SEC = 0x1b,
	SHUTTER_SPEED_10_SEC = 0x1d,
	SHUTTER_SPEED_8_SEC = 0x20,
	SHUTTER_SPEED_6_SEC = 0x23,
	SHUTTER_SPEED_5_SEC = 0x25,
	SHUTTER_SPEED_4_SEC = 0x28,
	SHUTTER_SPEED_3_2_SEC = 0x2b,
	SHUTTER_SPEED_2_5_SEC = 0x2d,
	SHUTTER_SPEED_2_SEC = 0x30,
	SHUTTER_SPEED_1_6_SEC = 0x32,
	SHUTTER_SPEED_1_3_SEC = 0x35,
	SHUTTER_SPEED_1_SEC = 0x38,
	SHUTTER_SPEED_0_8_SEC = 0x3b,
	SHUTTER_SPEED_0_6_SEC = 0x3d,
	SHUTTER_SPEED_0_5_SEC = 0x40,
	SHUTTER_SPEED_0_4_SEC = 0x43,
	SHUTTER_SPEED_0_3_SEC = 0x45,
	SHUTTER_SPEED_1_4 = 0x48,
	SHUTTER_SPEED_1_5 = 0x4b,
	SHUTTER_SPEED_1_6 = 0x4d,
	SHUTTER_SPEED_1_8 = 0x50,
	SHUTTER_SPEED_1_10 = 0x53,
	SHUTTER_SPEED_1_13 = 0x55,
	SHUTTER_SPEED_1_15 = 0x58,
	SHUTTER_SPEED_1_20 = 0x5b,
	SHUTTER_SPEED_1_25 = 0x5d,
	SHUTTER_SPEED_1_30 = 0x60,
	SHUTTER_SPEED_1_40 = 0x63,
	SHUTTER_SPEED_1_50 = 0x65,
	SHUTTER_SPEED_1_60 = 0x68,
	SHUTTER_SPEED_1_80 = 0x6b,
	SHUTTER_SPEED_1_100 = 0x6d,
	SHUTTER_SPEED_1_125 = 0x70,
	SHUTTER_SPEED_1_160 = 0x73,
	SHUTTER_SPEED_1_200 = 0x75,
	SHUTTER_SPEED_1_250 = 0x78,
	SHUTTER_SPEED_1_320 = 0x7b,
	SHUTTER_SPEED_1_400 = 0x7d,
	SHUTTER_SPEED_1_500 = 0x80,
	SHUTTER_SPEED_1_640 = 0x83,
	SHUTTER_SPEED_1_800 = 0x85,
	SHUTTER_SPEED_1_1000 = 0x88,
	SHUTTER_SPEED_1_1250 = 0x8b,
	SHUTTER_SPEED_1_1600 = 0x8d,
	SHUTTER_SPEED_1_2000 = 0x90,
	SHUTTER_SPEED_1_2500 = 0x93,
	SHUTTER_SPEED_1_3200 = 0x95,
	SHUTTER_SPEED_1_4000 = 0x98,
	SHUTTER_SPEED_1_5000 = 0x9a,
	SHUTTER_SPEED_1_6400 = 0x9d,
	SHUTTER_SPEED_1_8000 = 0xA0
} canonShutterSpeedState;

struct canonShutterSpeedStateStruct {
	canonShutterSpeedState value;
	char *label;
};

typedef enum {
	IMAGE_FORMAT_RAW                        = 0,
	IMAGE_FORMAT_RAW_2,
	IMAGE_FORMAT_RAW_AND_LARGE_FINE_JPEG,
	IMAGE_FORMAT_RAW_AND_LARGE_NORMAL_JPEG,
	IMAGE_FORMAT_RAW_AND_MEDIUM_FINE_JPEG,
	IMAGE_FORMAT_RAW_AND_MEDIUM_NORMAL_JPEG,
	IMAGE_FORMAT_RAW_AND_SMALL_FINE_JPEG,
	IMAGE_FORMAT_RAW_AND_SMALL_NORMAL_JPEG,
	IMAGE_FORMAT_LARGE_FINE_JPEG,
	IMAGE_FORMAT_LARGE_NORMAL_JPEG,
	IMAGE_FORMAT_MEDIUM_FINE_JPEG,
	IMAGE_FORMAT_MEDIUM_NORMAL_JPEG,
	IMAGE_FORMAT_SMALL_FINE_JPEG,
	IMAGE_FORMAT_SMALL_NORMAL_JPEG
} canonImageFormatState;


struct canonImageFormatStateStruct {
	canonImageFormatState value;
	char *label;
	unsigned char res_byte1;
	unsigned char res_byte2;
	unsigned char res_byte3;
};

typedef enum {
	AUTO_FOCUS_ONE_SHOT = 0,
	AUTO_FOCUS_AI_SERVO,
	AUTO_FOCUS_AI_FOCUS,
	MANUAL_FOCUS
} canonFocusModeState;

struct canonFocusModeStateStruct {
	canonFocusModeState value;
	char *label;
};

/**
 * canonFlashMode:
 * @FLASH_MODE_OFF: The flash does not fire
 * @FLASH_MODE_ON: The flash does fire (compulsory)
 * @FLASH_MODE_AUTO: The flash fires if necessary
 */
typedef enum {
	FLASH_MODE_OFF = 0,
	FLASH_MODE_ON,
	FLASH_MODE_AUTO
} canonFlashMode;

struct canonFlashModeStateStruct {
	canonFlashMode value;
	char* label;
};

/**
 * canonBeepMode:
 * @BEEP_OFF: The camera does not beep after focusing the image
 * @BEEP_ON: The camera emits an audible beep after focusing the image
 */
typedef enum {
	BEEP_OFF = 0x00,
	BEEP_ON = 0x01
} canonBeepMode;

struct canonBeepModeStateStruct {
	canonBeepMode value;
	char* label;
};

struct canonZoomLevelStateStruct {
	unsigned char value;
	char* label;
};

struct canonExposureBiasStateStruct {
	unsigned char value;
	char* label;
};


/* Size of the release parameter block */
#define RELEASE_PARAMS_LEN  0x2f

/* These indexes are byte offsets into the release parameter data */
#define IMAGE_FORMAT_1_INDEX  0x01
#define IMAGE_FORMAT_2_INDEX  0x02
#define IMAGE_FORMAT_3_INDEX  0x03
#define SELF_TIMER_1_INDEX  0x04 /* Currently not used */
#define SELF_TIMER_2_INDEX  0x05 /* Currently not used */
#define FLASH_INDEX         0x06
#define BEEP_INDEX          0x07
#define FOCUS_MODE_INDEX    0x12
#define ISO_INDEX           0x1a
#define APERTURE_INDEX      0x1c
#define SHUTTERSPEED_INDEX  0x1e
#define EXPOSUREBIAS_INDEX  0x20
#define SHOOTING_MODE_INDEX 0x08

/**
 * canonCaptureSizeClass:
 * @CAPTURE_COMPATIBILITY: operate in the traditional gphoto2 mode
 * @CAPTURE_THUMB: capture thumbnails
 * @CAPTURE_FULL_IMAGES: capture full-sized images
 *
 * By default (CAPTURE_COMPATIBILITY mode), the driver will capture
 * thumbnails to the host computer in capture_preview, and full-sized
 * images to the camera's drive in capture_image.
 * CAPTURE_FULL_IMAGE will capture a full-sized image to the host
 * computer in capture_preview, or to the camera's drive in capture_image.
 * CAPTURE_THUMB is likewise intended to capture thumbnails to either
 * the host computer or to the camera's drive, although these modes do not
 * seem to work right now.
 *
 */
typedef enum {
	CAPTURE_COMPATIBILITY = 1,
	CAPTURE_THUMB,
	CAPTURE_FULL_IMAGE
} canonCaptureSizeClass;

struct canonCaptureSizeClassStruct {
	canonCaptureSizeClass value;
	char *label;
};

struct canonCamModelData
{
	char *id_str;
	canonCamClass model;
	unsigned short usb_vendor;
	unsigned short usb_product;
	canonCaptureSupport usb_capture_support;
	/* these three constants aren't used properly */
	unsigned int max_movie_size;
	unsigned int max_thumbnail_size;
	unsigned int max_picture_size;
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

	int receive_error; /* status of transfer on serial connection */
	int first_init;  /* first use of camera   1 = yes 0 = no */
	int uploading;   /* 1 = yes ; 0 = no */
	int slow_send;   /* to send data via serial with a usleep(1)
			  * between each byte 1 = yes ; 0 = no */

	unsigned char seq_tx;
	unsigned char seq_rx;

	/* driver settings
	 * leave these as int, as gp_widget_get_value sets them as int!
	 */
	int list_all_files; /* whether to list all files, not just know types */

	int upload_keep_filename; /* 0=DCIF compatible filenames (AUT_*),
				     1=keep original filename */

	char *cached_drive;	/* usually something like C: */
	int cached_ready;       /* whether the camera is ready to rock */

	unsigned char *directory_state;	/* directory content state for wait_for_event */

	long image_key, thumb_length, image_length; /* For immediate download of captured image */
	long image_b_key, image_b_length; /* For immediate download of secondary captured image */
	int capture_step;	/* To record progress in interrupt
				 * reads from capture */
	int transfer_mode;	/* To remember what interrupt messages
				   are expected during capture from
				   newer cameras. */
	int keys_locked;	/* whether the keys are currently
				   locked out */
	unsigned int xfer_length; /* Length of max transfer for
				     download */

	int remote_control;   /* is the camera currently under USB control? */

	canonCaptureSizeClass capture_size; /* Size class for remote-
                                               captured images */

	unsigned int body_id;	/* hardware serial number for some cameras */

	unsigned char release_params[RELEASE_PARAMS_LEN]; /* "Release
							     parameters:"
							     ISO, aperture,
							     etc */

	int secondary_image; /* Should we attempt to download a
				secondary image? (e.g., RAW + JPEG) */

/*
 * Directory access may be rather expensive, so we cached some information.
 * This is now done by libgphoto2, so we are continuously removing this stuff.
 * So the following variables are OBSOLETE.
 */

	int cached_disk;
	int cached_capacity;
	int cached_available;
};

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

/**
 * canonDirentAttributeBits:
 * @CANON_ATTR_WRITE_PROTECTED: File is write-protected
 * @CANON_ATTR_UNKNOWN_2:
 * @CANON_ATTR_UNKNOWN_4:
 * @CANON_ATTR_UNKNOWN_8:
 * @CANON_ATTR_NON_RECURS_ENT_DIR: This entry represents a directory
 *   that was not entered in this listing.
 * @CANON_ATTR_NOT_DOWNLOADED: This file has not yet been downloaded
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
	CANON_ATTR_NOT_DOWNLOADED     = 0x20,
	CANON_ATTR_UNKNOWN_40	      = 0x40,
	CANON_ATTR_RECURS_ENT_DIR     = 0x80
} canonDirentAttributeBits;

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

/* These macros contain the default label for all the
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
			gp_context_error (context, _("Don't know how to handle " \
					     "camera->port->type value %i aka 0x%x " \
					     "in %s line %i."), camera->port->type, \
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

/*
 * All functions returning a pointer have malloc'ed the data. The caller must
 * free() it when done.
 */

int canon_int_ready(Camera *camera, GPContext *context);

char *canon_int_get_disk_name(Camera *camera, GPContext *context);

int canon_int_get_battery(Camera *camera, int *pwr_status, int *pwr_source, GPContext *context);

int canon_int_capture_image (Camera *camera, CameraFilePath *path, GPContext *context);
int canon_int_capture_preview (Camera *camera, unsigned char **data, unsigned int *length,
			       GPContext *context);

int canon_int_get_disk_name_info(Camera *camera, const char *name,int *capacity,int *available, GPContext *context);

int canon_int_list_directory (Camera *camera, const char *folder, CameraList *list, const canonDirlistFunctionBits flags, GPContext *context);
int canon_int_get_info_func (Camera *camera, const char *folder, const char *filename, CameraFileInfo * info, GPContext *context);

int canon_int_get_file(Camera *camera, const char *name, unsigned char **data, unsigned int *length, GPContext *context);
int canon_int_get_thumbnail(Camera *camera, const char *name, unsigned char **retdata, unsigned int *length, GPContext *context);
int canon_int_put_file(Camera *camera, CameraFile *file, const char *filename, const char *destname, const char *destpath, GPContext *context);
int canon_int_wait_for_event (Camera *camera, int timeout, CameraEventType *eventtype, void **eventdata, GPContext *context);
int canon_int_set_file_attributes(Camera *camera, const char *file, const char *dir, canonDirentAttributeBits attrs, GPContext *context);
int canon_int_delete_file(Camera *camera, const char *name, const char *dir, GPContext *context);
int canon_int_set_shutter_speed(Camera *camera, canonShutterSpeedState shutter_speed, GPContext *context);
int canon_int_set_iso(Camera *camera, canonIsoState iso, GPContext *context);
int canon_int_set_shooting_mode (Camera *camera, unsigned char shooting_mode, GPContext *context);
int canon_int_set_aperture(Camera *camera, canonApertureState aperture, GPContext *context);
int canon_int_set_exposurebias(Camera *camera, unsigned char expbias, GPContext *context);
int canon_int_set_focus_mode (Camera *camera, canonFocusModeState focus_mode, GPContext *context);
int canon_int_set_image_format (Camera *camera, unsigned char res_byte1, unsigned char res_byte2, unsigned char res_byte3, GPContext *context);
int canon_serial_off(Camera *camera);
int canon_int_get_time(Camera *camera, time_t *camera_time, GPContext *context);
int canon_int_set_time(Camera *camera, time_t date, GPContext *context);
int canon_int_directory_operations(Camera *camera, const char *path, canonDirFunctionCode action, GPContext *context);
int canon_int_identify_camera(Camera *camera, GPContext *context);
int canon_int_set_owner_name(Camera *camera, const char *name, GPContext *context);
int canon_int_start_remote_control(Camera *camera, GPContext *context);
int canon_int_end_remote_control(Camera *camera, GPContext *context);
int canon_int_set_beep(Camera *camera, canonBeepMode beep_mode, GPContext *context);
int canon_int_set_flash(Camera *camera, canonFlashMode flash_mode, GPContext *context);
int canon_int_set_zoom(Camera *camera, unsigned char zoom_level, GPContext *context);
int canon_int_get_zoom(Camera *camera, unsigned char *zoom_level, unsigned char *zoom_max, GPContext *context);

/*
 * introduced for capturing
 */
int canon_int_get_release_params (Camera *camera, GPContext *context);

void canon_int_find_new_image ( Camera *camera, unsigned char *initial_state, unsigned char *final_state, CameraFilePath *path );


/* path conversion - needs drive letter, and therefore cannot be moved
 * to util.c */
const char *gphoto2canonpath(Camera *camera, const char *path, GPContext *context);

const char *canon_int_filename2thumbname (Camera *camera, const char *filename);
const char *canon_int_filename2audioname (Camera *camera, const char *filename);

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
