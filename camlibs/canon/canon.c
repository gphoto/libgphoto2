/*
 * canon.c - Canon protocol "native" operations.
 *
 * Written 1999 by Wolfgang G. Reissnegger and Werner Almesberger
 * Additions 2000 by Philippe Marzouk and Edouard Lafargue
 * USB support, 2000, by Mikael Nyström
 *
 * This file includes both USB and serial support for the cameras
 * manufactured by Canon. These comprise all (or at least almost all)
 * of the digital models of the IXUS, PowerShot and EOS series.
 *
 * We are working at moving serial and USB specific stuff to serial.c
 * and usb.c, keeping the common protocols/busses support in this
 * file.
 *
 * $Id$
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#ifdef TM_IN_SYS_TIME
#include <sys/time.h>
#else
#include <time.h>
#endif

#ifdef OS2
#include <db.h>
#endif

#include <gphoto2.h>
#include <gphoto2-port-log.h>

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (GETTEXT_PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define _(String) (String)
#  define N_(String) (String)
#endif

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
 *
 * Some cameras are sold under different names in different regions,
 * but are technically the same. We treat them the same from a
 * technical point of view. To avoid unnecessary questions from users,
 * we add the other names to the camera list after the primary name,
 * such that their camera name occurs in the list of supported
 * cameras.
 *
 * Note that at least some serial cameras require a certain name for
 * correct detection.
 **/

#define S32K	(32 * 1024)
#define S1M	(1024 * 1024)
#define S2M	(2 * S1M)
#define S10M	(10 * S1M)
#define S99M	(99 * S1M)
#define NO_USB  0

const struct canonCamModelData models[] = {
	/* *INDENT-OFF* */
	{"DE300 Canon Inc.",		CANON_PS_A5,		NO_USB, NO_USB, 1,  S2M, S32K},
	{"Canon:DE300",		        CANON_PS_A5,		NO_USB, NO_USB, 1,  S2M, S32K},
	{"Canon:PowerShot A5 Zoom",	CANON_PS_A5_ZOOM,	NO_USB, NO_USB, 1,  S2M, S32K},
	{"Canon:PowerShot A50",		CANON_PS_A50,		NO_USB, NO_USB, 1,  S2M, S32K},
	{"Canon:PowerShot Pro70",	CANON_PS_A70,		NO_USB, NO_USB, 1,  S2M, S32K},
	{"Canon:PowerShot S10",		CANON_PS_S10,		0x04A9, 0x3041, 1, S10M, S32K},
	{"Canon:PowerShot S20",		CANON_PS_S20,		0x04A9, 0x3043, 1, S10M, S32K},
	{"Canon:EOS D30",		CANON_EOS_D30,		0x04A9, 0x3044, 0, S10M, S32K},
	{"Canon:PowerShot S100",	CANON_PS_S100,		0x04A9, 0x3045, 0, S10M, S32K},
	{"Canon:IXY DIGITAL",		CANON_PS_S100,		0x04A9, 0x3046, 0, S10M, S32K},
	{"Canon:Digital IXUS",		CANON_PS_S100,		0x04A9, 0x3047, 0, S10M, S32K},
	{"Canon:PowerShot G1",		CANON_PS_G1,		0x04A9, 0x3048, 1, S10M, S32K},
	{"Canon:PowerShot Pro90 IS",	CANON_PS_PRO90_IS,	0x04A9, 0x3049, 1, S10M, S32K},
	{"Canon:IXY DIGITAL 300",	CANON_PS_S300,		0x04A9, 0x304B, 0, S10M, S32K},
	{"Canon:PowerShot S300",	CANON_PS_S300,		0x04A9, 0x304C, 0, S10M, S32K},
	{"Canon:Digital IXUS 300",	CANON_PS_S300,		0x04A9, 0x304D, 0, S10M, S32K},
	{"Canon:PowerShot A20",		CANON_PS_A20,		0x04A9, 0x304E, 0, S10M, S32K},
	{"Canon:PowerShot A10",		CANON_PS_A10,		0x04A9, 0x304F, 0, S10M, S32K},
	{"Canon:PowerShot S110",	CANON_PS_S100,		0x04A9, 0x3051, 0, S10M, S32K},
	{"Canon:DIGITAL IXUS v",	CANON_PS_S100,		0x04A9, 0x3052, 0, S10M, S32K},
	{"Canon:PowerShot G2",		CANON_PS_G2,		0x04A9, 0x3055, 0, S10M, S32K},
	{"Canon:PowerShot S40",		CANON_PS_S40,		0x04A9, 0x3056, 0, S10M, S32K},
	{"Canon:PowerShot S30",		CANON_PS_S30,		0x04A9, 0x3057, 0, S10M, S32K},
	{"Canon:PowerShot A40",		CANON_PS_A40,		0x04A9, 0x3058, 0, S10M, S32K},
	{"Canon:PowerShot A30",		CANON_PS_A30,		0x04A9,	0x3059,	0, S10M, S32K},
	{"Canon:EOS D60",		CANON_EOS_D60,		0x04A9, 0x3060, 0, S10M, S32K},
	{"Canon:PowerShot A100",	CANON_PS_A100,		0x04A9, 0x3061, 0, S10M, S32K},
	{"Canon:PowerShot A200",	CANON_PS_A200,		0x04A9, 0x3062, 0, S10M, S32K},
	{"Canon:PowerShot S200",	CANON_PS_S200,		0x04A9, 0x3065, 0, S10M, S32K},
	{"Canon:Digital IXUS v2",	CANON_PS_S200,		0x04A9, 0x3065, 0, S10M, S32K},
	{"Canon:Digital IXUS 330",	CANON_PS_S330,		0x04A9, 0x3066, 0, S10M, S32K},
	{"Canon:PowerShot S45 (normal mode)",	CANON_PS_S45,	0x04A9, 0x306C, 0, S99M, S32K}, /* 0x306D is S45 in PTP mode */
	{"Canon:PowerShot G3 (normal mode)",	CANON_PS_G3,	0x04A9, 0x306E, 0, S99M, S32K}, /* 0x306F is G3 in PTP mode */
	{"Canon:PowerShot S230 (normal mode)",	CANON_PS_S230,	0x04A9, 0x3070, 0, S99M, S32K}, /* 0x3071 is S230 in PTP mode */
	{"Canon:Digital IXUS v3 (normal mode)",	CANON_PS_S230,	0x04A9, 0x3070, 0, S99M, S32K}, /* 0x3071 is IXUS v3 in PTP mode */
	{NULL}
	/* *INDENT-ON* */
};

