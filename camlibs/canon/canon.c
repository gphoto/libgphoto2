/*
 * canon.c - Canon protocol "native" operations.
 *
 * Written 1999 by Wolfgang G. Reissnegger and Werner Almesberger
 * Additions 2000 by Philippe Marzouk and Edouard Lafargue
 * USB support, 2000, by Mikael Nyström
 *
 * $Id$
 *
 * This file includes both USB and serial support for the cameras
 * manufactured by Canon. These comprise all (or at least almost all)
 * of the digital models of the IXUS, PowerShot and EOS series.
 *
 * We are working at moving serial and USB specific stuff to serial.c
 * and usb.c, keeping the common protocols/busses support in this
 * file.
 */

#include <config.h>

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>
#ifdef OS2
#include <db.h>
#endif

#include <gphoto2.h>
#include <gphoto2-port-log.h>

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define _(String) (String)
#  define N_(String) (String)
#endif

#include "../../libgphoto2/exif.h"

#include "usb.h"
#include "util.h"
#include "library.h"
#include "canon.h"
#include "serial.h"

/************************************************************************
 * Camera definitions
 ************************************************************************/

/**
 * models:
 *
 * Contains list of all camera models currently supported with their
 * respective USB IDs and a flag denoting RS232 serial support.
 **/

#define S32K	32 * 1024
#define S1M	1024 * 1024
#define S2M	2 * S1M
#define S10M	10 * S1M

static const struct canonCamModelData models[] =
{
	/* *INDENT-OFF* */
	{"DE300 Canon Inc.",		CANON_PS_A5,		0, 0, 1, S2M, S32K},
	{"Canon PowerShot A5 Zoom",	CANON_PS_A5_ZOOM,	0, 0, 1, S2M, S32K},
	{"Canon PowerShot A50",		CANON_PS_A50,		0, 0, 1, S2M, S32K},
	{"Canon PowerShot Pro70",	CANON_PS_A70,		0, 0, 1, S2M, S32K},
	{"Canon PowerShot S10",		CANON_PS_S10,		0x04A9, 0x3041, 1, S10M, S32K},
	{"Canon PowerShot S20",		CANON_PS_S20,		0x04A9, 0x3043, 1, S10M, S32K},
	{"Canon EOS D30",		CANON_EOS_D30,		0x04A9, 0x3044, 0, S10M, S32K},
	{"Canon PowerShot S100",	CANON_PS_S100,		0x04A9, 0x3045, 0, S10M, S32K},
	{"Canon IXY DIGITAL",		CANON_PS_S100,		0x04A9, 0x3046, 0, S10M, S32K},
	{"Canon Digital IXUS",		CANON_PS_S100,		0x04A9, 0x3047, 0, S10M, S32K},
	{"Canon PowerShot G1",		CANON_PS_G1,		0x04A9, 0x3048, 1, S10M, S32K},
	{"Canon PowerShot Pro90 IS",	CANON_PS_PRO90_IS,	0x04A9, 0x3049, 1, S10M, S32K},
	{"Canon IXY DIGITAL 300",	CANON_PS_S300,		0x04A9, 0x304B, 0, S10M, S32K},
	{"Canon PowerShot S300",	CANON_PS_S300,		0x04A9, 0x304C, 0, S10M, S32K},
	{"Canon Digital IXUS 300",	CANON_PS_S300,		0x04A9, 0x304D, 0, S10M, S32K},
	{"Canon PowerShot A20",		CANON_PS_A20,		0x04A9, 0x304E, 0, S10M, S32K},
	{"Canon PowerShot A10",		CANON_PS_A10,		0x04A9, 0x304F, 0, S10M, S32K},
	{"Canon PowerShot S110",	CANON_PS_S100,		0x04A9, 0x3051, 0, S10M, S32K},
	{"Canon DIGITAL IXUS v",	CANON_PS_S100,		0x04A9, 0x3052, 0, S10M, S32K},
	{"Canon PowerShot G2",		CANON_PS_G2,		0x04A9, 0x3055, 0, S10M, S32K},
	{"Canon PowerShot S40",		CANON_PS_S40,		0x04A9, 0x3056, 0, S10M, S32K},
	{"Canon PowerShot S30",		CANON_PS_S30,		0x04A9, 0x3057, 0, S10M, S32K},
	{NULL}
	/* *INDENT-ON* */
};

#undef S10M
#undef S2M
#undef S1M
#undef S32K

/************************************************************************
 * Methods
 ************************************************************************/

/*
 * does operations on a directory based on the value
 * of action : DIR_CREATE, DIR_REMOVE
 *
 */
