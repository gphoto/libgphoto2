/****************************************************************************
 *
 * File: usb.h
 *
 * $Id$
 *
 ****************************************************************************/

#ifndef _CANON_USB_H
#define _CANON_USB_H

/**
 * USB_BULK_READ_SIZE
 *
 * Maximum size to be used for a USB "bulk read" operation
 *
 */
#define USB_BULK_READ_SIZE 0x1400


/**
 * USB_BULK_WRITE_SIZE
 *
 * Maximum size to be used for a USB "bulk write" operation
 *
 */
/* #define USB_BULK_WRITE_SIZE 0xC000 */
#define USB_BULK_WRITE_SIZE 0x1400

/**
 * canonCommandIndex:
 * @CANON_USB_FUNCTION_GET_FILE: Command to download a file from the camera.
 * @CANON_USB_FUNCTION_IDENTIFY_CAMERA: Command to read the firmware version and
 *   strings with the camera type and owner from the camera.
 * @CANON_USB_FUNCTION_GET_TIME: Command to get the time in Unix time format from the camera.
 * @CANON_USB_FUNCTION_SET_TIME: Command to set the camera's internal time.
 * @CANON_USB_FUNCTION_MKDIR: Command to create a directory on the camera storage device.
 * @CANON_USB_FUNCTION_CAMERA_CHOWN: Change "owner" string on camera
 * @CANON_USB_FUNCTION_RMDIR: Command to delete a directory from camera storage.
 * @CANON_USB_FUNCTION_DISK_INFO: Command to get disk information from
 *   the camera, given a disk designator (e.g. "D:"). Returns total
 *   capacity and free space.
 * @CANON_USB_FUNCTION_FLASH_DEVICE_IDENT: Command to request the disk specifier
 *   (drive letter) for the storage device being used.
 * @CANON_USB_FUNCTION_POWER_STATUS: Command to query the camera for its power status:
 *   battery vs. mains, and whether the battery is low.
 * @CANON_USB_FUNCTION_GET_DIRENT: Get directory entries
 * @CANON_USB_FUNCTION_DELETE_FILE: Delete file
 * @CANON_USB_FUNCTION_SET_ATTR: Command to set the attributes of a
 *   file on the camera (e.g. downloaded, protect from delete).
 * @CANON_USB_FUNCTION_GET_PIC_ABILITIES: Command to "get picture
 *   abilities", which seems to be a list of the different sizes and
 *   quality of images that are available on this camera. Not
 *   implemented (and will cause an error) on the EOS cameras or on
 *   newer PowerShot cameras such as S45, G3, G5.
 * @CANON_USB_FUNCTION_GENERIC_LOCK_KEYS: Command to lock keys (and
 *   turn on "PC" indicator) on non-EOS cameras.
 * @CANON_USB_FUNCTION_EOS_LOCK_KEYS: Lock keys (EOS cameras)
 * @CANON_USB_FUNCTION_EOS_UNLOCK_KEYS: Unlock keys (EOS cameras)
 * @CANON_USB_FUNCTION_RETRIEVE_CAPTURE: Command to retrieve the last
 *   image captured, depending on the transfer mode set via
 *   %CANON_USB_FUNCTION_CONTROL_CAMERA with subcommand
 *   %CANON_USB_CONTROL_SET_TRANSFER_MODE.
 * @CANON_USB_FUNCTION_RETRIEVE_PREVIEW: Command to retrieve a preview
 *   image.
 * @CANON_USB_FUNCTION_CONTROL_CAMERA: Remote camera control (with
 *   many subcodes)
 * @CANON_USB_FUNCTION_FLASH_DEVICE_IDENT_2: Command to request the
 *   disk specifier (drive letter) for the storage device being
 *   used. Used with the "newer" protocol, e.g. with EOS 20D.
 * @CANON_USB_FUNCTION_POWER_STATUS_2: Command to query the camera for
 *   its power status: battery vs. mains, and whether the battery is
 *   low. Used in the "newer" protocol, e.g. with EOS 20D.
 * @CANON_USB_FUNCTION_UNKNOWN_FUNCTION: Don't know what this is for;
 *   it has been sighted in USB trace logs for an EOS D30, but not for
 *   a D60 or for any PowerShot camera.
 * @CANON_USB_FUNCTION_EOS_GET_BODY_ID: Command to read the body ID (serial number)
 *   from an EOS camera.
 * @CANON_USB_FUNCTION_SET_FILE_TIME: Set file time
 * @CANON_USB_FUNCTION_20D_UNKNOWN_1:  First seen with EOS 20D, not yet understood.
 * @CANON_USB_FUNCTION_20D_UNKNOWN_2:  First seen with EOS 20D, not yet understood.
 * @CANON_USB_FUNCTION_EOS_GET_BODY_ID_2: Same function as
 *   %CANON_USB_FUNCTION_EOS_GET_BODY_ID, but first seen on EOS 20D.
 * @CANON_USB_FUNCTION_GET_PIC_ABILITIES_2: Same function as
 *   %CANON_USB_FUNCTION_GET_PIC_ABILITIES, but first seen on EOS 20D.
 * @CANON_USB_FUNCTION_20D_UNKNOWN_3: First seen with EOS 20D, not yet
 *   understood.
 * @CANON_USB_FUNCTION_20D_RETRIEVE_CAPTURE_2: Same function as
 *   %CANON_USB_FUNCTION_RETRIEVE_CAPTURE, but first seen on EOS 20D.
 * @CANON_USB_FUNCTION_20D_UNKNOWN_4: First seen with EOS 20D, not yet
 *   understood.
 *
 * Codes to give to canon_usb_dialogue() or canon_usb_long_dialogue()
 * to select which command to issue to the camera. See the protocol
 * document for details.
 */
 