#undef NO_USB
#undef S99M
#undef S10M
#undef S2M
#undef S1M
#undef S32K

#ifndef HAVE_TM_GMTOFF
/* required for time conversions in canon_int_set_time() */
extern long int timezone;
#endif


/************************************************************************
 * Methods
 ************************************************************************/


/* Simulate capabilities
 *
 * If you need to change these for a certain camera, it is time to
 * introduce and use general capabilities as replacement.
 */

#define extra_file_for_thumb_of_jpeg FALSE
#define extra_file_for_thumb_of_crw TRUE

/**
 * canon_int_filename2thumbname:
 * @camera: Camera to work on
 * @filename: the file to get the name of the thumbnail of
 * @Returns: identifier for corresponding thumbnail
 *
 * The identifier returned is 
 *  a) NULL if no thumbnail exists for this file or an internal error occured
 *  b) pointer to empty string if thumbnail is contained in the file itself
 *  c) pointer to string with file name of the corresponding thumbnail
 *  d) pointer to filename in case filename is a thumbnail itself
 */

const char *
canon_int_filename2thumbname (Camera *camera, const char *filename)
{
	static char buf[1024];
	char *p;
	static char *nullstring = "";

	/* First handle cases where we shouldn't try to get extra .THM
	 * file but use the special get_thumbnail_of_xxx function.
	 */
	if (!extra_file_for_thumb_of_jpeg && is_jpeg (filename)) {
		GP_DEBUG ("canon_int_filename2thumbname: thumbnail for JPEG \"%s\" is internal", filename);
		return nullstring;
	}
	if (!extra_file_for_thumb_of_crw && is_crw (filename)) {
		GP_DEBUG ("canon_int_filename2thumbname: thumbnail for CRW \"%s\" is internal",
			  filename);
		return nullstring;
	}

	/* We use the thumbnail file itself as the thumbnail of the
	 * thumbnail file. In short thumbfile = thumbnail(thumbfile)
	 */
	if (is_thumbnail (filename)) {
		GP_DEBUG ("canon_int_filename2thumbname: \"%s\" IS a thumbnail file",
			  filename);
		return filename;
	}

	/* There are only thumbnails for images and movies */
	if (!is_movie (filename) && !is_image (filename)) {
		GP_DEBUG ("canon_int_filename2thumbname: "
			  "\"%s\" is neither movie nor image -> no thumbnail", filename);
		return NULL;
	}

	GP_DEBUG ("canon_int_filename2thumbname: thumbnail for file \"%s\" is external",
		  filename);

	/* We just replace file ending by .THM and assume this is the
	 * name of the thumbnail file.
	 */
	if (strncpy (buf, filename, sizeof (buf)) < 0) {
		GP_DEBUG ("canon_int_filename2thumbname: Buffer too small in %s line %i.",
			  __FILE__, __LINE__);
		return NULL;
	}
	if ((p = strrchr (buf, '.')) == NULL) {
		GP_DEBUG ("canon_int_filename2thumbname: No '.' found in filename '%s' "
			  "in %s line %i.", filename, __FILE__, __LINE__);
		return NULL;
	}
	if (((p - buf) < sizeof (buf) - 4) && strncpy (p, ".THM", 4)) {
		GP_DEBUG ("canon_int_filename2thumbname: Thumbnail name for '%s' is '%s'",
			  filename, buf);
		return buf;
	} else {
		GP_DEBUG ("canon_int_filename2thumbname: "
			  "Thumbnail name for filename '%s' doesnt fit in %s line %i.",
			  filename, __FILE__, __LINE__);
		return NULL;
	}
	/* never reached */
	return NULL;
}

/*
 * does operations on a directory based on the value
 * of action : DIR_CREATE, DIR_REMOVE
 *
 */
int
canon_int_directory_operations (Camera *camera, const char *path, int action,
				GPContext *context)
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
			GP_DEBUG ("canon_int_directory_operations: "
				  "Bad operation specified : %i", action);
			return GP_ERROR_BAD_PARAMETERS;
			break;
	}

	GP_DEBUG ("canon_int_directory_operations() called to %s the directory '%s'",
		  canon_usb_funct == CANON_USB_FUNCTION_MKDIR ? "create" : "remove", path);
	switch (camera->port->type) {
		case GP_PORT_USB:
			msg = canon_usb_dialogue (camera, canon_usb_funct, &len, path,
						  strlen (path) + 1);
			if (!msg)
				return GP_ERROR;
			break;
		case GP_PORT_SERIAL:
			msg = canon_serial_dialogue (camera, context, type, 0x11, &len, path,
						     strlen (path) + 1, NULL);
			if (!msg) {
				canon_serial_error_type (camera);
				return GP_ERROR;
			}

			break;
		GP_PORT_DEFAULT
	}

	if (len != 0x4) {
		GP_DEBUG ("canon_int_directory_operations: Unexpected ammount "
			  "of data returned (expected %i got %i)", 0x4, len);
		return GP_ERROR_CORRUPTED_DATA;
	}

	if (msg[0] != 0x00) {
		gp_context_error (context, "Could not %s directory %s.",
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
canon_int_identify_camera (Camera *camera, GPContext *context)
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
			msg = canon_serial_dialogue (camera, context, 0x01, 0x12, &len, NULL);
			if (!msg) {
				GP_DEBUG ("canon_int_identify_camera: msg error");
				canon_serial_error_type (camera);
				return GP_ERROR;
			}
			break;
		GP_PORT_DEFAULT
	}

	if (len != 0x4c) {
		GP_DEBUG ("canon_int_identify_camera: Unexpected ammount of data returned "
			  "(expected %i got %i)", 0x4c, len);
		return GP_ERROR_CORRUPTED_DATA;
	}

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
canon_int_get_battery (Camera *camera, int *pwr_status, int *pwr_source, GPContext *context)
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
			msg = canon_serial_dialogue (camera, context, 0x0a, 0x12, &len, NULL);
			if (!msg) {
				canon_serial_error_type (camera);
				return GP_ERROR;
			}

			break;
		GP_PORT_DEFAULT
	}

	if (len != 0x8) {
		GP_DEBUG ("canon_int_get_battery: Unexpected ammount of data returned "
			  "(expected %i got %i)", 0x8, len);
		return GP_ERROR_CORRUPTED_DATA;
	}

	if (pwr_status)
		*pwr_status = msg[4];
	if (pwr_source)
		*pwr_source = msg[7];

	GP_DEBUG ("canon_int_get_battery: Status: %i / Source: %i\n", *pwr_status,
		  *pwr_source);

	return GP_OK;
}