int
canon_int_directory_operations (Camera *camera, const char *path, int action)
{
	unsigned char *msg;
	int len, canon_usb_funct;
	char type;

	switch (action) {
		case DIR_CREATE:
			type = 0x5;
			canon_usb_funct = CANON_USB_FUNCTION_MKDIR;
			break;
		case DIR_REMOVE:
			type = 0x6;
			canon_usb_funct = CANON_USB_FUNCTION_RMDIR;
			break;
		default:
			gp_debug_printf (GP_DEBUG_LOW, "canon",
					 "canon_int_directory_operations: "
					 "Bad operation specified : %i\n", action);
			return GP_ERROR_BAD_PARAMETERS;
			break;
	}

	gp_debug_printf (GP_DEBUG_LOW, "canon", "canon_int_directory_operations() "
			 "called to %s the directory '%s'",
			 canon_usb_funct == CANON_USB_FUNCTION_MKDIR ? "create" : "remove",
			 path);
	switch (camera->port->type) {
		case GP_PORT_USB:
			msg = canon_usb_dialogue (camera, canon_usb_funct, &len, path,
						  strlen (path) + 1);
			break;
		case GP_PORT_SERIAL:
			msg = canon_serial_dialogue (camera, type, 0x11, &len, path,
						     strlen (path) + 1, NULL);
			break;
		GP_PORT_DEFAULT
	}

	if (!msg) {
		canon_serial_error_type (camera);
		return GP_ERROR;
	}

	if (msg[0] != 0x00) {
		gp_debug_printf (GP_DEBUG_LOW, "canon",
				 "Could not %s directory %s",
				 canon_usb_funct ==
				 CANON_USB_FUNCTION_MKDIR ? "create" : "remove", path);
		return GP_ERROR;
	}

	return GP_OK;
}

/**
 * canon_int_identify_camera:
 * @camera: the camera to work with
 * @Returns: gphoto2 error code
 *
 * Gets the camera identification string, usually the owner name.
 *
 * This information is then stored in the "camera" structure, which 
 * is a global variable for the driver.
 *
 * This function also gets the firmware revision in the camera struct.
 **/
int
canon_int_identify_camera (Camera *camera)
{
	unsigned char *msg;
	int len;

	GP_DEBUG ("canon_int_identify_camera() called");

	switch (camera->port->type) {
		case GP_PORT_USB:
			msg = canon_usb_dialogue (camera, CANON_USB_FUNCTION_IDENTIFY_CAMERA,
						  &len, NULL, 0);
			if (!msg)
				return GP_ERROR;
			break;
		case GP_PORT_SERIAL:
			msg = canon_serial_dialogue (camera, 0x01, 0x12, &len, NULL);
			if (!msg) {
				gp_debug_printf (GP_DEBUG_LOW, "canon",
						 "canon_int_identify_camera: " "msg error");
				canon_serial_error_type (camera);
				return GP_ERROR;
			}
			break;
		GP_PORT_DEFAULT
	}

	if (len != 0x4c)
		return GP_ERROR;

	/* Store these values in our "camera" structure: */
	memcpy (camera->pl->firmwrev, (char *) msg + 8, 4);
	strncpy (camera->pl->ident, (char *) msg + 12, 30);
	strncpy (camera->pl->owner, (char *) msg + 44, 30);

	GP_DEBUG ("canon_int_identify_camera: ident '%s' owner '%s'", camera->pl->ident,
		  camera->pl->owner);

	return GP_OK;
}

/**
 * canon_int_get_battery:
 * @camera: the camera to work on
 * @pwr_status: pointer to integer determining power status
 * @pwr_source: pointer to integer determining power source
 * @Returns: gphoto2 error code
 *
 * Gets battery status.
 **/
int
canon_int_get_battery (Camera *camera, int *pwr_status, int *pwr_source)
{
	unsigned char *msg;
	int len;

	GP_DEBUG ("canon_int_get_battery()");

	switch (camera->port->type) {
		case GP_PORT_USB:
			msg = canon_usb_dialogue (camera, CANON_USB_FUNCTION_POWER_STATUS,
						  &len, NULL, 0);
			if (!msg)
				return GP_ERROR;
			break;
		case GP_PORT_SERIAL:
			msg = canon_serial_dialogue (camera, 0x0a, 0x12, &len, NULL);
			if (!msg) {
				canon_serial_error_type (camera);
				return GP_ERROR;
			}

			break;
		GP_PORT_DEFAULT
	}

	if (len != 8)
		return GP_ERROR;

	if (pwr_status)
		*pwr_status = msg[4];
	if (pwr_source)
		*pwr_source = msg[7];
	GP_DEBUG ("canon_int_get_battery: Status: %i / Source: %i\n", *pwr_status,
		  *pwr_source);
	return GP_OK;
}


/**
 * canon_int_set_file_attributes:
 * @camera: camera to work with
 * @file: file to work on
 * @dir: directory to work in
 * @attrs: attribute bit field
 * @Returns: gphoto2 error code
 *
 * Sets a file's attributes. See the 'Protocol' file for details.
 **/
