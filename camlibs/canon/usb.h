/****************************************************************************
 *
 * File: usb.h
 *
 * $Id$
 *
 ****************************************************************************/

#ifndef _CANON_USB_H
#define _CANON_USB_H

/****************************************************************************
 *
 * prototypes
 *
 ****************************************************************************/

int canon_usb_init (Camera *camera, GPContext *context);
int canon_usb_camera_init (Camera *camera, GPContext *context);
int canon_usb_put_file (Camera *camera, CameraFile *file, char *destname, char *destpath, 
	        GPContext *context);
unsigned char *canon_usb_capture_dialogue (Camera *camera, int *return_length);
unsigned char *canon_usb_dialogue (Camera *camera, int canon_funct, int *return_length, 
		const char *payload, int payload_length);
int canon_usb_long_dialogue (Camera *camera, int canon_funct, unsigned char **data, 
		int *data_length, int max_data_size, const char *payload,
		int payload_length, int display_status, GPContext *context);
int canon_usb_get_file (Camera *camera, const char *name, unsigned char **data, int *length, GPContext *context);
int canon_usb_get_thumbnail (Camera *camera, const char *name, unsigned char **data, int *length, GPContext *context);
int canon_usb_lock_keys(Camera *camera, GPContext *context);
int canon_usb_unlock_keys(Camera *camera);
int canon_usb_get_dirents (Camera *camera, unsigned char **dirent_data, unsigned int *dirents_length, const char *path, GPContext *context);
int canon_usb_ready (Camera *camera);
int canon_usb_identify (Camera *camera, GPContext *context);

#define USB_BULK_READ_SIZE 0x3000
#define USB_BULK_WRITE_SIZE 0xC000

#define CANON_USB_FUNCTION_GET_FILE		1
#define CANON_USB_FUNCTION_IDENTIFY_CAMERA	2
#define CANON_USB_FUNCTION_GET_TIME		3
#define CANON_USB_FUNCTION_SET_TIME		4
#define CANON_USB_FUNCTION_MKDIR		5
#define CANON_USB_FUNCTION_CAMERA_CHOWN		6
#define CANON_USB_FUNCTION_RMDIR		7
#define CANON_USB_FUNCTION_DISK_INFO		8
#define CANON_USB_FUNCTION_FLASH_DEVICE_IDENT	9
#define CANON_USB_FUNCTION_POWER_STATUS		10
#define CANON_USB_FUNCTION_GET_DIRENT		11
#define CANON_USB_FUNCTION_DELETE_FILE		12
#define CANON_USB_FUNCTION_SET_ATTR		13
/* GET_PIC_ABILITIES currently not implemented in gPhoto2 camlibs/canon driver */
#define CANON_USB_FUNCTION_GET_PIC_ABILITIES	14
#define CANON_USB_FUNCTION_GENERIC_LOCK_KEYS	15
#define CANON_USB_FUNCTION_EOS_LOCK_KEYS	16
#define CANON_USB_FUNCTION_EOS_UNLOCK_KEYS	17

#define CANON_USB_FUNCTION_EOS_GET_BODY_ID	22



#define CANON_USB_FUNCTION_RETRIEVE_CAPTURE     18
#define CANON_USB_FUNCTION_RETRIEVE_PREVIEW     19
#define CANON_USB_FUNCTION_CONTROL_CAMERA       20
#define CANON_USB_FUNCTION_UNKNOWN_FUNCTION	21

/* 
 * CANON_USB_FUNCTION_CONTROL_CAMERA commands are used for a wide range
 * of remote camera control actions.  A control_cmdstruct is defined
 * below for the following remote camera control options.
 */
#define CANON_USB_CONTROL_INIT                  1
#define CANON_USB_CONTROL_SHUTTER_RELEASE       2
#define CANON_USB_CONTROL_SET_PARAMS            3
#define CANON_USB_CONTROL_SET_TRANSFER_MODE     4
#define CANON_USB_CONTROL_GET_PARAMS            5
#define CANON_USB_CONTROL_GET_ZOOM_POS          6
#define CANON_USB_CONTROL_SET_ZOOM_POS          7
#define CANON_USB_CONTROL_GET_EXT_PARAMS_SIZE   8
#define CANON_USB_CONTROL_GET_EXT_PARAMS        9
/* unobserved, commands present in canon headers defines */
#define CANON_USB_CONTROL_EXIT                  10
#define CANON_USB_CONTROL_VIEWFINDER_START      11
#define CANON_USB_CONTROL_VIEWFINDER_STOP       12
#define CANON_USB_CONTROL_GET_AVAILABLE_SHOT    13
#define CANON_USB_CONTROL_SET_CUSTOM_FUNC       14
#define CANON_USB_CONTROL_GET_CUSTOM_FUNC       15
#define CANON_USB_CONTROL_GET_EXT_PARAMS_VER    16
#define CANON_USB_CONTROL_SET_EXT_PARAMS        17
#define CANON_USB_CONTROL_SELECT_CAM_OUTPUT     18
#define CANON_USB_CONTROL_DO_AE_AF_AWB          19


struct canon_usb_control_cmdstruct 
{
	int num;
	char *description;
	char subcmd;
	int cmd_length;
	int additional_return_length;
};




struct canon_usb_cmdstruct 
{
	int num;
	char *description;
	char cmd1, cmd2;
	int cmd3;
	int return_length;
};

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
	{CANON_USB_FUNCTION_SET_ATTR,		"Set file attribute",		0x0e, 0x11, 0x201,	0x54},
	{CANON_USB_FUNCTION_CONTROL_CAMERA,     "Remote camera control",        0x13, 0x12, 0x201,      0x40},
	{CANON_USB_FUNCTION_RETRIEVE_CAPTURE,   "Download a captured image",    0x17, 0x12, 0x202,      0x40},
	{CANON_USB_FUNCTION_RETRIEVE_PREVIEW,   "Download a captured preview",  0x18, 0x12, 0x202,      0x40},
	{CANON_USB_FUNCTION_UNKNOWN_FUNCTION,	"Unknown function",		0x1a, 0x12, 0x201,	0x80},
	{CANON_USB_FUNCTION_EOS_LOCK_KEYS,	"EOS lock keys",		0x1b, 0x12, 0x201,	0x54},
	{CANON_USB_FUNCTION_EOS_UNLOCK_KEYS,	"EOS unlock keys",		0x1c, 0x12, 0x201,	0x54},
	{CANON_USB_FUNCTION_EOS_GET_BODY_ID,	"EOS get body ID",		0x1d, 0x12, 0x201,	0x58},
	{CANON_USB_FUNCTION_GET_PIC_ABILITIES,	"Get picture abilities",	0x1f, 0x12, 0x201,	0x384},
	{CANON_USB_FUNCTION_GENERIC_LOCK_KEYS,	"Lock keys and turn off LCD",	0x20, 0x12, 0x201,	0x54},
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
	{CANON_USB_CONTROL_GET_AVAILABLE_SHOT,  "Get available shot",   0x0d,  0x18,  0x60},
	{CANON_USB_CONTROL_GET_CUSTOM_FUNC,     "Get custom func.",     0x0f,  0x22,  0x66},
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

#define REMOTE_CAPTURE_THUMB_TO_PC    (0x0001)
#define REMOTE_CAPTURE_FULL_TO_PC     (0x0002)
#define REMOTE_CAPTURE_THUMB_TO_DRIVE (0x0004)
#define REMOTE_CAPTURE_FULL_TO_DRIVE  (0x0008)



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