typedef enum {
	CANON_USB_FUNCTION_GET_FILE = 1,
	CANON_USB_FUNCTION_IDENTIFY_CAMERA,
	CANON_USB_FUNCTION_GET_TIME,
	CANON_USB_FUNCTION_SET_TIME,
	CANON_USB_FUNCTION_MKDIR,
	CANON_USB_FUNCTION_CAMERA_CHOWN,
	CANON_USB_FUNCTION_RMDIR,
	CANON_USB_FUNCTION_DISK_INFO,
	CANON_USB_FUNCTION_FLASH_DEVICE_IDENT,
	CANON_USB_FUNCTION_POWER_STATUS,
	CANON_USB_FUNCTION_GET_DIRENT,
	CANON_USB_FUNCTION_DELETE_FILE,
	CANON_USB_FUNCTION_SET_ATTR,
	CANON_USB_FUNCTION_GET_PIC_ABILITIES,
	CANON_USB_FUNCTION_GENERIC_LOCK_KEYS,
	CANON_USB_FUNCTION_EOS_LOCK_KEYS,
	CANON_USB_FUNCTION_EOS_UNLOCK_KEYS,
	CANON_USB_FUNCTION_RETRIEVE_CAPTURE,
	CANON_USB_FUNCTION_RETRIEVE_PREVIEW,
	CANON_USB_FUNCTION_CONTROL_CAMERA,
	CANON_USB_FUNCTION_FLASH_DEVICE_IDENT_2,
	CANON_USB_FUNCTION_POWER_STATUS_2,
	CANON_USB_FUNCTION_UNKNOWN_FUNCTION,
	CANON_USB_FUNCTION_EOS_GET_BODY_ID,
	CANON_USB_FUNCTION_SET_FILE_TIME,
	CANON_USB_FUNCTION_20D_UNKNOWN_1,
	CANON_USB_FUNCTION_20D_UNKNOWN_2,
	CANON_USB_FUNCTION_EOS_GET_BODY_ID_2,
	CANON_USB_FUNCTION_GET_PIC_ABILITIES_2,
	CANON_USB_FUNCTION_20D_UNKNOWN_3,
	CANON_USB_FUNCTION_20D_RETRIEVE_CAPTURE_2,
	CANON_USB_FUNCTION_20D_UNKNOWN_4,
} canonCommandIndex;