int
canon_int_set_file_attributes (Camera *camera, const char *file, const char *dir,
			       unsigned char attrs)
{
	unsigned char payload[300];
	unsigned char *msg;
	unsigned char attr[4];
	int len, payload_length;

	GP_DEBUG ("canon_int_set_file_attributes() "
		  "called for '%s' '%s', attributes 0x%x", dir, file, attrs);

	attr[0] = attr[1] = attr[2] = 0;
	attr[3] = attrs;

	switch (camera->port->type) {
		case GP_PORT_USB:
			if ((4 + strlen (dir) + 1 + strlen (file) + 1) > sizeof (payload)) {
				GP_DEBUG ("canon_int_set_file_attributes: "
					  "dir '%s' + file '%s' too long, "
					  "won't fit in payload buffer.", dir, file);
				return GP_ERROR_BAD_PARAMETERS;
			}
			/* create payload (yes, path and filename are two different strings
			 * and not meant to be concatenated)
			 */
			memset (payload, 0, sizeof (payload));
			memcpy (payload, attr, 4);
			memcpy (payload + 4, dir, strlen (dir) + 1);
			memcpy (payload + 4 + strlen (dir) + 1, file, strlen (file) + 1);
			payload_length = 4 + strlen (dir) + 1 + strlen (file) + 1;
			msg = canon_usb_dialogue (camera, CANON_USB_FUNCTION_SET_ATTR, &len,
						  payload, payload_length);
			if (len == 4) {
				/* XXX check camera return value (not canon_usb_dialogue return value
				 * but the bytes in the packet returned)
				 */
				gp_debug_printf (GP_DEBUG_LOW, "canon",
						 "canon_int_set_file_attributes: "
						 "returned four bytes as expected, "
						 "we should check if they indicate "
						 "error or not. Returned data :");
				gp_log_data ("canon", msg, 4);
			} else {
				gp_debug_printf (GP_DEBUG_LOW, "canon",
						 "canon_int_set_file_attributes: "
						 "setting attribute failed!");
				return GP_ERROR;
			}

			break;
		case GP_PORT_SERIAL:
			msg = canon_serial_dialogue (camera, 0xe, 0x11, &len, attr, 4, dir,
						     strlen (dir) + 1, file, strlen (file) + 1,
						     NULL);
			break;
		GP_PORT_DEFAULT
	}

	if (!msg) {
		canon_serial_error_type (camera);
		return GP_ERROR;
	}

	return GP_OK;
}

/**
 * canon_int_set_owner_name:
 * @camera: the camera to set the owner name of
 * @name: owner name to set the camera to
 *
 * Sets the camera owner name. The string should not be more than 30
 * characters long. We call #get_owner_name afterwards in order to
 * check that everything went fine.
 **/
int
canon_int_set_owner_name (Camera *camera, const char *name)
{
	unsigned char *msg;
	int len;

	gp_debug_printf (GP_DEBUG_LOW, "canon", "canon_int_set_owner_name() "
			 "called, name = '%s'", name);
	if (strlen (name) > 30) {
		gp_debug_printf (GP_DEBUG_LOW, "canon",
				 "canon_int_set_owner_name: Name too long (%i chars), "
				 "max is 30 characters!", strlen (name));
		gp_camera_status (camera, _("Name too long, max is 30 characters!"));
		return 0;
	}

	switch (camera->port->type) {
		case GP_PORT_USB:
			msg = canon_usb_dialogue (camera, CANON_USB_FUNCTION_CAMERA_CHOWN,
						  &len, name, strlen (name) + 1);
			break;
		case GP_PORT_SERIAL:
			msg = canon_serial_dialogue (camera, 0x05, 0x12, &len, name,
						     strlen (name) + 1, NULL);
			break;
		GP_PORT_DEFAULT
	}

	if (!msg) {
		canon_serial_error_type (camera);
		return GP_ERROR;
	}
	return canon_int_identify_camera (camera);
}


/**
 * canon_int_get_time:
 * @camera: camera to get the current time of
 * @Returns: time of camera (local time)
 *
 * Get camera's current time.
 *
 * The camera gives time in little endian format, therefore we need
 * to swap the 4 bytes on big-endian machines.
 *
 * Nota: the time returned is not GMT but local time. Therefore,
 * if you use functions like "ctime", it will be translated to local
 * time _a second time_, and the result will be wrong. Only use functions
 * that don't translate the date into localtime, like "gmtime".
 **/
time_t
canon_int_get_time (Camera *camera)
{
	unsigned char *msg;
	int len;
	time_t date;

	GP_DEBUG ("canon_int_get_time()");

	switch (camera->port->type) {
		case GP_PORT_USB:
			msg = canon_usb_dialogue (camera, CANON_USB_FUNCTION_GET_TIME, &len,
						  NULL, 0);
			if (!msg)
				return GP_ERROR;
			break;
		case GP_PORT_SERIAL:
			msg = canon_serial_dialogue (camera, 0x03, 0x12, &len, NULL);
			if (!msg) {
				canon_serial_error_type (camera);
				return GP_ERROR;
			}
			break;
		GP_PORT_DEFAULT
	}

	if (len != 0x10)
		return GP_ERROR;

	date = (time_t) le32atoh (msg+4);

	/* XXX should strip \n at the end of asctime() return data */
	GP_DEBUG ("Camera time: %s ", asctime (gmtime (&date)));
	return date;
}