#ifdef CANON_EXPERIMENTAL_CAPTURE

int
canon_int_pack_control_subcmd (unsigned char *payload, int subcmd,
			       int word0, int word1,
			       char *desc)
{
	int i, paysize;

	i = 0;
	while (canon_usb_control_cmd[i].num != 0) {
		if (canon_usb_control_cmd[i].num == subcmd)
			break;
		i++;
	}
	if (canon_usb_control_cmd[i].num == 0) {
		GP_DEBUG ("canon_int_pack_control_subcmd: unknown subcommand %d", subcmd);
		sprintf (desc, "unknown subcommand");
		return 0;
	}

	sprintf (desc, "%s", canon_usb_control_cmd[i].description);
	paysize = canon_usb_control_cmd[i].cmd_length - 0x10;
	memset (payload, 0, paysize);
	if (paysize >= 0x04) htole32a(payload,     canon_usb_control_cmd[i].subcmd);
	if (paysize >= 0x08) htole32a(payload+0x4, word0);
	if (paysize >= 0x0c) htole32a(payload+0x8, word1);

	return paysize;
}

int
canon_int_do_control_command (Camera *camera, int subcmd, int a, int b)
{
	char payload[0x4c];
	char desc[128];
	int payloadlen;
	int datalen = 0;
	unsigned char *msg = NULL;

	payloadlen = canon_int_pack_control_subcmd(payload, subcmd,
						   a, b, desc);
	GP_DEBUG("%s++ with %x, %x", desc, a, b);
	msg = canon_usb_dialogue(camera, 
				 CANON_USB_FUNCTION_CONTROL_CAMERA,
				 &datalen, payload, payloadlen);
	if (!msg && datalen != 0x1c) {
		// ERROR
		GP_DEBUG("%s returned msg=%p, datalen=%x",
			 desc, msg, datalen);
		return GP_ERROR;
	}
	msg = NULL;
	datalen = 0;
	GP_DEBUG("%s--", desc);

	return GP_OK;
}

/**
 * canon_int_capture_image
 * @camera: camera to work with
 * @path: gets filled in with the filename of the captured image
 * @Returns: gphoto2 error code
 *
 * Directs the camera to capture an image (remote shutter release via USB).
 * See the 'Protocol' file for details.
 **/
int
canon_int_capture_image (Camera *camera, CameraFilePath *path, 
			 GPContext *context)
{
	int c_res;
	char payload[0x4c];
	int payloadlen;
	unsigned char *msg = NULL;

	unsigned char *data = NULL;
	int datalen = 0;
	int mstimeout = -1;
	int transfermode = 0x0;
	
	switch (camera->port->type) {
	case GP_PORT_USB:
		gp_port_get_timeout (camera->port, &mstimeout);
		GP_DEBUG("usb port timeout starts at %dms", mstimeout);

		/*
		 * Send a sequence of CONTROL_CAMERA commands.
		 */

		// Init, extends camera lens, puts us in remote capture mode //
		if (canon_int_do_control_command (camera, 
						  CANON_USB_CONTROL_INIT, 0, 0) == GP_ERROR)
			return GP_ERROR;


		/*
		 * Set the captured image transfer mode.  We have four options 
		 * that we can specify any combo of, captured thumb to PC,
		 * full to PC, thumb to disk, and full to disk.
		 *
		 * The to-PC option requires that we figure out how to get 
		 * the alter telling us the data is in the camera buffer.
		 */
		transfermode = REMOTE_CAPTURE_THUMB_TO_DRIVE;
		if (canon_int_do_control_command (camera,
						  CANON_USB_CONTROL_SET_TRANSFER_MODE, 
						  0x04, transfermode) == GP_ERROR)
			return GP_ERROR;
		// Verify that msg points to 
                // 02 00 00 00 13 00 00 12 1c 00 00 00 xx xx xx xx
		// 00 00 00 00 00 00 00 00 00 00 00 00



		// Shutter Release //
		if (canon_int_do_control_command (camera,
						  CANON_USB_CONTROL_SHUTTER_RELEASE, 
						  0, 0) == GP_ERROR)
			return GP_ERROR;
		// Verify that msg points to 
                // 02 00 00 00 13 00 00 12 1c 00 00 00 xx xx xx xx
		// 00 00 00 00 04 00 00 00 00 00 00 00


		// End release mode //
		if (canon_int_do_control_command (camera,
						  CANON_USB_CONTROL_EXIT,
						  0, 0) == GP_ERROR)
			return GP_ERROR;

		
		/*
		 * This is how the computer responds to the event saying there's a 
		 * thumbnail capture to download, but I don't know how the event
		 * is transmitted yet.
		 */
		/*
		//gp_port_set_timeout (camera->port, 10000);
		//GP_DEBUG("set camera port timeout to 10 seconds...");

		memset (payload, 0, sizeof(payload));
		payloadlen = 0x10;

		payload[5] = 0x50;
		payload[8] = 0x02;
		payload[12] = 0x01;
		
		c_res = canon_usb_long_dialogue(camera, CANON_USB_FUNCTION_RETRIEVE_CAPTURE,
						&data, &datalen, 1024*1024,
						payload, 0x10, 1, context);
		if (datalen > 0 && data) {
			GP_DEBUG ("canon camera_capture: Got the expected amount of data back");
		} else {
			gp_context_error (context, 
					  _("camera_capture: Unexpected "
					    "amount of data returned (%i bytes)"),
					    datalen);
			return GP_ERROR;
		}
		*/
		break;
	case GP_PORT_SERIAL:
		return GP_ERROR_NOT_SUPPORTED;
		break;
	GP_PORT_DEFAULT
	}

	return GP_OK;
}