/**
 * canonSubcommandIndex:
 * @CANON_USB_CONTROL_INIT: Enter camera control mode
 * @CANON_USB_CONTROL_SHUTTER_RELEASE: Release camera shutter (capture still)
 * @CANON_USB_CONTROL_SET_PARAMS: Set release parameters (AE mode, beep, etc.)
 * @CANON_USB_CONTROL_SET_TRANSFER_MODE: Set transfer mode for next image
 *  capture. Either the full image, a thumbnail or both may be either
 *  stored on the camera, downloaded to the host, or both.
 * @CANON_USB_CONTROL_GET_PARAMS: Read the same parameters set by
 *  @CANON_USB_CONTROL_SET_PARAMS.
 * @CANON_USB_CONTROL_GET_ZOOM_POS: Get the position of the zoom lens
 * @CANON_USB_CONTROL_SET_ZOOM_POS: Set the position of the zoom lens
 * @CANON_USB_CONTROL_GET_EXT_PARAMS_SIZE: Get the size of the "extended
 *   release parameters".
 * @CANON_USB_CONTROL_GET_EXT_PARAMS: Get the "extended release parameters".
 * @CANON_USB_CONTROL_EXIT: Leave camera control mode; opposite of
 *   @CANON_USB_CONTROL_INIT.
 * @CANON_USB_CONTROL_VIEWFINDER_START: Switch video viewfinder on.
 * @CANON_USB_CONTROL_VIEWFINDER_STOP: Swictch video viewfinder off.
 * @CANON_USB_CONTROL_GET_AVAILABLE_SHOT: Get estimated number of images
 *   that can be captured in current mode before filling flash card.
 * @CANON_USB_CONTROL_SET_CUSTOM_FUNC:  Not yet seen in USB trace.
 * @CANON_USB_CONTROL_GET_CUSTOM_FUNC: Read custom functions from an EOS camera
 * @CANON_USB_CONTROL_GET_EXT_PARAMS_VER:  Not yet seen in USB trace.
 * @CANON_USB_CONTROL_SET_EXT_PARAMS:  Not yet seen in USB trace.
 * @CANON_USB_CONTROL_SELECT_CAM_OUTPUT:  Not yet seen in USB trace.
 * @CANON_USB_CONTROL_DO_AE_AF_AWB:  Not yet seen in USB trace.
 *
 * CANON_USB_FUNCTION_CONTROL_CAMERA commands are used for a wide range
 * of remote camera control actions.  A control_cmdstruct is defined
 * below for the following remote camera control options.
 */
typedef enum {
	CANON_USB_CONTROL_INIT = 1,
	CANON_USB_CONTROL_SHUTTER_RELEASE,
	CANON_USB_CONTROL_SET_PARAMS,
	CANON_USB_CONTROL_SET_TRANSFER_MODE,
	CANON_USB_CONTROL_GET_PARAMS,
	CANON_USB_CONTROL_GET_ZOOM_POS,
	CANON_USB_CONTROL_SET_ZOOM_POS,
	CANON_USB_CONTROL_GET_EXT_PARAMS_SIZE,
	CANON_USB_CONTROL_GET_EXT_PARAMS,
	CANON_USB_CONTROL_EXIT,
	CANON_USB_CONTROL_VIEWFINDER_START,
	CANON_USB_CONTROL_VIEWFINDER_STOP,
	CANON_USB_CONTROL_GET_AVAILABLE_SHOT,
	CANON_USB_CONTROL_SET_CUSTOM_FUNC,	/* Not yet seen in USB trace */
	CANON_USB_CONTROL_GET_CUSTOM_FUNC,
	CANON_USB_CONTROL_GET_EXT_PARAMS_VER,	/* Not yet seen in USB trace */
	CANON_USB_CONTROL_SET_EXT_PARAMS,	/* Not yet seen in USB trace */
	CANON_USB_CONTROL_SELECT_CAM_OUTPUT,	/* Not yet seen in USB trace */
	CANON_USB_CONTROL_DO_AE_AF_AWB,		/* Not yet seen in USB trace */
} canonSubcommandIndex;