int
canon_int_set_time (Camera *camera)
{
	unsigned char *msg;
	int len, i;
	time_t date;
	char pcdate[4];

	date = time (NULL);
	for (i = 0; i < 4; i++)
		pcdate[i] = (date >> (8 * i)) & 0xff;

	switch (camera->port->type) {
		case GP_PORT_USB:
			msg = canon_usb_dialogue (camera, CANON_USB_FUNCTION_SET_TIME, &len,
						  NULL, 0);
			if (!msg)
				return GP_ERROR;
			break;
		case GP_PORT_SERIAL:
			msg = canon_serial_dialogue (camera, 0x04, 0x12, &len, pcdate,
						     sizeof (pcdate),
						     "\x00\x00\x00\x00\x00\x00\x00\x00", 8,
						     NULL);
			if (!msg) {
				canon_serial_error_type (camera);
				return GP_ERROR;
			}

			break;
		GP_PORT_DEFAULT
	}

	if (len != 0x10)
		return GP_ERROR;

	return GP_OK;
}

/**
 * canon_int_ready:
 * @camera: camera to get ready
 * @Returns: gphoto2 error code
 *
 * Switches the camera on, detects the model and sets its speed.
 **/
int
canon_int_ready (Camera *camera)
{
	int res;

	GP_DEBUG ("canon_int_ready()");

	switch (camera->port->type) {
		case GP_PORT_USB:
			res = canon_usb_ready (camera);
			break;
		case GP_PORT_SERIAL:
			res = canon_serial_ready (camera);
			break;
		GP_PORT_DEFAULT
	}

	return (res);
}

/**
 * canon_int_get_disk_name:
 * @camera: camera to ask for disk drive
 * @Returns: name of disk
 *
 * Ask the camera for the name of the flash storage
 * device. Usually "D:" or something like that.
 **/
char *
canon_int_get_disk_name (Camera *camera)
{
	unsigned char *msg;
	int len, res;

	GP_DEBUG ("canon_int_get_disk_name()");

	switch (camera->port->type) {
		case GP_PORT_USB:
			res = canon_usb_long_dialogue (camera,
						       CANON_USB_FUNCTION_FLASH_DEVICE_IDENT,
						       &msg, &len, 1024, NULL, 0, 0);
			if (res != GP_OK) {
				GP_DEBUG ("canon_int_get_disk_name: canon_usb_long_dialogue "
					  "failed! returned %i", res);
				return NULL;
			}
			break;
		case GP_PORT_SERIAL:
			msg = canon_serial_dialogue (camera, 0x0a, 0x11, &len, NULL);
			if (!msg) {
				canon_serial_error_type (camera);
				return NULL;
			}
			break;
		GP_PORT_DEFAULT
	}

	if (camera->port->type == GP_PORT_SERIAL) {
		/* this is correct even though it looks a bit funny. canon_serial_dialogue()
		 * has a static buffer, strdup() part of that buffer and return to our caller.
		 */
		msg = strdup ((char *) msg + 4);	/* @@@ should check length */
		if (!msg) {
			GP_DEBUG ("canon_int_get_disk_name: could not allocate %i "
				  "bytes of memory to hold response",
				  strlen ((char *) msg + 4));
			return NULL;
		}
	}

	GP_DEBUG ("canon_int_get_disk_name: disk '%s'", msg);

	return msg;
}

/**
 * canon_int_get_disk_name_info:
 * @camera: camera to ask about disk
 * @name: name of the disk
 * @capacity: returned maximum disk capacity
 * @available: returned currently available disk capacity
 * @Returns: boolean value denoting success (FIXME: ATTENTION!)
 *
 * Gets available room and max capacity of a disk given by @name.
 **/
int
canon_int_get_disk_name_info (Camera *camera, const char *name, int *capacity, int *available)
{
	unsigned char *msg = NULL;
	int len, cap, ava;

	GP_DEBUG ("canon_int_get_disk_name_info() name '%s'", name);

	CHECK_PARAM_NULL(name);
	CHECK_PARAM_NULL(capacity);
	CHECK_PARAM_NULL(available);

	switch (camera->port->type) {
		case GP_PORT_USB:
			msg = canon_usb_dialogue (camera, CANON_USB_FUNCTION_DISK_INFO, &len,
						  name, strlen (name) + 1);
			if (!msg)
				return GP_ERROR;
			break;
		case GP_PORT_SERIAL:
			msg = canon_serial_dialogue (camera, 0x09, 0x11, &len, name,
						     strlen (name) + 1, NULL);
			if (!msg) {
				canon_serial_error_type (camera);
				return GP_ERROR;
			}
			break;
		GP_PORT_DEFAULT
	}

	if (len < 12) {
		GP_DEBUG ("ERROR: truncated message");
		return GP_ERROR;
	}
	cap = le32atoh (msg + 4);
	ava = le32atoh (msg + 8);
	if (capacity)
		*capacity = cap;
	if (available)
		*available = ava;

	GP_DEBUG ("canon_int_get_disk_name_info: capacity %i kb, available %i kb",
		  cap > 0 ? (cap / 1024) : 0, ava > 0 ? (ava / 1024) : 0);

	return GP_OK;
}


/**
 * gphoto2canonpath:
 * @path: gphoto2 path 
 *
 * convert gphoto2 path  (e.g.   "/DCIM/116CANON/IMG_1240.JPG")
 * into canon style path (e.g. "D:\DCIM\116CANON\IMG_1240.JPG")
 */
