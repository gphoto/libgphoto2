/****************************************************************************
 *
 * File: usb.h
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
 * @CANON_USB_FUNCTION_DISK_INFO_2: get disk info for newer protocol
 *   (capacity and free space)
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
 * @CANON_USB_FUNCTION_CONTROL_CAMERA_2: Replacement for
 *   %CANON_USB_FUNCTION_CONTROL_CAMERA, with many similarities, first
 *   seen with EOS 20D.
 * @CANON_USB_FUNCTION_RETRIEVE_CAPTURE_2: Same function as
 *   %CANON_USB_FUNCTION_RETRIEVE_CAPTURE, but first seen on EOS 20D.
 * @CANON_USB_FUNCTION_LOCK_KEYS_2: Same as %CANON_USB_FUNCTION_EOS_LOCK_KEYS,
 *   but for newer protocol.
 * @CANON_USB_FUNCTION_UNLOCK_KEYS_2: Same as %CANON_USB_FUNCTION_EOS_UNLOCK_KEYS,
 *   but for newer protocol.
 * @CANON_USB_FUNCTION_SET_ATTR_2: Presumed code to set attribute bits
 *   for a file on an EOS 20D and its ilk.
 * @CANON_USB_FUNCTION_CAMERA_CHOWN_2: Same as %CANON_USB_FUNCTION_CAMERA_CHOWN,
 *  but for newer protocol.
 * @CANON_USB_FUNCTION_GET_OWNER: Gets just the owner name, in newer protocol.
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
	CANON_USB_FUNCTION_DISK_INFO_2,
	CANON_USB_FUNCTION_FLASH_DEVICE_IDENT_2,
	CANON_USB_FUNCTION_POWER_STATUS_2,
	CANON_USB_FUNCTION_UNKNOWN_FUNCTION,
	CANON_USB_FUNCTION_EOS_GET_BODY_ID,
	CANON_USB_FUNCTION_SET_FILE_TIME,
	CANON_USB_FUNCTION_20D_UNKNOWN_1,
	CANON_USB_FUNCTION_20D_UNKNOWN_2,
	CANON_USB_FUNCTION_EOS_GET_BODY_ID_2,
	CANON_USB_FUNCTION_GET_PIC_ABILITIES_2,
	CANON_USB_FUNCTION_CONTROL_CAMERA_2,
	CANON_USB_FUNCTION_RETRIEVE_CAPTURE_2,
	CANON_USB_FUNCTION_LOCK_KEYS_2,
	CANON_USB_FUNCTION_UNLOCK_KEYS_2,
	CANON_USB_FUNCTION_DELETE_FILE_2,
	CANON_USB_FUNCTION_SET_ATTR_2,
	CANON_USB_FUNCTION_CAMERA_CHOWN_2,
	CANON_USB_FUNCTION_GET_OWNER,
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
 * @CANON_USB_CONTROL_UNKNOWN_1: part of new protocol, function unknown.
 * @CANON_USB_CONTROL_UNKNOWN_2: part of new protocol, function unknown.
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
	CANON_USB_CONTROL_UNKNOWN_1,
	CANON_USB_CONTROL_UNKNOWN_2
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


/* USB command data structures defined in usb.c */
/*extern const struct canon_usb_cmdstruct canon_usb_cmd[];*/

extern const struct canon_usb_control_cmdstruct canon_usb_control_cmd[];


/* For mapping status codes to intelligible messages */
struct canon_usb_status {
	int code;
	char *message;
};

/****************************************************************************
 *
 * prototypes
 *
 ****************************************************************************/

int canon_usb_init (Camera *camera, GPContext *context);
int canon_usb_set_file_time ( Camera *camera, char *camera_filename, time_t time, GPContext *context);
int canon_usb_put_file (Camera *camera, CameraFile *file, const char *filename, const char *destname, const char *destpath,
	        GPContext *context);
unsigned char *canon_usb_capture_dialogue (Camera *camera, unsigned int *return_length, int *photo_status, GPContext *context );
unsigned char *canon_usb_dialogue_full (Camera *camera, canonCommandIndex canon_funct,
				   unsigned int *return_length, const unsigned char *payload, unsigned int payload_length);
unsigned char *canon_usb_dialogue (Camera *camera, canonCommandIndex canon_funct,
				   unsigned int *return_length, const unsigned char *payload, unsigned int payload_length);
int canon_usb_long_dialogue (Camera *camera, canonCommandIndex canon_funct, unsigned char **data,
		unsigned int *data_length, unsigned int max_data_size, const unsigned char *payload,
		unsigned int payload_length, int display_status, GPContext *context);
int canon_usb_get_file (Camera *camera, const char *name, unsigned char **data, unsigned int *length, GPContext *context);
int canon_usb_get_thumbnail (Camera *camera, const char *name, unsigned char **data, unsigned int *length, GPContext *context);
int canon_usb_get_captured_image (Camera *camera, const int key, unsigned char **data, unsigned int *length, GPContext *context);
int canon_usb_get_captured_secondary_image (Camera *camera, const int key, unsigned char **data, unsigned int *length, GPContext *context);
int canon_usb_get_captured_thumbnail (Camera *camera, const int key, unsigned char **data, unsigned int *length, GPContext *context);
int canon_usb_lock_keys(Camera *camera, GPContext *context);
int canon_usb_unlock_keys(Camera *camera, GPContext *context);
int canon_usb_get_dirents (Camera *camera, unsigned char **dirent_data, unsigned int *dirents_length, const char *path, GPContext *context);
int canon_usb_list_all_dirs (Camera *camera, unsigned char **dirent_data,
			     unsigned int *dirents_length, GPContext *context);
int canon_usb_set_file_attributes (Camera *camera,
				   unsigned int attr_bits,
				   const char *dir, const char *file,
				   GPContext *context);
int canon_usb_ready (Camera *camera, GPContext *context);

int canon_usb_wait_for_event (Camera *camera, int timeout,
			CameraEventType *eventtype, void **eventdata,
			GPContext *context);
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