struct canon_usb_control_cmdstruct 
{
	canonSubcommandIndex num;
	char *description;
	char subcmd;
	int cmd_length;
	int additional_return_length;
};

/**
 * MAX_INTERRUPT_TRIES
 *
 * Maximum number of times to try a read from the interrupt pipe. We
 * will keep reading until an error, a read of non-zero length, or for
 * a maximum of this many times.
 */
#define MAX_INTERRUPT_TRIES 12000



struct canon_usb_cmdstruct 
{
	canonCommandIndex num;
	char *description;
	char cmd1, cmd2;
	int cmd3;
	int return_length;
};

/*
 * Command codes are structured:
 *   cmd2=11 -> camera control,
 *   cmd2=12 -> storage control.
 *
 *   cmd3=201 -> fixed length response
 *   cmd3=202 -> variable length response
 */

static const struct canon_usb_cmdstruct canon_usb_cmd[] = {
	{CANON_USB_FUNCTION_GET_FILE,		"Get file",			0x01, 0x11, 0x202,	0x40},
	{CANON_USB_FUNCTION_IDENTIFY_CAMERA,	"Identify camera",		0x01, 0x12, 0x201,	0x9c},
	{CANON_USB_FUNCTION_GET_TIME,		"Get time",			0x03, 0x12, 0x201,	0x60},
	{CANON_USB_FUNCTION_SET_TIME,		"Set time",			0x04, 0x12, 0x201,	0x54},
	{CANON_USB_FUNCTION_MKDIR,		"Make directory",		0x05, 0x11, 0x201,	0x54},
	{CANON_USB_FUNCTION_CAMERA_CHOWN,	"Change camera owner",		0x05, 0x12, 0x201,	0x54},
	{CANON_USB_FUNCTION_RMDIR,		"Remove directory",		0x06, 0x11, 0x201,	0x54},
	{CANON_USB_FUNCTION_DISK_INFO,		"Disk info request",		0x09, 0x11, 0x201,	0x5c},
	{CANON_USB_FUNCTION_FLASH_DEVICE_IDENT,	"Flash device ident",		0x0a, 0x11, 0x202,	0x40},
	{CANON_USB_FUNCTION_POWER_STATUS,	"Power supply status",		0x0a, 0x12, 0x201,	0x58},
	{CANON_USB_FUNCTION_GET_DIRENT,		"Get directory entries",	0x0b, 0x11, 0x202,	0x40},
	{CANON_USB_FUNCTION_DELETE_FILE,	"Delete file",			0x0d, 0x11, 0x201,	0x54},
	/* Command code 0x0e is overloaded: set file attribute (old),
	 * flash device ID (new). And the response is different: fixed
	 * length in old, variable length in new. */
	{CANON_USB_FUNCTION_SET_ATTR,		"Set file attribute",		0x0e, 0x11, 0x201,	0x54},
	{CANON_USB_FUNCTION_FLASH_DEVICE_IDENT_2, "Flash device ident",		0x0e, 0x11, 0x202,	0x40},
	{CANON_USB_FUNCTION_SET_FILE_TIME,	"Set file time",		0x0f, 0x11, 0x201,	0x54},
	/* Notice the overloaded command code 0x13: remote camera control
	   in the original protocol, power status in the new protocol. */
	{CANON_USB_FUNCTION_CONTROL_CAMERA,	"Remote camera control",	0x13, 0x12, 0x201,      0x40},
	{CANON_USB_FUNCTION_POWER_STATUS_2,	"Power supply status (new protocol)",	0x13, 0x12, 0x201,      0x58},
	{CANON_USB_FUNCTION_RETRIEVE_CAPTURE,	"Download a captured image",	0x17, 0x12, 0x202,      0x40},
	{CANON_USB_FUNCTION_RETRIEVE_PREVIEW,	"Download a captured preview",	0x18, 0x12, 0x202,      0x40},
	{CANON_USB_FUNCTION_UNKNOWN_FUNCTION,	"Unknown function",		0x1a, 0x12, 0x201,	0x80},
	{CANON_USB_FUNCTION_EOS_LOCK_KEYS,	"EOS lock keys",		0x1b, 0x12, 0x201,	0x54},
	{CANON_USB_FUNCTION_EOS_UNLOCK_KEYS,	"EOS unlock keys",		0x1c, 0x12, 0x201,	0x54},
	{CANON_USB_FUNCTION_EOS_GET_BODY_ID,	"EOS get body ID",		0x1d, 0x12, 0x201,	0x58},
	{CANON_USB_FUNCTION_GET_PIC_ABILITIES,	"Get picture abilities",	0x1f, 0x12, 0x201,	0x384},
	{CANON_USB_FUNCTION_GENERIC_LOCK_KEYS,	"Lock keys and turn off LCD",	0x20, 0x12, 0x201,	0x54},
	{CANON_USB_FUNCTION_20D_UNKNOWN_1,	"Unknown EOS 20D function",	0x21, 0x12, 0x201,	0x54},
	{CANON_USB_FUNCTION_20D_UNKNOWN_2,	"Unknown EOS 20D function",	0x22, 0x12, 0x201,	0x54},
	{CANON_USB_FUNCTION_EOS_GET_BODY_ID_2,	"New EOS get body ID",		0x23, 0x12, 0x201,	0x54},
	{CANON_USB_FUNCTION_GET_PIC_ABILITIES_2, "New get picture abilities",	0x24, 0x12, 0x201,	0x474},
	{CANON_USB_FUNCTION_20D_UNKNOWN_3,	"Unknown EOS 20D function",	0x25, 0x12, 0x201,	0x54},
	{CANON_USB_FUNCTION_20D_RETRIEVE_CAPTURE_2, "New download a captured image", 0x26, 0x12, 0x202,	0x54},
	{CANON_USB_FUNCTION_20D_UNKNOWN_4,	"Unknown EOS 20D function",	0x36, 0x12, 0x201,	0x54},
	{ 0 }
};