char *
gphoto2canonpath (Camera *camera, char *path)
{
	static char tmp[2000];
	char *p;

	if (path[0] != '/') {
		GP_DEBUG ("Non-absolute gphoto2 path cannot be converted");
		return NULL;
	}

	if (camera->pl->cached_drive == NULL)
		GP_DEBUG ("NULL camera->pl->cached_drive in gphoto2canonpath");

	strcpy (tmp, camera->pl->cached_drive);
	strcat (tmp, path);

	/* replace all slashes by backslashes */
	for (p = tmp; *p != '\0'; p++) {
		if (*p == '/')
			*p = '\\';
	}

	/* remove trailing backslash */
	if ((p > tmp) && (*(p-1) == '\\'))
		*(p-1) = '\0';

	GP_LOG (GP_LOG_DATA, "gphoto2canonpath: converted '%s' to '%s'", path, tmp);

	return (tmp);
}

/**
 * cancon2gphotopath:
 * @path: canon style path
 *
 * convert canon style path (e.g. "D:\DCIM\116CANON\IMG_1240.JPG")
 * into gphoto2 path        (e.g.   "/DCIM/116CANON/IMG_1240.JPG")
 *
 * Assumes @path to start with drive name followed by a colon.
 * It just drops the drive name.
 */
const char *
canon2gphotopath (Camera *camera, const char *path)
{
	static char tmp[2000];
	char *p;

	if (!((path[1] == ':') && (path[2] == '\\'))) {
		return NULL;
	}

	p = strchr(path, ':') + 1;
	strcpy (tmp, p);

	/* replace backslashes by slashes */
	for (p = tmp; *p != '\0'; p++) {
		if (*p == '\\')
			*p = '/';
	}

	GP_LOG (GP_LOG_DATA, "canon2gphotopath: converted '%s' to '%s'", path, tmp);

	return (tmp);
}



void
canon_int_free_dir (Camera *camera, struct canon_dir *list)
{
	struct canon_dir *walk;

	for (walk = list; walk->name; walk++)
		free ((char *) walk->name);
	free (list);
}

/**
 * Get the directory tree of a given flash device.
 */