#endif /* CANON_EXPERIMENTAL_CAPTURE */


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
			       unsigned char attrs, GPContext *context)
{
	unsigned char payload[300];
	unsigned char *msg;
	unsigned char attr[4];
	int len, payload_length;

	GP_DEBUG ("canon_int_set_file_attributes() called for '%s' '%s', attributes 0x%x",
		  dir, file, attrs);

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
			if (!msg)
				return GP_ERROR;

			break;
		case GP_PORT_SERIAL:
			msg = canon_serial_dialogue (camera, context, 0xe, 0x11, &len, attr, 4,
						     dir, strlen (dir) + 1, file,
						     strlen (file) + 1, NULL);
			if (!msg) {
				canon_serial_error_type (camera);
				return GP_ERROR;
			}
			break;
		GP_PORT_DEFAULT
	}

	if (len != 0x4) {
		GP_DEBUG ("canon_int_set_file_attributes: Unexpected amount of data returned "
			  "(expected %i got %i)", 0x4, len);
		return GP_ERROR_CORRUPTED_DATA;
	}

	GP_LOG (GP_LOG_DATA,
		"canon_int_set_file_attributes: returned four bytes as expected, "
		"we should check if they indicate error or not. Returned data :");
	gp_log_data ("canon", msg, 4);

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
canon_int_set_owner_name (Camera *camera, const char *name, GPContext *context)
{
	unsigned char *msg;
	int len;

	GP_DEBUG ("canon_int_set_owner_name() called, name = '%s'", name);
	if (strlen (name) > 30) {
		gp_context_error (context,
				  _("Name '%s' (%i characters) "
				    "too long (%i chars), maximal 30 characters are "
				    "allowed."), name, strlen (name));
		return GP_ERROR;
	}

	switch (camera->port->type) {
		case GP_PORT_USB:
			msg = canon_usb_dialogue (camera, CANON_USB_FUNCTION_CAMERA_CHOWN,
						  &len, name, strlen (name) + 1);
			if (!msg)
				return GP_ERROR;
			break;
		case GP_PORT_SERIAL:
			msg = canon_serial_dialogue (camera, context, 0x05, 0x12, &len, name,
						     strlen (name) + 1, NULL);
			if (!msg) {
				canon_serial_error_type (camera);
				return GP_ERROR;
			}
			break;
		GP_PORT_DEFAULT
	}

	if (len != 0x04) {
		GP_DEBUG ("canon_int_set_owner_name: Unexpected amount of data returned "
			  "(expected %i got %i)", 0x4, len);
		return GP_ERROR_CORRUPTED_DATA;
	}

	return canon_int_identify_camera (camera, context);
}


/**
 * canon_int_get_time:
 * @camera: camera to get the current time of
 * @camera_time: pointer to where you want the camera time (NOT IN UTC!!!)
 * @Returns: gphoto2 error code
 *
 * Get camera's current time.
 *
 * The camera gives time in little endian format, therefore we need
 * to swap the 4 bytes on big-endian machines.
 *
 * Note: the time returned from the camera is not UTC but local time.
 * This means you should only use functions that don't adjust the
 * timezone, like gmtime(), instead of functions that do, like localtime()
 * since otherwise you will end up with the wrong time.
 *
 * We pass it along to calling functions in local time instead of UTC
 * since it is more correct to say 'Camera time: 2002-01-01 00:00:00'
 * if that is what the display says and not to display the cameras time
 * converted to local timezone (which will of course be wrong if you
 * are not in the timezone the cameras clock was set in).
 **/
int
canon_int_get_time (Camera *camera, time_t *camera_time, GPContext *context)
{
	unsigned char *msg;
	int len;

	GP_DEBUG ("canon_int_get_time()");

	switch (camera->port->type) {
		case GP_PORT_USB:
			msg = canon_usb_dialogue (camera, CANON_USB_FUNCTION_GET_TIME, &len,
						  NULL, 0);
			if (!msg)
				return GP_ERROR;
			break;
		case GP_PORT_SERIAL:
			msg = canon_serial_dialogue (camera, context, 0x03, 0x12, &len, NULL);
			if (!msg) {
				canon_serial_error_type (camera);
				return GP_ERROR;
			}
			break;
		GP_PORT_DEFAULT
	}

	if (len != 0x10) {
		GP_DEBUG ("canon_int_get_time: Unexpected amount of data returned "
			  "(expected %i got %i)", 0x10, len);
		return GP_ERROR_CORRUPTED_DATA;
	}

	if (camera_time != NULL)
		*camera_time = (time_t) le32atoh (msg + 4);

	/* XXX should strip \n at the end of asctime() return data */
	GP_DEBUG ("Camera time: %s", asctime (gmtime (camera_time)));

	return GP_OK;
}


/**
 * canon_int_set_time:
 * @camera: camera to get the current time of
 * @date: the date to set (in UTC)
 * @Returns: gphoto2 error code
 *
 * Set camera's current time.
 *
 * Canon cameras know nothing about time zones so we have to convert it to local
 * time (but still expressed in UNIX time format (seconds since 1970-01-01).
 */