static const struct canon_usb_control_cmdstruct canon_usb_control_cmd[] = {
	/* COMMAND NAME                         Description            Value   CmdLen ReplyLen */
	{CANON_USB_CONTROL_INIT,                "Camera control init",  0x00,  0x18,  0x1c},  /* load 0x00, 0x00 */
	{CANON_USB_CONTROL_SHUTTER_RELEASE,     "Release shutter",      0x04,  0x18,  0x1c},  /* load 0x04, 0x00 */
	{CANON_USB_CONTROL_SET_PARAMS,          "Set release params",   0x07,  0x00,  0x1c},  /* ?? */
	{CANON_USB_CONTROL_SET_TRANSFER_MODE,   "Set transfer mode",    0x09,  0x1c,  0x1c},  /* load (0x09, 0x04, 0x03) or (0x09, 0x04, 0x02000003) */
	{CANON_USB_CONTROL_GET_PARAMS,          "Get release params",   0x0a,  0x18,  0x4c},  /* load 0x0a, 0x00 */
	{CANON_USB_CONTROL_GET_ZOOM_POS,        "Get zoom position",    0x0b,  0x18,  0x20},  /* load 0x0b, 0x00 */
	{CANON_USB_CONTROL_SET_ZOOM_POS,        "Set zoom position",    0x0c,  0x1c,  0x1c},  /* load 0x0c, 0x04, 0x01 (or 0x0c, 0x04, 0x0b) (or 0x0c, 0x04, 0x0a) or (0x0c, 0x04, 0x09) or (0x0c, 0x04, 0x08) or (0x0c, 0x04, 0x07) or (0x0c, 0x04, 0x06) or (0x0c, 0x04, 0x00) */
	{CANON_USB_CONTROL_GET_AVAILABLE_SHOT,  "Get available shot",   0x0d,  0x18,  0x20},
	{CANON_USB_CONTROL_GET_CUSTOM_FUNC,     "Get custom func.",     0x0f,  0x22,  0x26},
	{CANON_USB_CONTROL_GET_EXT_PARAMS_SIZE, "Get extended release params size",              
	                                                                0x10,  0x1c,  0x20},  /* load 0x10, 0x00 */
	{CANON_USB_CONTROL_GET_EXT_PARAMS,      "Get extended release params",              
	                                                                0x12,  0x1c,  0x2c},  /* load 0x12, 0x04, 0x10 */

	/* unobserved, commands present in canon headers defines, but need more usb snoops to get reply lengths */
	{CANON_USB_CONTROL_EXIT,                "Exit release control", 0x01,  0x18,  0x1c},
	{CANON_USB_CONTROL_VIEWFINDER_START,    "Start viewfinder",     0x02,  0x00,  0x00},
	{CANON_USB_CONTROL_VIEWFINDER_STOP,     "Stop viewfinder",      0x03,  0x00,  0x00},
	{CANON_USB_CONTROL_SET_CUSTOM_FUNC,     "Set custom func.",     0x0e,  0x00,  0x00},
	{CANON_USB_CONTROL_GET_EXT_PARAMS_VER,  "Get extended params version",
	                                                                0x11,  0x00,  0x00},
	{CANON_USB_CONTROL_SET_EXT_PARAMS,      "Set extended params",  0x13,  0x00,  0x00},
	{CANON_USB_CONTROL_SELECT_CAM_OUTPUT,   "Select camera output", 0x14,  0x00,  0x00}, /* LCD (0x1), Video out (0x2), or OFF (0x3) */
	{CANON_USB_CONTROL_DO_AE_AF_AWB,        "Do AE, AF, and AWB",   0x15,  0x00,  0x00}, 
	{ 0 }
};