int
canon_int_list_directory (Camera *camera, struct canon_dir **result_dir, const char *path)
{
	struct canon_dir *dir = NULL;
	int entrys = 0, res, is_dir = 0;
	unsigned int dirents_length;
	char filedate_str[32];
	unsigned char *dirent_data = NULL, *end_of_data, *temp_ch, *dirent_name, *pos;

	gp_log (GP_LOG_DEBUG, "canon", "canon_list_directory() path '%s'", path);

	/* set return value to NULL in case something fails */
	*result_dir = NULL;

	/* Fetch all directory entrys from the camera */
	switch (camera->port->type) {
		case GP_PORT_USB:
			res = canon_usb_get_dirents (camera, &dirent_data, &dirents_length,
						     path);
			break;
		case GP_PORT_SERIAL:
			res = canon_serial_get_dirents (camera, &dirent_data, &dirents_length,
							path);
			break;
		GP_PORT_DEFAULT
	}
	if (res != GP_OK)
		return res;

	end_of_data = dirent_data + dirents_length;

	if (dirents_length < CANON_MINIMUM_DIRENT_SIZE) {
		gp_camera_set_error (camera, "canon_list_directory: ERROR: "
				     "initial message too short (%i < minimum %i)",
				     dirents_length, CANON_MINIMUM_DIRENT_SIZE);
		free (dirent_data);
		return GP_ERROR;
	}

	/* The first data we have got here is the dirent for the
	 * directory we are reading. Skip over 10 bytes
	 * (2 for attributes, 4 date and 4 size) and then go find
	 * the end of the directory name so that we get to the next
	 * dirent which is actually the first one we are interested
	 * in
	 */
	dirent_name = dirent_data + 10;

	GP_DEBUG ("canon_list_directory: "
		  "Camera directory listing for directory '%s'", dirent_name);

	for (pos = dirent_name; pos < end_of_data && *pos != 0; pos++) ;
	if (pos == end_of_data || *pos != 0) {
		gp_camera_set_error (camera, "canon_list_directory: "
				     "Reached end of packet while "
				     "examining the first dirent");
		free (dirent_data);
		return GP_ERROR;
	}
	pos++;			/* skip NULL byte terminating directory name */

	/* we are now positioned at the first interesting dirent */

	/* This is the main loop, for every directory entry returned */
	while (pos < end_of_data) {
		/* don't use GP_DEBUG since we log this with GP_LOG_DATA */
		gp_log (GP_LOG_DATA, "canon", "canon_list_directory: "
			"reading dirent at position %i of %i", (pos - dirent_data),
			(end_of_data - dirent_data));

		if (pos + CANON_MINIMUM_DIRENT_SIZE > end_of_data) {
			if (camera->port->type == GP_PORT_SERIAL) {
				/* check to see if it is only NULL bytes left,
				 * that is not an error for serial cameras
				 * (at least the A50 adds five zero bytes at the end)
				 */
				for (temp_ch = pos; temp_ch < end_of_data && *temp_ch;
				     temp_ch++) ;

				if (temp_ch == end_of_data) {
					GP_DEBUG ("canon_list_directory: "
						  "the last %i bytes were all 0 - ignoring.",
						  temp_ch - pos);
					break;
				} else {
					GP_DEBUG ("canon_list_directory: "
						  "byte[%i=0x%x] == %i=0x%x", temp_ch - pos,
						  temp_ch - pos, *temp_ch, *temp_ch);
					GP_DEBUG ("canon_list_directory: "
						  "pos is 0x%x, end_of_data is 0x%x, temp_ch is 0x%x - diff is 0x%x",
						  pos, end_of_data, temp_ch, temp_ch - pos);
				}
			}
			GP_DEBUG ("canon_list_directory: "
				  "dirent at position %i=0x%x of %i=0x%x is too small, "
				  "minimum dirent is %i bytes",
				  (pos - dirent_data), (pos - dirent_data),
				  (end_of_data - dirent_data), (end_of_data - dirent_data),
				  CANON_MINIMUM_DIRENT_SIZE);
			gp_camera_set_error (camera,
					     "canon_list_directory: "
					     "truncated directory entry encountered");
			free (dirent_data);
			return GP_ERROR;
		}

		/* Check end of this dirent, 10 is to skip over
		 * 2    attributes + 0x00
		 * 4    file date (UNIX localtime)
		 * 4    file size
		 * to where the direntry name begins.
		 */
		dirent_name = pos + 10;
		for (temp_ch = dirent_name; temp_ch < end_of_data && *temp_ch != 0;
		     temp_ch++) ;

		if (temp_ch == end_of_data || *temp_ch != 0) {
			GP_DEBUG ("canon_list_directory: "
				  "dirent at position %i of %i has invalid name in it."
				  "bailing out with what we've got.",
				  (pos - dirent_data), (end_of_data - dirent_data));
			break;
		}

		/* check that length of name in this dirent is not of unreasonable size.
		 * 256 was picked out of the blue
		 */
		if (strlen (dirent_name) > 256) {
			GP_DEBUG ("canon_list_directory: "
				  "dirent at position %i of %i has too long name in it (%i bytes)."
				  "bailing out with what we've got.",
				  (pos - dirent_data), (end_of_data - dirent_data),
				  strlen (pos + 10));
			break;
		}

		/* 10 bytes of attributes, size and date, a name and a NULL terminating byte */
		/* don't use GP_DEBUG since we log this with GP_LOG_DATA */
		gp_log (GP_LOG_DATA, "canon", "canon_list_directory: "
			"dirent determined to be %i=0x%x bytes :",
			10 + strlen (dirent_name) + 1, 10 + strlen (dirent_name) + 1);
		gp_log_data ("canon", pos, 10 + strlen (dirent_name) + 1);

		if (strlen (dirent_name)) {
			/* OK, this directory entry has a name in it. */

			temp_ch = realloc (dir, sizeof (struct canon_dir) * (entrys + 1));
			if (temp_ch == NULL) {
				gp_camera_set_error (camera, "canon_list_directory: "
						     "Could not resize canon_dir buffer to %i bytes",
						     sizeof (*dir) * (entrys + 1));
				if (dir)
					canon_int_free_dir (camera, dir);

				free (dirent_data);
				return GP_ERROR_NO_MEMORY;
			}
			dir = (struct canon_dir *) temp_ch;

			dir[entrys].name = strdup (dirent_name);
			if (!dir[entrys].name) {
				gp_camera_set_error (camera, "canon_list_directory: "
						     "Could not duplicate string of %i bytes",
						     strlen (dirent_name));
				if (dir)
					canon_int_free_dir (camera, dir);

				free (dirent_data);
				return GP_ERROR_NO_MEMORY;
			}

			dir[entrys].attrs = *pos;
			/* is_dir is set to the 'real' value, used when printing the
			 * debug output later on.
			 */
			is_dir = ((dir[entrys].attrs & CANON_ATTR_NON_RECURS_ENT_DIR) != 0x0)
				|| ((dir[entrys].attrs & CANON_ATTR_RECURS_ENT_DIR) != 0x0);
			/* dir[entrys].is_file is really 'is_not_recursively_entered_directory' */
			dir[entrys].is_file =
				!((dir[entrys].attrs & CANON_ATTR_NON_RECURS_ENT_DIR) != 0x0);

			/* the size is located at offset 2 and is 4 bytes long */
			dir[entrys].size = le32atoh (pos+2);

			/* the date is located at offset 6 and is 4 bytes long */
			dir[entrys].date = le32atoh (pos+6);

			/* if there is a date, make filedate_str be the ascii representation
			 * without newline at the end
			 */
			if (dir[entrys].date != 0) {
				snprintf (filedate_str, sizeof (filedate_str), "%s",
					  asctime (gmtime (&dir[entrys].date)));
				if (filedate_str[strlen (filedate_str) - 1] == '\n')
					filedate_str[strlen (filedate_str) - 1] = 0;
			} else {
				strcpy (filedate_str, "           -");
			}

			/* This produces ls(1) like output, one line per file :
			 * XXX ADD EXAMPLE HERE
			 */
			/* *INDENT-OFF* */
			GP_DEBUG ("dirent: %c%s  %-5s  (attrs:0x%02x%s%s%s%s)   %10i %-24s %s\n",
				(is_dir) ? 'd' : '-',
				(dir[entrys].attrs & CANON_ATTR_WRITE_PROTECTED) == 0x0 ? "rw" : "r-",
				(dir[entrys].attrs & CANON_ATTR_DOWNLOADED) == 0x0 ? "saved" : "new",
				(unsigned char) dir[entrys].attrs,
				(dir[entrys].attrs & CANON_ATTR_UNKNOWN_2) == 0x0 ? "" : " u2",
				(dir[entrys].attrs & CANON_ATTR_UNKNOWN_4) == 0x0 ? "" : " u4",
				(dir[entrys].attrs & CANON_ATTR_UNKNOWN_8) == 0x0 ? "" : " u8",
				(dir[entrys].attrs & CANON_ATTR_UNKNOWN_40) == 0x0 ? "" : " u40", dir[entrys].size,
				filedate_str,
				dir[entrys].name);
			/* *INDENT-ON* */

			entrys++;
		} else {
			GP_DEBUG ("canon_list_directory: "
				  "dirent at position %i of %i has NULL name, skipping.",
				  (pos - dirent_data), (end_of_data - dirent_data));
		}

		/* make 'p' point to next dirent in packet.
		 * first we skip 10 bytes of attribute, size and date,
		 * then we skip the name plus 1 for the NULL
		 * termination bytes.
		 */
		pos += 10 + strlen (dirent_name) + 1;
	}
	free (dirent_data);

	if (dir) {
		/* allocate one more dirent */
		temp_ch = realloc (dir, sizeof (struct canon_dir) * (entrys + 1));
		if (temp_ch == NULL) {
			gp_camera_set_error (camera, "canon_list_directory: "
					     "could not realloc() %i bytes of memory",
					     sizeof (*dir) * (entrys + 1));
			if (dir)
				canon_int_free_dir (camera, dir);

			return GP_ERROR_NO_MEMORY;
		}

		dir = (struct canon_dir *) temp_ch;
		/* show that this is the last record by setting the name to NULL */
		dir[entrys].name = NULL;

		gp_log (GP_LOG_DEBUG, "canon", "canon_list_directory: "
			"Returning %i directory entrys", entrys);

		*result_dir = dir;
	}

	return GP_OK;
}