int
canon_int_set_time (Camera *camera, time_t date, GPContext *context)
{
	unsigned char *msg;
	int len;
	char payload[12];
	time_t new_date;
	struct tm *tm;

	GP_DEBUG ("canon_int_set_time: %i=0x%x %s", (unsigned int) date, (unsigned int) date,
		  asctime (localtime (&date)));

	/* call localtime() just to get 'extern long timezone' / tm->tm_gmtoff set.
	 *
	 * this handles DST too (at least if HAVE_TM_GMTOFF), if you are in UTC+1
	 * tm_gmtoff is 3600 and if you are in UTC+1+DST tm_gmtoff is 7200 (if your
	 * DST is one hour of course).
	 */
	tm = localtime (&date);

	/* convert to local UNIX time since canon cameras know nothing about timezones */

#ifdef HAVE_TM_GMTOFF
	new_date = date + tm->tm_gmtoff;
	GP_DEBUG ("canon_int_set_time: converted %i to localtime %i (tm_gmtoff is %i)",
		  date, new_date, tm->tm_gmtoff);
#else
	new_date = date - timezone;
	GP_DEBUG ("canon_int_set_time: converted %i to localtime %i (timezone is %i)",
		  date, new_date, timezone);
#endif

	memset (payload, 0, sizeof (payload));

	htole32a (payload, (unsigned int) new_date);

	switch (camera->port->type) {
		case GP_PORT_USB:
			msg = canon_usb_dialogue (camera, CANON_USB_FUNCTION_SET_TIME, &len,
						  payload, sizeof (payload));
			if (!msg)
				return GP_ERROR;
			break;
		case GP_PORT_SERIAL:
			msg = canon_serial_dialogue (camera, context, 0x04, 0x12, &len,
						     payload, sizeof (payload), NULL);
			if (!msg) {
				canon_serial_error_type (camera);
				return GP_ERROR;
			}

			break;
		GP_PORT_DEFAULT
	}

	if (len != 0x4) {
		GP_DEBUG ("canon_int_set_time: Unexpected ammount of data returned "
			  "(expected %i got %i)", 0x4, len);
		return GP_ERROR_CORRUPTED_DATA;
	}

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
canon_int_ready (Camera *camera, GPContext *context)
{
	int res;

	GP_DEBUG ("canon_int_ready()");

	switch (camera->port->type) {
		case GP_PORT_USB:
			res = canon_usb_ready (camera);
			break;
		case GP_PORT_SERIAL:
			res = canon_serial_ready (camera, context);
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
canon_int_get_disk_name (Camera *camera, GPContext *context)
{
	unsigned char *msg;
	int len, res;

	GP_DEBUG ("canon_int_get_disk_name()");

	switch (camera->port->type) {
		case GP_PORT_USB:
			res = canon_usb_long_dialogue (camera,
						       CANON_USB_FUNCTION_FLASH_DEVICE_IDENT,
						       &msg, &len, 1024, NULL, 0, 0, context);
			if (res != GP_OK) {
				GP_DEBUG ("canon_int_get_disk_name: canon_usb_long_dialogue "
					  "failed! returned %i", res);
				return NULL;
			}
			break;
		case GP_PORT_SERIAL:
			msg = canon_serial_dialogue (camera, context, 0x0a, 0x11, &len, NULL);
			if (!msg) {
				canon_serial_error_type (camera);
				return NULL;
			}

			if (len < 5)
				return NULL;	/* should be GP_ERROR_CORRUPTED_DATA */

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
			break;
		GP_PORT_DEFAULT_RETURN (NULL)
	}

	if (!msg)
		return NULL;

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
canon_int_get_disk_name_info (Camera *camera, const char *name, int *capacity, int *available,
			      GPContext *context)
{
	unsigned char *msg = NULL;
	int len, cap, ava;

	GP_DEBUG ("canon_int_get_disk_name_info() name '%s'", name);

	CON_CHECK_PARAM_NULL (name);
	CON_CHECK_PARAM_NULL (capacity);
	CON_CHECK_PARAM_NULL (available);

	switch (camera->port->type) {
		case GP_PORT_USB:
			msg = canon_usb_dialogue (camera, CANON_USB_FUNCTION_DISK_INFO, &len,
						  name, strlen (name) + 1);
			if (!msg)
				return GP_ERROR;
			break;
		case GP_PORT_SERIAL:
			msg = canon_serial_dialogue (camera, context, 0x09, 0x11, &len, name,
						     strlen (name) + 1, NULL);
			if (!msg) {
				canon_serial_error_type (camera);
				return GP_ERROR;
			}
			break;
		GP_PORT_DEFAULT
	}

	if (len < 0x0c) {
		GP_DEBUG ("canon_int_get_disk_name_info: Unexpected ammount of data returned "
			  "(expected %i got %i)", 0x0c, len);
		return GP_ERROR_CORRUPTED_DATA;
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
const char *
gphoto2canonpath (Camera *camera, const char *path, GPContext *context)
{
	static char tmp[2000];
	char *p;

	if (path[0] != '/') {
		GP_DEBUG ("Non-absolute gphoto2 path cannot be converted");
		return NULL;
	}

	if (camera->pl->cached_drive == NULL) {
		GP_DEBUG ("NULL camera->pl->cached_drive in gphoto2canonpath");
		camera->pl->cached_drive = canon_int_get_disk_name (camera, context);
		if (camera->pl->cached_drive == NULL) {
			GP_DEBUG ("2nd NULL camera->pl->cached_drive in gphoto2canonpath");
		}
	}

	snprintf (tmp, sizeof (tmp), "%s%s", camera->pl->cached_drive, path);

	/* replace all slashes by backslashes */
	for (p = tmp; *p != '\0'; p++) {
		if (*p == '/')
			*p = '\\';
	}

	/* remove trailing backslash */
	if ((p > tmp) && (*(p - 1) == '\\'))
		*(p - 1) = '\0';

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
		GP_DEBUG ("canon2gphotopath called on invalid canon path '%s'", path);
		return NULL;
	}

	/* 3 is D: plus NULL byte */
	if (strlen (path) - 3 > sizeof (tmp)) {
		GP_DEBUG ("canon2gphotopath called on too long canon path (%i bytes): %s",
			  strlen (path), path);
		return NULL;
	}

	/* path is something like D:\FOO, we want what is after the colon */
	strcpy (tmp, path + 2);

	/* replace backslashes by slashes */
	for (p = tmp; *p != '\0'; p++) {
		if (*p == '\\')
			*p = '/';
	}

	GP_LOG (GP_LOG_DATA, "canon2gphotopath: converted '%s' to '%s'", path, tmp);

	return (tmp);
}

static void
debug_fileinfo (CameraFileInfo * info)
{
	GP_DEBUG ("<CameraFileInfo>");
	GP_DEBUG ("  <CameraFileInfoFile>");
	if ((info->file.fields & GP_FILE_INFO_NAME) != 0)
		GP_DEBUG ("    Name:   %s", info->file.name);
	if ((info->file.fields & GP_FILE_INFO_TYPE) != 0)
		GP_DEBUG ("    Type:   %s", info->file.type);
	if ((info->file.fields & GP_FILE_INFO_SIZE) != 0)
		GP_DEBUG ("    Size:   %i", info->file.size);
	if ((info->file.fields & GP_FILE_INFO_WIDTH) != 0)
		GP_DEBUG ("    Width:  %i", info->file.width);
	if ((info->file.fields & GP_FILE_INFO_HEIGHT) != 0)
		GP_DEBUG ("    Height: %i", info->file.height);
	if ((info->file.fields & GP_FILE_INFO_PERMISSIONS) != 0)
		GP_DEBUG ("    Perms:  0x%x", info->file.permissions);
	if ((info->file.fields & GP_FILE_INFO_STATUS) != 0)
		GP_DEBUG ("    Status: %i", info->file.status);
	if ((info->file.fields & GP_FILE_INFO_MTIME) != 0) {
		char *p, *time = asctime (gmtime (&info->file.mtime));

		/* remove trailing \n */
		for (p = time; *p != 0; ++p)
			/* do nothing */ ;
		*(p - 1) = '\0';
		GP_DEBUG ("    Time:   %s (%i)", time, info->file.mtime);
	}
	GP_DEBUG ("  </CameraFileInfoFile>");
	GP_DEBUG ("</CameraFileInfo>");
}

/**
 * Get the directory tree of a given flash device.
 *
 * Implicitly assumes that uint8_t[] is a char[] for strings.
 */
int
canon_int_list_directory (Camera *camera, const char *folder, CameraList *list,
			  const int flags, GPContext *context)
{
	CameraFileInfo info;
	int res;
	unsigned int dirents_length;
	unsigned char *dirent_data = NULL;
	unsigned char *end_of_data, *temp_ch, *pos;
	const char *canonfolder = gphoto2canonpath (camera, folder, context);
	int list_files = ((flags & CANON_LIST_FILES) != 0);
	int list_folders = ((flags & CANON_LIST_FOLDERS) != 0);

	GP_DEBUG ("BEGIN canon_int_list_dir() folder '%s' aka '%s' (%s, %s)", folder,
		  canonfolder, list_files ? "files" : "no files",
		  list_folders ? "folders" : "no folders");

	/* Fetch all directory entrys from the camera */
	switch (camera->port->type) {
		case GP_PORT_USB:
			res = canon_usb_get_dirents (camera, &dirent_data, &dirents_length,
						     canonfolder, context);
			break;
		case GP_PORT_SERIAL:
			res = canon_serial_get_dirents (camera, &dirent_data, &dirents_length,
							canonfolder, context);
			break;
		GP_PORT_DEFAULT
	}
	if (res != GP_OK)
		return res;

	end_of_data = dirent_data + dirents_length;

	if (dirents_length < CANON_MINIMUM_DIRENT_SIZE) {
		gp_context_error (context,
				  "canon_int_list_dir: ERROR: "
				  "initial message too short (%i < minimum %i)",
				  dirents_length, CANON_MINIMUM_DIRENT_SIZE);
		free (dirent_data);
		dirent_data = NULL;
		return GP_ERROR;
	}

	/* The first data we have got here is the dirent for the
	 * directory we are reading. Skip over 10 bytes
	 * (2 for attributes, 4 date and 4 size) and then go find
	 * the end of the directory name so that we get to the next
	 * dirent which is actually the first one we are interested
	 * in
	 */
	GP_DEBUG ("canon_int_list_dir: Camera directory listing for directory '%s'",
		  dirent_data + CANON_DIRENT_NAME);

	for (pos = dirent_data + CANON_DIRENT_NAME; pos < end_of_data && *pos != 0; pos++)
		/* do nothing */ ;
	if (pos == end_of_data || *pos != 0) {
		gp_context_error (context,
				  "canon_int_list_dir: Reached end of packet while "
				  "examining the first dirent");
		free (dirent_data);
		dirent_data = NULL;
		return GP_ERROR;
	}
	pos++;			/* skip NULL byte terminating directory name */

	/* we are now positioned at the first interesting dirent */

	/* This is the main loop, for every directory entry returned */
	while (pos < end_of_data) {
		int is_dir, is_file;
		uint16_t dirent_attrs;	/* attributes of dirent */
		uint32_t dirent_file_size;	/* size of dirent in octets */
		uint32_t dirent_time;	/* time stamp of dirent (Unix Epoch) */
		uint8_t *dirent_name;	/* name of dirent */
		size_t dirent_name_len;	/* length of dirent_name */
		size_t dirent_ent_size;	/* size of dirent in octets */
		uint32_t tmp_time;
		time_t date;
		struct tm *tm;

		dirent_attrs = le16atoh (pos + CANON_DIRENT_ATTRS);
		dirent_file_size = le32atoh (pos + CANON_DIRENT_SIZE);
		dirent_name = pos + CANON_DIRENT_NAME;

		/* see canon_int_set_time() for timezone handling */
		tmp_time = le32atoh (pos + CANON_DIRENT_TIME);
		if (tmp_time != 0) {
			/* FIXME: I just want the tm_gmtoff/timezone info */
			date = time(NULL);
			tm   = localtime (&date);
#ifdef HAVE_TM_GMTOFF
			dirent_time = tmp_time - tm->tm_gmtoff;
			GP_DEBUG ("canon_int_list_dir: converted %i to UTC %i (tm_gmtoff is %i)",
				tmp_time, dirent_time, tm->tm_gmtoff);
#else
			dirent_time = tmp_time + timezone;
			GP_DEBUG ("canon_int_list_dir: converted %i to UTC %i (timezone is %i)",
				tmp_time, dirent_time, timezone);
#endif
		} else {
			dirent_time = tmp_time;
		}

		is_dir = ((dirent_attrs & CANON_ATTR_NON_RECURS_ENT_DIR) != 0)
			|| ((dirent_attrs & CANON_ATTR_RECURS_ENT_DIR) != 0);
		is_file = !is_dir;

		GP_LOG (GP_LOG_DATA,
			"canon_int_list_dir: "
			"reading dirent at position %i of %i (0x%x of 0x%x)",
			(pos - dirent_data), (end_of_data - dirent_data), (pos - dirent_data),
			(end_of_data - dirent_data)
			);

		if (pos + CANON_MINIMUM_DIRENT_SIZE > end_of_data) {
			if (camera->port->type == GP_PORT_SERIAL) {
				/* check to see if it is only NULL bytes left,
				 * that is not an error for serial cameras
				 * (at least the A50 adds five zero bytes at the end)
				 */
				for (temp_ch = pos; temp_ch < end_of_data && *temp_ch; temp_ch++) ;	/* do nothing */

				if (temp_ch == end_of_data) {
					GP_DEBUG ("canon_int_list_dir: "
						  "the last %i bytes were all 0 - ignoring.",
						  temp_ch - pos);
					break;
				} else {
					GP_DEBUG ("canon_int_list_dir: "
						  "byte[%i=0x%x] == %i=0x%x", temp_ch - pos,
						  temp_ch - pos, *temp_ch, *temp_ch);
					GP_DEBUG ("canon_int_list_dir: "
						  "pos is 0x%x, end_of_data is 0x%x, temp_ch is 0x%x - diff is 0x%x",
						  pos, end_of_data, temp_ch, temp_ch - pos);
				}
			}
			GP_DEBUG ("canon_int_list_dir: "
				  "dirent at position %i=0x%x of %i=0x%x is too small, "
				  "minimum dirent is %i bytes", (pos - dirent_data),
				  (pos - dirent_data), (end_of_data - dirent_data),
				  (end_of_data - dirent_data), CANON_MINIMUM_DIRENT_SIZE);
			gp_context_error (context,
					  "canon_int_list_dir: "
					  "truncated directory entry encountered");
			free (dirent_data);
			dirent_data = NULL;
			return GP_ERROR;
		}

		/* Check end of this dirent, 10 is to skip over
		 * 2    attributes + 0x00
		 * 4    file size
		 * 4    file date (UNIX localtime)
		 * to where the direntry name begins.
		 */
		for (temp_ch = dirent_name; temp_ch < end_of_data && *temp_ch != 0;
		     temp_ch++) ;

		if (temp_ch == end_of_data || *temp_ch != 0) {
			GP_DEBUG ("canon_int_list_dir: "
				  "dirent at position %i of %i has invalid name in it."
				  "bailing out with what we've got.", (pos - dirent_data),
				  (end_of_data - dirent_data));
			break;
		}
		dirent_name_len = strlen (dirent_name);
		dirent_ent_size = CANON_MINIMUM_DIRENT_SIZE + dirent_name_len;

		/* check that length of name in this dirent is not of unreasonable size.
		 * 256 was picked out of the blue
		 */
		if (dirent_name_len > 256) {
			GP_DEBUG ("canon_int_list_dir: "
				  "dirent at position %i of %i has too long name in it (%i bytes)."
				  "bailing out with what we've got.", (pos - dirent_data),
				  (end_of_data - dirent_data), dirent_name_len);
			break;
		}

		/* 10 bytes of attributes, size and date, a name and a NULL terminating byte */
		/* don't use GP_DEBUG since we log this with GP_LOG_DATA */
		GP_LOG (GP_LOG_DATA,
			"canon_int_list_dir: dirent determined to be %i=0x%x bytes :",
			dirent_ent_size, dirent_ent_size);
		gp_log_data ("canon", pos, dirent_ent_size);
		if (dirent_name_len) {
			/* OK, this directory entry has a name in it. */

			if ((list_folders && is_dir) || (list_files && is_file)) {

				/* we're going to fill out the info structure
				   in this block */
				memset (&info, 0, sizeof (info));

				/* we start with nothing and continously add stuff */
				info.file.fields = GP_FILE_INFO_NONE;

				strncpy (info.file.name, dirent_name, sizeof (info.file.name));
				info.file.fields |= GP_FILE_INFO_NAME;

				info.file.mtime = dirent_time;
				if (info.file.mtime != 0)
					info.file.fields |= GP_FILE_INFO_MTIME;

				if (is_file) {
					/* determine file type based on file name
					 * this stuff only makes sense for files, not for folders
					 */

					strncpy (info.file.type,
						 filename2mimetype (info.file.name),
						 sizeof (info.file.type));
					info.file.fields |= GP_FILE_INFO_TYPE;

					if ((dirent_attrs & CANON_ATTR_DOWNLOADED) == 0)
						info.file.status = GP_FILE_STATUS_DOWNLOADED;
					else
						info.file.status =
							GP_FILE_STATUS_NOT_DOWNLOADED;
					info.file.fields |= GP_FILE_INFO_STATUS;

					/* the size is located at offset 2 and is 4
					 * bytes long, re-order little/big endian */
					info.file.size = dirent_file_size;
					info.file.fields |= GP_FILE_INFO_SIZE;

					/* file access modes */
					if ((dirent_attrs & CANON_ATTR_WRITE_PROTECTED) == 0)
						info.file.permissions =
							GP_FILE_PERM_READ |
							GP_FILE_PERM_DELETE;
					else
						info.file.permissions = GP_FILE_PERM_READ;
					info.file.fields |= GP_FILE_INFO_PERMISSIONS;
				}

				/* print dirent as text */
				GP_DEBUG ("Raw info: name=%s is_dir=%i, is_file=%i, attrs=0x%x", dirent_name, is_dir, is_file, dirent_attrs);
				debug_fileinfo (&info);

				if (is_file) {
					/*
					 * Append directly to the filesystem instead of to the list,
					 * because we have additional information.
					 */
					if (!camera->pl->list_all_files
					    && !is_image (info.file.name)
					    && !is_movie (info.file.name)) {
						/* do nothing */
						GP_DEBUG ("Ignored %s/%s", folder,
							  info.file.name);
					} else {
						const char *thumbname;

						res = gp_filesystem_append (camera->fs, folder,
								      info.file.name, context);
						if (res != GP_OK) {
							GP_DEBUG ("Could not gp_filesystem_append "
								  "%s in folder %s: %s",
								  info.file.name, folder, gp_result_as_string (res));
						} else {
							GP_DEBUG ("Added file %s/%s", folder,
								  info.file.name);

							thumbname =
								canon_int_filename2thumbname (camera,
											      info.file.name);
							if (thumbname == NULL) {
								/* no thumbnail */
							} else {
								/* all known Canon cams have JPEG thumbs */
								info.preview.fields =
									GP_FILE_INFO_TYPE;
								strncpy (info.preview.type,
									 GP_MIME_JPEG,
									 sizeof (info.preview.type));
							}

							res = gp_filesystem_set_info_noop (camera->fs,
										     folder, info,
										     context);
							if (res != GP_OK) {
								GP_DEBUG ("Could not gp_filesystem_set_info_noop() "
									  "%s in folder %s: %s",
									  info.file.name, folder, gp_result_as_string (res));
							}
						}
					}
				}
				if (is_dir) {
					res = gp_list_append (list, info.file.name, NULL);
					if (res != GP_OK)
						GP_DEBUG ("Could not gp_list_append "
							  "folder %s: %s",
							  folder, gp_result_as_string (res));
				}
			} else {
				/* this case could mean that this was the last dirent */
				GP_DEBUG ("canon_int_list_dir: "
					  "dirent at position %i of %i has NULL name, skipping.",
					  (pos - dirent_data), (end_of_data - dirent_data));
			}
		}

		/* make 'pos' point to next dirent in packet.
		 * first we skip 10 bytes of attribute, size and date,
		 * then we skip the name plus 1 for the NULL
		 * termination bytes.
		 */
		pos += dirent_ent_size;
	}
	free (dirent_data);
	dirent_data = NULL;

	GP_DEBUG ("<FILESYSTEM-DUMP>");
	gp_filesystem_dump (camera->fs);
	GP_DEBUG ("</FILESYSTEM-DUMP>");

	GP_DEBUG ("END canon_int_list_dir() folder '%s' aka '%s'", folder, canonfolder);

	return GP_OK;
}

int
canon_int_get_file (Camera *camera, const char *name, unsigned char **data, int *length,
		    GPContext *context)
{
	switch (camera->port->type) {
		case GP_PORT_USB:
			return canon_usb_get_file (camera, name, data, length, context);
			break;
		case GP_PORT_SERIAL:
			*data = canon_serial_get_file (camera, name, length, context);
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
 * @retdata: The thumbnail data
 *
 * NOTE: Since cameras that do not store the thumbnail in a separate
 * file does not return just the thumbnail but the first 10813 bytes
 * of the image (most oftenly the EXIF header with thumbnail data in
 * it) this must be treated before called a true thumbnail.
 *
 * Returns GPError.
 **/
int
canon_int_get_thumbnail (Camera *camera, const char *name, unsigned char **retdata,
			 int *length, GPContext *context)
{
	int res;

	GP_DEBUG ("canon_int_get_thumbnail() called for file '%s'", name);

	CON_CHECK_PARAM_NULL (retdata);
	CON_CHECK_PARAM_NULL (length);

	switch (camera->port->type) {
		case GP_PORT_USB:
			res = canon_usb_get_thumbnail (camera, name, retdata, length, context);
			break;
		case GP_PORT_SERIAL:
			res = canon_serial_get_thumbnail (camera, name, retdata, length,
							  context);
			break;
		GP_PORT_DEFAULT
	}
	if (res != GP_OK) {
		GP_DEBUG ("canon_int_get_thumbnail() failed, returned %i", res);
		return res;
	}

	return res;
}

int
canon_int_delete_file (Camera *camera, const char *name, const char *dir, GPContext *context)
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
			msg = canon_serial_dialogue (camera, context, 0xd, 0x11, &len, dir,
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
		return GP_ERROR_CORRUPTED_DATA;
	}

	if (msg[0] == 0x29) {
		gp_context_error (context, _("File protected."));
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
canon_int_put_file (Camera *camera, CameraFile *file, char *destname, char *destpath,
		    GPContext *context)
{
	switch (camera->port->type) {
		case GP_PORT_USB:
			return canon_usb_put_file (camera, file, destname, destpath, 
						      context);
			break;
		case GP_PORT_SERIAL:
			return canon_serial_put_file (camera, file, destname, destpath,
						      context);
			break;
		GP_PORT_DEFAULT
	}
	/* Never reached */
	return GP_ERROR;
}

/**
 * canon_int_extract_jpeg_thumb:
 *
 * extract thumbnail from JFIF image (A70) or movie .thm file.
 * just extracted the code from the old #canon_int_get_thumbnail
 **/

int
canon_int_extract_jpeg_thumb (unsigned char *data, const unsigned int datalen,
			     unsigned char **retdata, unsigned int *retdatalen,
			     GPContext *context)
{
	unsigned int i, thumbstart = 0, thumbsize = 0;

	CHECK_PARAM_NULL (data);
	CHECK_PARAM_NULL (retdata);

	*retdata = NULL;
	*retdatalen = 0;

	if (data[0] != JPEG_ESC || data[1] != JPEG_BEG) {
		gp_context_error (context, "Could not extract JPEG "
				  "thumbnail from data: Data is not JFIF");
		GP_DEBUG ("canon_int_extract_jpeg_thumb: data is not JFIF, cannot extract thumbnail");
		return GP_ERROR_CORRUPTED_DATA;
	}
	
	/* pictures are JFIF files, we skip the first 2 bytes (0xFF 0xD8)
	 * first go look for start of JPEG, when that is found we set thumbstart
	 * to the current position and never look for JPEG begin bytes again.
	 * when thumbstart is set look for JPEG end.
	 */
	for (i = 3; i < datalen; i++)
		if (data[i] == JPEG_ESC) {
			if (! thumbstart) {
				if (i < (datalen - 3) &&
					data[i + 1] == JPEG_BEG &&
					((data[i + 3] == JPEG_SOS) || (data[i + 3] == JPEG_A50_SOS)))
					thumbstart = i;
			} else if (i < (datalen - 1) && (data[i + 1] == JPEG_END)) {
				thumbsize = i + 2 - thumbstart;
				break;
			}
		
		}					
	if (! thumbsize) {
		gp_context_error (context, "Could not extract JPEG "
				  "thumbnail from data: No beginning/end");
		GP_DEBUG ("canon_int_extract_jpeg_thumb: could not find JPEG "
			  "beginning (offset %s) or end (size %s) in %i bytes of data",
			  datalen, thumbstart, thumbsize);
		return GP_ERROR_CORRUPTED_DATA;	  
	}

	/* now that we know the size of the thumbnail embedded in the JFIF data, malloc() */
	*retdata = malloc (thumbsize);
	if (! *retdata) {
		GP_DEBUG ("canon_int_extract_jpeg_thumb: could not allocate %i bytes of memory", thumbsize);
		return GP_ERROR_NO_MEMORY;
	}

	/* and copy */
	memcpy (*retdata, data + thumbstart, thumbsize);
	*retdatalen = thumbsize;
	
	return GP_OK;
}

/*
 * Local Variables:
 * c-file-style:"linux"
 * indent-tabs-mode:t
 * End:
 */