/****************************************************************************
 *
 * prototypes
 *
 ****************************************************************************/

int canon_usb_init (Camera *camera, GPContext *context);
int canon_usb_camera_init (Camera *camera, GPContext *context);
int canon_usb_set_file_time ( Camera *camera, char *camera_filename, time_t time, GPContext *context);
int canon_usb_put_file (Camera *camera, CameraFile *file, char *destname, char *destpath, 
	        GPContext *context);
unsigned char *canon_usb_capture_dialogue (Camera *camera, int *return_length, GPContext *context );
unsigned char *canon_usb_dialogue (Camera *camera, canonCommandIndex canon_funct,
				   int *return_length, const char *payload, int payload_length);
int canon_usb_long_dialogue (Camera *camera, canonCommandIndex canon_funct, unsigned char **data, 
		int *data_length, int max_data_size, const char *payload,
		int payload_length, int display_status, GPContext *context);
int canon_usb_get_file (Camera *camera, const char *name, unsigned char **data, int *length, GPContext *context);
int canon_usb_get_thumbnail (Camera *camera, const char *name, unsigned char **data, int *length, GPContext *context);
int canon_usb_poll_interrupt_multiple ( Camera *camera[], int n_cameras,
					int camera_flags[],
					unsigned char *buf, int n_tries,
					int *which );
int canon_usb_get_captured_image (Camera *camera, const int key, unsigned char **data, int *length, GPContext *context);
int canon_usb_get_captured_thumbnail (Camera *camera, const int key, unsigned char **data, int *length, GPContext *context);
int canon_usb_lock_keys(Camera *camera, GPContext *context);
int canon_usb_unlock_keys(Camera *camera, GPContext *context);
int canon_usb_get_dirents (Camera *camera, unsigned char **dirent_data, unsigned int *dirents_length, const char *path, GPContext *context);
int canon_usb_list_all_dirs (Camera *camera, unsigned char **dirent_data,
			     unsigned int *dirents_length, GPContext *context);
int canon_usb_set_file_attributes (Camera *camera, unsigned short attr_bits,
			       const char *pathname, GPContext *context);
int canon_usb_ready (Camera *camera);
int canon_usb_identify (Camera *camera, GPContext *context);

#endif /* _CANON_USB_H */

/****************************************************************************
 *
 * End of file: usb.h
 *
 ****************************************************************************/

/*
 * Local Variables:
 * c-file-style:"linux"
 * indent-tabs-mode:t
 * End:
 */
