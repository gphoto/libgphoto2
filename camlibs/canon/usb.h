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

int canon_usb_init (Camera *camera);
int canon_usb_camera_init (Camera *camera);
int canon_usb_put_file (Camera *camera, CameraFile *file, char *destname, char *destpath);
unsigned char *canon_usb_dialogue (Camera *camera, int canon_funct, int *return_length, 
		const char *payload, int payload_length);
int canon_usb_long_dialogue (Camera *camera, int canon_funct, unsigned char **data, 
		int *data_length, int max_data_size, const char *payload,
		int payload_length, int display_status);
int canon_usb_get_file (Camera *camera, const char *name, unsigned char **data, int *length);
int canon_usb_get_thumbnail (Camera *camera, const char *name, unsigned char **data, int *length);
int canon_usb_lock_keys(Camera *camera);
int canon_usb_unlock_keys(Camera *camera);
int canon_usb_get_dirents (Camera *camera, unsigned char **dirent_data, unsigned int *dirents_length, const char *path);
int canon_usb_ready (Camera *camera);
int canon_usb_identify (Camera *camera);

#define USB_BULK_READ_SIZE 0x3000

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
	{CANON_USB_FUNCTION_GET_DIRENT,		"Get directory entrys",		0x0b, 0x11, 0x202,	0x40},
	{CANON_USB_FUNCTION_DELETE_FILE,	"Delete file",			0x0d, 0x11, 0x201,	0x54},
	{CANON_USB_FUNCTION_SET_ATTR,		"Set file attribute",		0x0e, 0x11, 0x201,	0x54},
	{CANON_USB_FUNCTION_EOS_LOCK_KEYS,	"EOS lock keys",		0x1b, 0x12, 0x201,	0x54},
	{CANON_USB_FUNCTION_EOS_UNLOCK_KEYS,	"EOS unlock keys",		0x1c, 0x12, 0x201,	0x54},
	{CANON_USB_FUNCTION_GET_PIC_ABILITIES,	"Get picture abilities",	0x1f, 0x12, 0x201,	0x384},
	{CANON_USB_FUNCTION_GENERIC_LOCK_KEYS,	"Lock keys and turn off LCD",	0x20, 0x12, 0x201,	0x54},
	{ 0 }
};


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