int
canon_int_get_file (Camera *camera, const char *name, unsigned char **data, int *length)
{
	switch (camera->port->type) {
		case GP_PORT_USB:
			return canon_usb_get_file (camera, name, data, length);
			break;
		case GP_PORT_SERIAL:
			*data = canon_serial_get_file (camera, name, length);
			if (*data)
				return GP_OK;
			return GP_ERROR;
			break;
		GP_PORT_DEFAULT
	}
}

/**
 * canon_int_get_thumbnail:
 * @camera: camera to work with
 * @name: image to get thumbnail of
 * @length: length of data returned
 *
 * Returns the thumbnail data of the picture designated by @name.
 **/
unsigned char *
canon_int_get_thumbnail (Camera *camera, const char *name, int *length)
{
	unsigned char *data = NULL;
	unsigned char *msg;
	exifparser exifdat;
	unsigned int total = 0, expect = 0, size, payload_length, total_file_size;
	int i, j, in;
	unsigned char *thumb;

	GP_DEBUG ("canon_int_get_thumbnail() called for file '%s'", name);

	gp_camera_progress (camera, 0);
	switch (camera->port->type) {
		case GP_PORT_USB:
			i = canon_usb_get_thumbnail (camera, name, &data, length);
			if (i != GP_OK) {
				GP_DEBUG ("canon_usb_get_thumbnail() failed, "
					  "returned %i", i);
				return NULL;	// XXX for now
			}
			break;
		case GP_PORT_SERIAL:
			if (camera->pl->receive_error == FATAL_ERROR) {
				GP_DEBUG ("ERROR: can't continue a fatal "
					  "error condition detected");
				return NULL;
			}

			payload_length = strlen (name) + 1;
			msg = canon_serial_dialogue (camera, 0x1, 0x11, &total_file_size,
						     "\x01\x00\x00\x00\x00", 5,
						     &payload_length, 1, "\x00", 2,
						     name, strlen (name) + 1, NULL);
			if (!msg) {
				canon_serial_error_type (camera);
				return NULL;
			}

			total = le32atoh (msg + 4);
			if (total > 2000000) {	/* 2 MB thumbnails ? unlikely ... */
				GP_DEBUG ("ERROR: %d is too big", total);
				return NULL;
			}
			data = malloc (total);
			if (!data) {
				perror ("malloc");
				return NULL;
			}
			if (length)
				*length = total;

			while (msg) {
				if (total_file_size < 20 || le32atoh (msg)) {
					return NULL;
				}
				size = le32atoh (msg + 12);
				if (le32atoh (msg + 8) != expect || expect + size > total
				    || size > total_file_size - 20) {
					GP_DEBUG ("ERROR: doesn't fit");
					return NULL;
				}
				memcpy (data + expect, msg + 20, size);
				expect += size;
				gp_camera_progress (camera,
						    total ? (expect / (float) total) : 1.);
				if ((expect == total) != le32atoh (msg + 16)) {
					GP_DEBUG ("ERROR: end mark != end of data");
					return NULL;
				}
				if (expect == total) {
					/* We finished receiving the file. Parse the header and
					   return just the thumbnail */
					break;
				}
				msg = canon_serial_recv_msg (camera, 0x1, 0x21,
							     &total_file_size);
			}
			break;
		GP_PORT_DEFAULT
	}

	switch (camera->pl->md->model) {
		case CANON_PS_A70:	/* pictures are JFIF files */
			/* we skip the first FF D8 */
			i = 3;
			j = 0;
			in = 0;

			/* we want to drop the header to get the thumbnail */

			thumb = malloc (total);
			if (!thumb) {
				perror ("malloc");
				break;
			}

			while (i < total) {
				if (data[i] == JPEG_ESC) {
					if (data[i + 1] == JPEG_BEG &&
					    ((data[i + 3] == JPEG_SOS)
					     || (data[i + 3] == JPEG_A50_SOS))) {
						in = 1;
					} else if (data[i + 1] == JPEG_END) {
						in = 0;
						thumb[j++] = data[i];
						thumb[j] = data[i + 1];
						return thumb;
					}
				}

				if (in == 1)
					thumb[j++] = data[i];
				i++;

			}
			return NULL;
			break;

		default:	/* Camera supports EXIF */
			exifdat.header = data;
			exifdat.data = data + 12;

			GP_DEBUG ("Got thumbnail, extracting it with the EXIF lib.");
			if (exif_parse_data (&exifdat) > 0) {
				data = exif_get_thumbnail (&exifdat);	// Extract Thumbnail
				if (data == NULL) {
					int f;
					char fn[255];

					if (rindex (name, '\\') != NULL)
						snprintf (fn, sizeof (fn),
							  "canon-death-dump.dat-%s",
							  rindex (name, '\\') + 1);
					else
						snprintf (fn, sizeof (fn),
							  "canon-death-dump.dat-%s", name);

					GP_DEBUG ("canon_int_get_thumbnail: "
						  "Thumbnail conversion error, saving "
						  "%i bytes to '%s'", total, fn);
					/* create with O_EXCL and 0600 for security */
					if ((f =
					     open (fn, O_CREAT | O_EXCL | O_RDWR,
						   0600)) == -1) {
						GP_DEBUG ("canon_int_get_thumbnail: "
							  "error creating '%s': %m", fn);
						break;
					}
					if (write (f, data, total) == -1) {
						GP_DEBUG ("canon_int_get_thumbnail: "
							  "error writing to file '%s': %m",
							  fn);
					}

					close (f);
					break;
				}

				GP_DEBUG ("Parsed EXIF data.");
				return data;
			}
			break;
	}

	free (data);
	return NULL;
}

int
canon_int_delete_file (Camera *camera, const char *name, const char *dir)
{
	unsigned char payload[300];
	unsigned char *msg;
	int len, payload_length;

	switch (camera->port->type) {
		case GP_PORT_USB:
			memcpy (payload, dir, strlen (dir) + 1);
			memcpy (payload + strlen (dir) + 1, name, strlen (name) + 1);
			payload_length = strlen (dir) + strlen (name) + 2;
			msg = canon_usb_dialogue (camera, CANON_USB_FUNCTION_DELETE_FILE, &len,
						  payload, payload_length);
			if (!msg)
				return GP_ERROR;

			break;
		case GP_PORT_SERIAL:
			msg = canon_serial_dialogue (camera, 0xd, 0x11, &len, dir,
						     strlen (dir) + 1, name, strlen (name) + 1,
						     NULL);
			if (!msg) {
				canon_serial_error_type (camera);
				return GP_ERROR;
			}
			break;
		GP_PORT_DEFAULT
	}

	if (len != 4) {
		/* XXX should mark folder as dirty since we can't be sure if the file
		 * got deleted or not
		 */
		return GP_ERROR;
	}

	if (msg[0] == 0x29) {
		gp_camera_message (camera, _("File protected"));
		return GP_ERROR;
	}

	/* XXX we should mark folder as dirty, re-read it and check if the file
	 * is gone or not.
	 */
	return GP_OK;
}

/*
 * Upload a file to the camera
 *
 */
int
canon_int_put_file (Camera *camera, CameraFile *file, char *destname, char *destpath)
{

	switch (camera->port->type) {
		case GP_PORT_USB:
			return canon_usb_put_file (camera, file, destname, destpath);
			break;
		case GP_PORT_SERIAL:
			return canon_serial_put_file (camera, file, destname, destpath);
			break;
		GP_PORT_DEFAULT
	}
}

/*
 * Local Variables:
 * c-file-style:"linux"
 * indent-tabs-mode:t
 * End:
 */
