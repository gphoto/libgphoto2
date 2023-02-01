/**************************************************************************
 *
 * library.c
 *
 *   Canon Camera library for the gphoto project,
 *   Copyright 1999 Wolfgang G. Reissnegger
 *   Developed for the Canon PowerShot A50
 *   Additions for PowerShot A5 by Ole W. Saastad
 *   Copyright 2000: Other additions  by Edouard Lafargue, Philippe Marzouk
 *
 * This file contains all the "glue code" required to use the canon
 * driver with libgphoto2.
 *
 ****************************************************************************/


/****************************************************************************
 *
 * include files
 *
 ****************************************************************************/

#define _DEFAULT_SOURCE

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <ctype.h>

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#include <gphoto2/gphoto2.h>

#include "libgphoto2/i18n.h"


#include "util.h"
#include "library.h"
#include "canon.h"
#include "serial.h"
#include "usb.h"


#ifdef __GNUC__
# define __unused__ __attribute__((unused))
#else
# define __unused__
#endif


#ifndef HAVE_TM_GMTOFF
/* required for time conversions in camera_summary() */
extern long int timezone;
#endif

#define CHECK_RESULT(result) {int r = (result); if (r < 0) return (r);}

#ifndef TRUE
# define TRUE (0==0)
#endif
#ifndef FALSE
# define FALSE (0!=0)
#endif

#ifdef CANON_EXPERIMENTAL_UPLOAD
#define UPLOAD_BOOL TRUE
#else
#define UPLOAD_BOOL FALSE
#endif

static const struct canonCaptureSizeClassStruct captureSizeArray[] = {
	{CAPTURE_COMPATIBILITY, N_("Compatibility Mode")},
        {CAPTURE_THUMB, N_("Thumbnail")},
        {CAPTURE_FULL_IMAGE, N_("Full Image")},
	{0, NULL}
};

static const struct canonIsoStateStruct isoStateArray[] = {
	{ISO_50, "50"},
	{ISO_100, "100"},
	{ISO_125, "125"},
	{ISO_160, "160"},
	{ISO_200, "200"},
	{ISO_250, "250"},
	{ISO_320, "320"},
	{ISO_400, "400"},
	{ISO_500, "500"},
	{ISO_640, "640"},
	{ISO_800, "800"},
	{ISO_1000, "1000"},
	{ISO_1250, "1250"},
	{ISO_1600, "1600"},
	{ISO_3200, "3200"},
	{0, NULL},
};

static const struct canonShutterSpeedStateStruct shutterSpeedStateArray[] = {
        {SHUTTER_SPEED_BULB, N_("Bulb")},
        {SHUTTER_SPEED_30_SEC,"30"},
        {SHUTTER_SPEED_25_SEC,"25"},
        {SHUTTER_SPEED_20_SEC,"20"},
        {SHUTTER_SPEED_15_SEC,"15"},
        {SHUTTER_SPEED_13_SEC,"13"},
        {SHUTTER_SPEED_10_SEC,"10"},
        {SHUTTER_SPEED_8_SEC,"8"},
        {SHUTTER_SPEED_6_SEC,"6"},
        {SHUTTER_SPEED_5_SEC,"5"},
        {SHUTTER_SPEED_4_SEC,"4"},
        {SHUTTER_SPEED_3_2_SEC,"3.2"},
        {SHUTTER_SPEED_2_5_SEC,"2.5"},
        {SHUTTER_SPEED_2_SEC,"2"},
        {SHUTTER_SPEED_1_6_SEC,"1.6"},
        {SHUTTER_SPEED_1_3_SEC,"1.3"},
        {SHUTTER_SPEED_1_SEC,"1"},
	{SHUTTER_SPEED_0_8_SEC,"0.8"},
	{SHUTTER_SPEED_0_6_SEC,"0.6"},
	{SHUTTER_SPEED_0_5_SEC,"0.5"},
	{SHUTTER_SPEED_0_4_SEC,"0.4"},
	{SHUTTER_SPEED_0_3_SEC,"0.3"},
        {SHUTTER_SPEED_1_4,"1/4"},
        {SHUTTER_SPEED_1_5,"1/5"},
        {SHUTTER_SPEED_1_6,"1/6"},
        {SHUTTER_SPEED_1_8,"1/8"},
        {SHUTTER_SPEED_1_10,"1/10"},
        {SHUTTER_SPEED_1_13,"1/13"},
        {SHUTTER_SPEED_1_15,"1/15"},
        {SHUTTER_SPEED_1_20,"1/20"},
        {SHUTTER_SPEED_1_25,"1/25"},
        {SHUTTER_SPEED_1_30,"1/30"},
        {SHUTTER_SPEED_1_40,"1/40"},
        {SHUTTER_SPEED_1_50,"1/50"},
        {SHUTTER_SPEED_1_60,"1/60"},
        {SHUTTER_SPEED_1_80,"1/80"},
        {SHUTTER_SPEED_1_100,"1/100"},
        {SHUTTER_SPEED_1_125,"1/125"},
        {SHUTTER_SPEED_1_160,"1/160"},
        {SHUTTER_SPEED_1_200,"1/200"},
        {SHUTTER_SPEED_1_250,"1/250"},
        {SHUTTER_SPEED_1_320,"1/320"},
        {SHUTTER_SPEED_1_400,"1/400"},
        {SHUTTER_SPEED_1_500,"1/500"},
        {SHUTTER_SPEED_1_640,"1/640"},
        {SHUTTER_SPEED_1_800,"1/800"},
        {SHUTTER_SPEED_1_1000,"1/1000"},
        {SHUTTER_SPEED_1_1250,"1/1250"},
        {SHUTTER_SPEED_1_1600,"1/1600"},
        {SHUTTER_SPEED_1_2000,"1/2000"},
        {SHUTTER_SPEED_1_2500,"1/2500"},
        {SHUTTER_SPEED_1_3200,"1/3200"},
        {SHUTTER_SPEED_1_4000,"1/4000"},
        {SHUTTER_SPEED_1_5000,"1/5000"},
        {SHUTTER_SPEED_1_6400,"1/6400"},
        {SHUTTER_SPEED_1_8000,"1/8000"},
	{0, NULL},
};

static const struct canonApertureStateStruct apertureStateArray[] = {
	{APERTURE_F1_2, "1.2"},
	{APERTURE_F1_4, "1.4"},
	{APERTURE_F1_6, "1.6"},
	{APERTURE_F1_8, "1.8"},
	{APERTURE_F2_0, "2.0"},
	{APERTURE_F2_2, "2.2"},
	{APERTURE_F2_5, "2.5"},
	{APERTURE_F2_8, "2.8"},
	{APERTURE_F3_2, "3.2"},
	{APERTURE_F3_5, "3.5"},
	{APERTURE_F4_0, "4.0"},
	{APERTURE_F4_5, "4.5"},
	{APERTURE_F5_0, "5.0"},
	{APERTURE_F5_6, "5.6"},
	{APERTURE_F6_3, "6.3"},
	{APERTURE_F7_1, "7.1"},
	{APERTURE_F8, "8"},
	{APERTURE_F9, "9"},
	{APERTURE_F10, "10"},
	{APERTURE_F11, "11"},
	{APERTURE_F13, "13"},
	{APERTURE_F14, "14"},
	{APERTURE_F16, "16"},
	{APERTURE_F18, "18"},
	{APERTURE_F20, "20"},
	{APERTURE_F22, "22"},
	{APERTURE_F25, "25"},
	{APERTURE_F29, "29"},
	{APERTURE_F32, "32"},
	{0, NULL},
};

static const struct canonFocusModeStateStruct focusModeStateArray[] = {
        {AUTO_FOCUS_ONE_SHOT, N_("Auto focus: one-shot")},
        {AUTO_FOCUS_AI_SERVO, N_("Auto focus: AI servo")},
        {AUTO_FOCUS_AI_FOCUS, N_("Auto focus: AI focus")},
        {MANUAL_FOCUS, N_("Manual focus")},
	{0, NULL},
};

static const struct canonBeepModeStateStruct beepModeStateArray[] = {
	{BEEP_OFF, N_("Beep off")},
	{BEEP_ON, N_("Beep on")},
	{0, NULL},
};

static const struct canonFlashModeStateStruct flashModeStateArray[] = {
	{FLASH_MODE_OFF, N_("Flash off")},
	{FLASH_MODE_ON, N_("Flash on")},
	{FLASH_MODE_AUTO, N_("Flash auto")},
	{0, NULL},
};

static const struct canonExposureBiasStateStruct exposureBiasStateArray[] = {
	{0x10,0x08,"+2"},
	{0x0d,0x0b,"+1 2/3"},
	{0x0c,0x0c,"+1 1/2"},
	{0x0b,0x0d,"+1 1/3"},
	{0x08,0x10,"+1"},
	{0x05,0x13,"+2/3"},
	{0x04,0x14,"+1/2"},
	{0x03,0x15,"+1/3"},
	{0x00,0x18,"0"},
	{0xfd,0x1b,"-1/3"},
	{0xfc,0x1c,"-1/2"},
	{0xfb,0x1d,"-2/3"},
	{0xf8,0x20,"-1"},
	{0xf5,0x23,"-1 1/3"},
	{0xf4,0x24,"-1 1/2"},
	{0xf3,0x25,"-1 2/3"},
	{0xf0,0x28,"-2"},
	{0,0,NULL},
};


static const struct canonShootingModeStateStruct shootingModeStateArray[] = {
	{0x00,N_("AUTO")},
	{0x01,N_("P")},
	{0x02,N_("Tv")},
	{0x03,N_("Av")},
	{0x04,N_("M")},
	{0x05,N_("A-DEP")},
	{0x06,N_("M-DEP")},
	{0x07,N_("Bulb")},
	{0x65,N_("Manual 2")},
	{0x66,N_("Far scene")},
	{0x67,N_("Fast shutter")},
	{0x68,N_("Slow shutter")},
	{0x69,N_("Night scene")},
	{0x6a,N_("Gray scale")},
	{0x6b,N_("Sepia")},
	{0x6c,N_("Portrait")},
	{0x6d,N_("Spot")},
	{0x6e,N_("Macro")},
	{0x6f,N_("BW")},
	{0x70,N_("PanFocus")},
	{0x71,N_("Vivid")},
	{0x72,N_("Neutral")},
	{0x73,N_("Flash off")},
	{0x74,N_("Long shutter")},
	{0x75,N_("Super macro")},
	{0x76,N_("Foliage")},
	{0x77,N_("Indoor")},
	{0x78,N_("Fireworks")},
	{0x79,N_("Beach")},
	{0x7a,N_("Underwater")},
	{0x7b,N_("Snow")},
	{0x7c,N_("Kids and pets")},
	{0x7d,N_("Night snapshot")},
	{0x7e,N_("Digital macro")},
	{0x7f,N_("MyColors")},
	{0x80,N_("Photo in movie")},
	{0, NULL}
};


static const struct canonImageFormatStateStruct imageFormatStateArray[] = {
	{IMAGE_FORMAT_RAW, N_("RAW"),
	 0x04, 0x02, 0x00},
	{IMAGE_FORMAT_RAW_2, N_("RAW 2"),
	 0x03, 0x02, 0x00},
	{IMAGE_FORMAT_SMALL_NORMAL_JPEG, N_("Small Normal JPEG"),
	 0x02, 0x01, 0x02},
	{IMAGE_FORMAT_SMALL_FINE_JPEG, N_("Small Fine JPEG"),
	 0x03, 0x01, 0x02},
	{IMAGE_FORMAT_MEDIUM_NORMAL_JPEG, N_("Medium Normal JPEG"),
	 0x02, 0x01, 0x01},
	{IMAGE_FORMAT_MEDIUM_FINE_JPEG, N_("Medium Fine JPEG"),
	 0x03, 0x01, 0x01},
	{IMAGE_FORMAT_LARGE_NORMAL_JPEG, N_("Large Normal JPEG"),
	 0x02, 0x01, 0x00},
	{IMAGE_FORMAT_LARGE_FINE_JPEG, N_("Large Fine JPEG"),
	 0x03, 0x01, 0x00},
	{IMAGE_FORMAT_RAW_AND_SMALL_NORMAL_JPEG, N_("RAW + Small Normal JPEG"),
	 0x24, 0x12, 0x20},
	{IMAGE_FORMAT_RAW_AND_SMALL_FINE_JPEG, N_("RAW + Small Fine JPEG"),
	 0x34, 0x12, 0x20},
	{IMAGE_FORMAT_RAW_AND_MEDIUM_NORMAL_JPEG, N_("RAW + Medium Normal JPEG"),
	 0x24, 0x12, 0x10},
	{IMAGE_FORMAT_RAW_AND_MEDIUM_FINE_JPEG, N_("RAW + Medium Fine JPEG"),
	 0x34, 0x12, 0x10},
	{IMAGE_FORMAT_RAW_AND_LARGE_NORMAL_JPEG, N_("RAW + Large Normal JPEG"),
	 0x24, 0x12, 0x00},
	{IMAGE_FORMAT_RAW_AND_LARGE_FINE_JPEG, N_("RAW + Large Fine JPEG"),
	 0x34, 0x12, 0x00},
	{0, NULL, 0, 0, 0},
};

/**
 * camera_id:
 * @id: string buffer to receive text identifying camera type
 *
 * Standard gphoto2 camlib interface
 *
 * Returns:
 *  string is copied into @id.
 *
 */
int camera_id (CameraText *id)
{
	/* GP_DEBUG ("camera_id()"); */

	strcpy (id->text, "canon");

	return GP_OK;
}

/**
 * camera_manual
 * @camera: Camera for which to get manual text (unused)
 * @manual: Buffer into which to copy manual text
 * @context: unused
 *
 * Returns the "manual" text for cameras supported by this driver.
 *
 * Returns: manual text is copied into @manual->text.
 *
 */
static int
camera_manual (Camera __unused__ *camera, CameraText *manual,
	       GPContext __unused__ *context)
{
	GP_DEBUG ("camera_manual()");

	strncpy (manual->text,
		 _("This is the driver for Canon PowerShot, Digital IXUS, IXY Digital,\n"
		   " and EOS Digital cameras in their native (sometimes called \"normal\")\n"
		   " mode. It also supports a small number of Canon digital camcorders\n"
		   " with still image capability.\n"
		   "It includes code for communicating over a serial port or USB connection,\n"
		   " but not (yet) over IEEE 1394 (Firewire).\n"
		   "It is designed to work with over 70 models as old as the PowerShot A5\n"
		   " and Pro70 of 1998 and as new as the PowerShot A510 and EOS 350D of\n"
		   " 2005.\n"
		   "It has not been verified against the EOS 1D or EOS 1Ds.\n"
		   "For the A50, using 115200 bps may effectively be slower than using 57600\n"
		   "If you experience a lot of serial transmission errors, try to have your\n"
		   " computer as idle as possible (i.e. no disk activity)\n"),
		   sizeof ( CameraText )
		);

	return GP_OK;
}

/**
 * camera_abilities:
 * @list: list of abilities
 *
 * Returns a list of what any of the cameras supported by this driver
 *   can do. Each entry of the list will represent one camera model.
 *
 * Returns: list of abilities in @list
 *
 */
int
camera_abilities (CameraAbilitiesList *list)
{
	int i;
	CameraAbilities a;

	/* GP_DEBUG ("camera_abilities()"); */

	for (i = 0; models[i].id_str; i++) {
		memset (&a, 0, sizeof (a));

		a.status = GP_DRIVER_STATUS_PRODUCTION;

		strcpy (a.model, models[i].id_str);
		a.port = 0;
		if (models[i].usb_vendor && models[i].usb_product) {
			a.port |= GP_PORT_USB;
			a.usb_vendor = models[i].usb_vendor;
			a.usb_product = models[i].usb_product;
		}
		if (models[i].serial_id_string != NULL) {
			a.port |= GP_PORT_SERIAL;
			a.speed[0] = 9600;
			a.speed[1] = 19200;
			a.speed[2] = 38400;
			a.speed[3] = 57600;
			a.speed[4] = 115200;
			a.speed[5] = 0;
		}
		a.operations = GP_OPERATION_CONFIG;

		if (models[i].usb_capture_support != CAP_NON) {
			a.operations |= GP_OPERATION_CAPTURE_IMAGE;
			a.operations |= GP_OPERATION_CAPTURE_PREVIEW;
		}

		a.folder_operations =
			GP_FOLDER_OPERATION_MAKE_DIR |
			GP_FOLDER_OPERATION_REMOVE_DIR;

		if (UPLOAD_BOOL || (models[i].serial_id_string != NULL)) {
			a.folder_operations |= GP_FOLDER_OPERATION_PUT_FILE;
		}

		a.file_operations = GP_FILE_OPERATION_DELETE | GP_FILE_OPERATION_PREVIEW | GP_FILE_OPERATION_EXIF;
		gp_abilities_list_append (list, a);
	}

	return GP_OK;
}

/**
 * clear_readiness
 * @camera: the camera to affect
 *
 * Clears the cached flag indicating that the camera is ready.
 *
 */
void
clear_readiness (Camera *camera)
{
	GP_DEBUG ("clear_readiness()");

	camera->pl->cached_ready = 0;
}

/**
 * check_readiness
 * @camera: the camera to affect
 * @context: context for error reporting
 *
 * Checks the readiness of the camera. If the cached "ready" flag isn't
 *  set, checks using canon_int_ready().
 *
 * Returns: 1 if ready, 0 if not.
 *
 */
static int
check_readiness (Camera *camera, GPContext *context)
{
	int status;

	GP_DEBUG ("check_readiness() cached_ready == %i", camera->pl->cached_ready);

	if (camera->pl->cached_ready)
		return 1;
	status = canon_int_ready (camera, context);
	if ( status == GP_OK ) {
		GP_DEBUG ("Camera type: %s (%d)", camera->pl->md->id_str,
			  camera->pl->md->model);
		camera->pl->cached_ready = 1;
		return 1;
	}
	gp_context_error ( context, _("Camera unavailable: %s"),
			   gp_result_as_string ( status ) );
	return 0;
}

/**
 * canon_int_switch_camera_off
 * @camera: the camera to affect
 * @context: context for error reporting
 *
 * Switches a serial camera off. Does nothing to a USB camera.
 *
 */
static void
canon_int_switch_camera_off (Camera *camera, GPContext *context)
{
	GP_DEBUG ("switch_camera_off()");

	switch (camera->port->type) {
		case GP_PORT_SERIAL:
			gp_context_status (context, _("Switching Camera Off"));
			canon_serial_off (camera);
			break;
		case GP_PORT_USB:
			GP_DEBUG ("Not trying to shut down USB camera...");
			break;
	GP_PORT_DEFAULT_RETURN_EMPTY}
	clear_readiness (camera);
}

/**
 * camera_exit
 * @camera: the camera to affect
 * @context: context for error reporting
 *
 * Ends our use of the camera. For USB cameras, unlock keys. For
 *  serial cameras, switch the camera off and unlock the port.
 *
 * Returns: GP_OK
 *
 */
static int
camera_exit (Camera *camera, GPContext *context)
{
	int res = 0;

	if (camera->port->type == GP_PORT_USB)
		canon_usb_unlock_keys (camera, context);

	/* Turn off remote control if enabled (remote control only
	 * applies to USB cameras, but in case of serial connection
	 * remote_control should never be seen set here) */
	if (camera->pl->remote_control) {
		res = canon_int_end_remote_control (camera, context);
		if (res != GP_OK)
			return -1;
	}

	if (camera->pl) {
		canon_int_switch_camera_off (camera, context);
		free (camera->pl);
		camera->pl = NULL;
	}

	return GP_OK;
}

/**
 * camera_capture_preview
 * @camera: the camera to affect
 * @context: context for error reporting
 *
 * Triggers the camera to capture a new image. Discards the image and
 * downloads its thumbnail to the host computer without storing it on
 * the camera.
 *
 * Returns: gphoto2 error code
 *
 */
static int
camera_capture_preview (Camera *camera, CameraFile *file, GPContext *context)
{
	unsigned int size;
	int code;
	unsigned char *data;

	GP_DEBUG ("canon_capture_preview() called");

	code = canon_int_capture_preview (camera, &data, &size, context);
	if ( code != GP_OK) {
		gp_context_error (context, _("Error capturing image"));
		return code;
	}
	gp_file_set_data_and_size ( file, (char *)data, size );
	gp_file_set_mime_type (file, GP_MIME_JPEG);	/* always */
	return GP_OK;
}

/**
 * camera_capture
 * @camera: the camera to affect
 * @type: type of capture (only GP_CAPTURE_IMAGE supported)
 * @path: path on camera to use
 * @context: context for error reporting
 *
 * Captures a single image from the camera.
 *
 * Returns: gphoto2 error code
 *
 */
static int
camera_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path,
		GPContext *context)
{
	int code;

	GP_DEBUG ("canon_capture() called");

	if (type != GP_CAPTURE_IMAGE) {
		return GP_ERROR_NOT_SUPPORTED;
	}

	code = canon_int_capture_image (camera, path, context);
	if (code != GP_OK) {
		gp_context_error (context, _("Error capturing image"));
		return code;
	}

	return GP_OK;
}

/**
 * canon_get_batt_status
 * @camera: the camera to affect
 * @pwr_status: to receive power status from camera
 * @pwr_source: to receive power source from camera
 * @context: context for error reporting
 *
 * Gets the power status for the camera.
 *
 * Returns: -1 if camera isn't ready; status from canon_int_get_battery() otherwise.
 *   @pwr_status will contain @CAMERA_POWER_OK if power is OK (i.e. battery isn't low)
 *   @pwr_source will have bit %CAMERA_MASK_BATTERY set if battery power is used
 *
 */
static int
canon_get_batt_status (Camera *camera, int *pwr_status, int *pwr_source, GPContext *context)
{
	GP_DEBUG ("canon_get_batt_status() called");

	if (!check_readiness (camera, context))
		return -1;

	return canon_int_get_battery (camera, pwr_status, pwr_source, context);
}

/**
 * update_disk_cache
 * @camera: the camera to work on
 * @context: the context to print on error
 *
 * Updates the disk cache for this camera.
 *
 * Returns: 1 on success, 0 on failure
 *
 */
static int
update_disk_cache (Camera *camera, GPContext *context)
{
	char root[10];		/* D:\ or such */
	int res;

	GP_DEBUG ("update_disk_cache()");

	if (camera->pl->cached_disk)
		return 1;
	if (!check_readiness (camera, context))
		return 0;
	camera->pl->cached_drive = canon_int_get_disk_name (camera, context);
	if (!camera->pl->cached_drive) {
		gp_context_error (context, _("Could not get disk name: %s"),
				  _("No reason available"));
		return 0;
	}
	snprintf (root, sizeof (root), "%s\\", camera->pl->cached_drive);
	res = canon_int_get_disk_name_info (camera, root, &camera->pl->cached_capacity,
					    &camera->pl->cached_available, context);
	if (res != GP_OK) {
		gp_context_error (context, _("Could not get disk info: %s"),
				  gp_result_as_string (res));
		return 0;
	}
	camera->pl->cached_disk = 1;

	return 1;
}

/****************************************************************************
 *
 * gphoto library interface calls
 *
 ****************************************************************************/

static int
file_list_func (CameraFilesystem __unused__ *fs, const char *folder,
		CameraList *list, void *data,
		GPContext *context)
{
	Camera *camera = data;

	GP_DEBUG ("file_list_func()");

	if (!check_readiness (camera, context))
		return GP_ERROR;

	return canon_int_list_directory (camera, folder, list,
					 CANON_LIST_FILES, context);
}

static int
folder_list_func (CameraFilesystem __unused__ *fs, const char *folder,
		  CameraList *list, void *data,
		  GPContext *context)
{
	Camera *camera = data;

	GP_DEBUG ("folder_list_func()");

	if (!check_readiness (camera, context))
		return GP_ERROR;

	return canon_int_list_directory (camera, folder, list,
					 CANON_LIST_FOLDERS, context);
}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *user_data,
	       GPContext *context)
{
	Camera *camera = user_data;
	unsigned char *data = NULL, *thumbdata = NULL;
	const char *thumbname = NULL;
	const char *audioname = NULL;
	int ret;
	unsigned int datalen;
	char canon_path[300];

	/* for debugging */
	char *filetype;
	char buf[32];

	/* Assemble complete canon path into canon_path */
	ret = snprintf (canon_path, sizeof (canon_path) - 3, "%s\\%s",
			gphoto2canonpath (camera, folder, context), filename);
	if (ret < 0) {
		gp_context_error (context,
				  _("Internal error #1 in get_file_func() (%s line %i)"),
				  __FILE__, __LINE__);
		return GP_ERROR_BAD_PARAMETERS;
	}

	/* for debugging */
	switch (type) {
	case GP_FILE_TYPE_PREVIEW: filetype = "thumbnail"; break;
	case GP_FILE_TYPE_EXIF: filetype = "exif data"; break;
	case GP_FILE_TYPE_AUDIO: filetype = "audio annotation"; break;
	case GP_FILE_TYPE_NORMAL: filetype = "file itself"; break;
	default: snprintf(buf, sizeof(buf), "unknown type %d", type); filetype = buf; break;
	}
	GP_DEBUG ("get_file_func: folder '%s' filename '%s' (i.e. '%s'), getting %s",
		  folder, filename, canon_path, filetype);

	/* preparation, if it is _AUDIO we convert the file name to
	 * the corresponding audio file name. If the name of the audio
	 * file is supplied, this step will be a no-op. */
	if (type == GP_FILE_TYPE_AUDIO) {
		audioname = canon_int_filename2audioname (camera, canon_path);
		if (audioname == NULL) {
			gp_context_error (context,
					  _("No audio file could be found for %s"),
					  canon_path);
			return(GP_ERROR_FILE_NOT_FOUND);
		}
	}

	/* fetch file/thumbnail/exif/audio/whatever */
	switch (type) {
		case GP_FILE_TYPE_NORMAL:
			ret = canon_int_get_file (camera, canon_path, &data, &datalen,
						  context);
			if (ret == GP_OK) {
				/* 0 also marks image as downloaded */
				uint8_t attr = 0;

				/* This should cover all attribute
				 * bits known of and reflected in
				 * info.file
				 */
				CameraFileInfo info;

				gp_filesystem_get_info (fs, folder, filename, &info, context);
				if ((info.file.permissions & GP_FILE_PERM_DELETE) == 0)
					attr |= CANON_ATTR_WRITE_PROTECTED;
				canon_int_set_file_attributes (camera, filename,
							       gphoto2canonpath (camera,
										 folder,
										 context),
							       attr, context);

				/* we cannot easily retrieve it except by reading the directory */
				if (info.file.fields & GP_FILE_INFO_MTIME)
					gp_file_set_mtime (file, info.file.mtime);
			}
			break;

		case GP_FILE_TYPE_AUDIO:
			if (*audioname != '\0') {
				/* extra audio file */
				ret = canon_int_get_file (camera, audioname, &data, &datalen,
							  context);
			} else {
				/* internal audio file; not handled yet */
				ret = GP_ERROR_NOT_SUPPORTED;
			}
			break;

		case GP_FILE_TYPE_PREVIEW:
			thumbname = canon_int_filename2thumbname (camera, canon_path);
			if (thumbname == NULL) {
				/* no thumbnail available */
				GP_DEBUG ("%s is a file type for which no thumbnail is provided",canon_path);
				return (GP_ERROR_NOT_SUPPORTED);
			}
#ifdef HAVE_LIBEXIF
			/* Check if we have libexif, it is a jpeg file
			 * and it is not a PowerShot Pro 70 (which
			 * does not support EXIF), return not
			 * supported here so that gPhoto2 query for
			 * GP_FILE_TYPE_EXIF instead
			 */
			if (is_jpeg (filename)) {
				if (camera->pl->md->model != CANON_CLASS_2) {
					GP_DEBUG ("get_file_func: preview requested where "
						  "EXIF should be possible");
					return (GP_ERROR_NOT_SUPPORTED);
				}
			}
#endif /* HAVE_LIBEXIF */
			if (*thumbname == '\0') {
				/* file internal thumbnail */
				ret = canon_int_get_thumbnail (camera, canon_path, &data,
							       &datalen, context);
			} else {
				/* extra thumbnail file */
				ret = canon_int_get_file (camera, thumbname, &data, &datalen,
							  context);
			}
			break;

		case GP_FILE_TYPE_EXIF:
#ifdef HAVE_LIBEXIF
			/* the PowerShot Pro 70 does not support EXIF */
			if (camera->pl->md->model == CANON_CLASS_2)
				return (GP_ERROR_NOT_SUPPORTED);

			thumbname = canon_int_filename2thumbname (camera, canon_path);
			if (thumbname == NULL) {
				/* no thumbnail available */
				GP_DEBUG ("%s is a file type for which no thumbnail is provided", canon_path);
				return (GP_ERROR_NOT_SUPPORTED);
			}

			if (*thumbname == '\0') {
				/* file internal thumbnail with EXIF data */
				ret = canon_int_get_thumbnail (camera, canon_path, &data,
							       &datalen, context);
			} else {
				/* extra thumbnail file */
				ret = canon_int_get_file (camera, thumbname, &data, &datalen,
							  context);
			}
#else
			GP_DEBUG ("get_file_func: EXIF file type requested but no libexif");
			return (GP_ERROR_NOT_SUPPORTED);
#endif /* HAVE_LIBEXIF */
			break;

		default:
			GP_DEBUG ("get_file_func: unsupported file type %i", type);
			return (GP_ERROR_NOT_SUPPORTED);
	}

	if (ret != GP_OK) {
		GP_DEBUG ("get_file_func: getting image data failed, returned %i", ret);
		return ret;
	}

	if (data == NULL) {
		GP_DEBUG ("get_file_func: Fatal error: data == NULL");
		return GP_ERROR_CORRUPTED_DATA;
	}
	/* 256 is picked out of the blue, I figured no JPEG with EXIF header
	 * (not all canon cameras produces EXIF headers I think, but still)
	 * should be less than 256 bytes long.
	 */
	if (datalen < 256) {
		GP_DEBUG ("get_file_func: datalen < 256 (datalen = %i = 0x%x)", datalen,
			  datalen);
		return GP_ERROR_CORRUPTED_DATA;
	}

	/* do different things with the data fetched above */
	switch (type) {
		case GP_FILE_TYPE_PREVIEW:
			/* Either this camera model does not support EXIF,
			 * this is not a file known to contain EXIF thumbnails
			 * (movies for example) or we do not have libexif.
			 * Try to extract thumbnail by looking for JPEG start/end
			 * in data.
			 */
			ret = canon_int_extract_jpeg_thumb (data, datalen, &thumbdata, &datalen, context);

			if (thumbdata != NULL) {
				/* free old data */
				free (data);
				/* make data point to extracted thumbnail data */
				data = thumbdata;
				thumbdata = NULL;
			}
			if (ret != GP_OK) {
				GP_DEBUG ("get_file_func: GP_FILE_TYPE_PREVIEW: couldn't extract JPEG thumbnail data");
				if (data)
					free (data);
				data = NULL;
				return ret;
			}
			GP_DEBUG ("get_file_func: GP_FILE_TYPE_PREVIEW: extracted thumbnail data (%i bytes)", datalen);

			gp_file_set_data_and_size (file, (char *)data, datalen);
			gp_file_set_mime_type (file, GP_MIME_JPEG);	/* always */
			break;

		case GP_FILE_TYPE_AUDIO:
			gp_file_set_mime_type (file, GP_MIME_WAV);
			gp_file_set_data_and_size (file, (char *)data, datalen);
			break;

		case GP_FILE_TYPE_NORMAL:
			gp_file_set_mime_type (file, filename2mimetype (filename));
			gp_file_set_data_and_size (file, (char *)data, datalen);
			break;
#ifdef HAVE_LIBEXIF
		case GP_FILE_TYPE_EXIF:
			if ( !is_cr2 ( filename ) )
				gp_file_set_mime_type (file, GP_MIME_JPEG);
			else
				gp_file_set_mime_type (file, GP_MIME_EXIF);
			gp_file_set_data_and_size (file, (char *)data, datalen);
			break;
#endif /* HAVE_LIBEXIF */
		default:
			free (data);
			data = NULL;
			return (GP_ERROR_NOT_SUPPORTED);
	}

	return GP_OK;
}

/****************************************************************************/


static void
pretty_number (int number, char *buffer)
{
	int len, tmp, digits;
	char *pos;

#ifdef HAVE_LOCALE_H
	/* We should really use ->grouping as well */
	char thousands_sep = *localeconv ()->thousands_sep;

	if (thousands_sep == '\0')
		thousands_sep = '\'';
#else
	const char thousands_sep = '\'';
#endif
	len = 0;

	tmp = number;
	do {
		len++;
		tmp /= 10;
	}
	while (tmp);
	len += (len - 1) / 3;
	pos = buffer + len;
	*pos = 0;
	digits = 0;
	do {
		*--pos = (number % 10) + '0';
		number /= 10;
		if (++digits == 3) {
			*--pos = thousands_sep;
			digits = 0;
		}
	}
	while (number);
}

static int
camera_summary (Camera *camera, CameraText *summary, GPContext *context)
{
	char a[20], b[20];
	int pwr_source, pwr_status;
	int res;
	char disk_str[128], power_str[128], time_str[128];
	time_t camera_time, local_time, tmp_time;
	struct tm *tm;
	double time_diff;
	char formatted_camera_time[20];

	GP_DEBUG ("camera_summary()");

	if (!check_readiness (camera, context))
		return GP_ERROR;

	/*clear_readiness(); */
	if (!update_disk_cache (camera, context))
		return GP_ERROR;

	pretty_number (camera->pl->cached_capacity, a);
	pretty_number (camera->pl->cached_available, b);

	snprintf (disk_str, sizeof (disk_str),
		  _("  Drive %s\n  %11s bytes total\n  %11s bytes available"),
		  camera->pl->cached_drive, a, b);

	res = canon_get_batt_status (camera, &pwr_status, &pwr_source, context);
	if (res == GP_OK) {
		if (pwr_status == CAMERA_POWER_OK || pwr_status == CAMERA_POWER_BAD)
			snprintf (power_str, sizeof (power_str), "%s (%s)",
				  ((pwr_source & CAMERA_MASK_BATTERY) ==
				   0) ? _("AC adapter") : _("on battery"),
				  pwr_status ==
				  CAMERA_POWER_OK ? _("power OK") : _("power bad"));
		else
			snprintf (power_str, sizeof (power_str), "%s - %i",
				  ((pwr_source & CAMERA_MASK_BATTERY) ==
				   0) ? _("AC adapter") : _("on battery"), pwr_status);
	} else {
		GP_DEBUG ("canon_get_batt_status() returned error: %s (%i), ",
			  gp_result_as_string (res), res);
		snprintf (power_str, sizeof (power_str), _("not available: %s"),
			  gp_result_as_string (res));
	}

	canon_int_set_time (camera, time (NULL), context);
	res = canon_int_get_time (camera, &camera_time, context);

	/* it's hard to get local time with DST portably... */
	tmp_time = time (NULL);
	tm = localtime (&tmp_time);
#ifdef HAVE_TM_GMTOFF
	local_time = tmp_time + tm->tm_gmtoff;
	GP_DEBUG ("camera_summary: converted %ld to localtime %ld (tm_gmtoff is %ld)",
		  (long)tmp_time, (long)local_time, (long)tm->tm_gmtoff);
#else
	local_time = tmp_time - timezone;
	GP_DEBUG ("camera_summary: converted %ld to localtime %ld (timezone is %ld)",
		  (long)tmp_time, (long)local_time, (long)timezone);
#endif

	if (res == GP_OK) {
		time_diff = difftime (camera_time, local_time);

		strftime (formatted_camera_time, sizeof (formatted_camera_time),
			  "%Y-%m-%d %H:%M:%S", gmtime (&camera_time));

		snprintf (time_str, sizeof (time_str), _("%s (host time %s%i seconds)"),
			  formatted_camera_time, time_diff >= 0 ? "+" : "", (int) time_diff);
	} else {
		GP_DEBUG ("canon_int_get_time() returned negative result: %s (%li)",
			  gp_result_as_string ((int) camera_time), (long) camera_time);
		snprintf (time_str, sizeof (time_str), ("not available: %s"),
			  gp_result_as_string ((int) camera_time));
	}

	sprintf (summary->text,
		 _("\nCamera identification:\n  Model: %s\n  Owner: %s\n\n"
		   "Power status: %s\n\n"
		   "Flash disk information:\n%s\n\n"
		   "Time: %s\n"),
		 camera->pl->md->id_str, camera->pl->owner, power_str, disk_str, time_str);

	return GP_OK;
}

static int
storage_info_func (
	CameraFilesystem *fs,
	CameraStorageInformation **sinfos, int *nrofsinfos,
	void *data, GPContext *context
) {
	char root[10];
	Camera *camera = (Camera*)data;

	if (!check_readiness (camera, context))
		return GP_ERROR_IO;

	camera->pl->cached_drive = canon_int_get_disk_name (camera, context);
	if (!camera->pl->cached_drive) {
		gp_context_error (context, _("Could not get disk name: %s"),
				  _("No reason available"));
		return GP_ERROR_IO;
	}
	snprintf (root, sizeof (root), "%s\\", camera->pl->cached_drive);
	canon_int_get_disk_name_info (camera, root, &camera->pl->cached_capacity,
				    &camera->pl->cached_available, context);

	*sinfos = (CameraStorageInformation*) calloc (sizeof (CameraStorageInformation), 1);
	*nrofsinfos = 1;
	(*sinfos)->fields = GP_STORAGEINFO_BASE;
	strcpy ((*sinfos)->basedir, "/");
	if (camera->pl->cached_drive) {
		(*sinfos)->fields = GP_STORAGEINFO_LABEL;
		strcpy ((*sinfos)->basedir, camera->pl->cached_drive);
	}
	(*sinfos)->fields |= GP_STORAGEINFO_MAXCAPACITY;
	(*sinfos)->capacitykbytes = camera->pl->cached_capacity;
	(*sinfos)->fields |= GP_STORAGEINFO_FREESPACEKBYTES;
	(*sinfos)->freekbytes = camera->pl->cached_available;
	(*sinfos)->fields |= GP_STORAGEINFO_ACCESS;
	(*sinfos)->access = GP_STORAGEINFO_AC_READONLY_WITH_DELETE;
	return GP_OK;
}


/****************************************************************************/

static int
camera_about (Camera __unused__ *camera, CameraText *about,
	      GPContext __unused__ *context)
{
	GP_DEBUG ("camera_about()");

	strcpy (about->text,
		_("Canon PowerShot series driver by\n"
		  " Wolfgang G. Reissnegger,\n"
		  " Werner Almesberger,\n"
		  " Edouard Lafargue,\n"
		  " Philippe Marzouk,\n"
		  "A5 additions by Ole W. Saastad\n"
		  "Additional enhancements by\n"
		  " Holger Klemm\n"
		  " Stephen H. Westin")
		);

	return GP_OK;
}

/****************************************************************************/

static int
delete_file_func (CameraFilesystem __unused__ *fs, const char *folder,
		  const char *filename, void *data,
		  GPContext *context)
{
	Camera *camera = data;
	const char *thumbname;
	char canonfolder[300];
	const char *_canonfolder;

	GP_DEBUG ("delete_file_func()");

	_canonfolder = gphoto2canonpath (camera, folder, context);
	strncpy (canonfolder, _canonfolder, sizeof (canonfolder) - 1);
	canonfolder[sizeof (canonfolder) - 1] = 0;

	if (!check_readiness (camera, context))
		return GP_ERROR;

	if (camera->pl->md->model == CANON_CLASS_3) {
		GP_DEBUG ("delete_file_func: deleting "
			  "pictures disabled for cameras: PowerShot A5, PowerShot A5 ZOOM");

		return GP_ERROR_NOT_SUPPORTED;
	}

	GP_DEBUG ("delete_file_func: filename: %s, folder: %s", filename, canonfolder);
	if (canon_int_delete_file (camera, filename, canonfolder, context) != GP_OK) {
		gp_context_error (context, _("Error deleting file"));
		return GP_ERROR;
	}


	/* If we have a file with associated thumbnail file that is not
	 * listed, delete its thumbnail as well */
	if (!camera->pl->list_all_files) {
		thumbname = canon_int_filename2thumbname (camera, filename);
		if ((thumbname != NULL) && (*thumbname != '\0')) {
			GP_DEBUG ("delete_file_func: thumbname: %s, folder: %s", thumbname, canonfolder);
			if (canon_int_delete_file (camera, thumbname, canonfolder, context) != GP_OK) {
				/* XXX should we handle this as an error?
				 * Probably only in case the camera link died,
				 * but not if the file just had already been
				 * deleted before. */
				gp_context_error (context, _("Error deleting associated thumbnail file"));
				return GP_ERROR;
			}
		}
	}
	return GP_OK;
}

#ifdef CANON_EXPERIMENTAL_UPLOAD
/*
 * get from the filesystem the name of the highest numbered picture or directory
 *
 */
static int
get_last_file (Camera *camera, char *directory, char *destname,
	       GPContext* context, char getdirectory, char*result)
{
	CameraFilesystem * fs = camera->fs;
	CameraList*list;
	char dir2[300];
	int t;
	GP_DEBUG ("get_last_file()");

	if(directory[1] == ':') {
	    /* gp_file_list_folder() needs absolute filenames
	       starting with '/' */
	    GP_DEBUG ("get_last_file(): replacing '%c:%c' by '/'",
		      directory[0], directory[2]);
	    sprintf(dir2, "/%s", &directory[3]);
	    directory = dir2;
	}

	gp_list_new(&list);
	if(getdirectory) {
	    CHECK_RESULT (gp_filesystem_list_folders (fs, directory, list, context));
	    GP_DEBUG ("get_last_file(): %d folders", list->count);
	}
	else {
	    CHECK_RESULT (gp_filesystem_list_files (fs, directory, list, context));
	    GP_DEBUG ("get_last_file(): %d files", list->count);
	}
	for(t=0;t<list->count;t++)
	{
	    char* name = list->entry[t].name;
	    if(getdirectory)
		GP_DEBUG ("get_last_file(): folder: %s", name);
	    else
		GP_DEBUG ("get_last_file(): file: %s", name);

	    /* we search only for directories of the form [0-9]{3}CANON... */
	    if(getdirectory && ((strlen (name)!=8) ||
		    (!isdigit(name[0]) ||
		     !isdigit(name[1]) ||
		     !isdigit(name[2])) || strcmp(&name[3],"CANON")))
		continue;

	    /* ...and only for files similar to AUT_[0-9]{4} */
	    if(!getdirectory && (!isdigit(name[6]) ||
			         !isdigit(name[7])))
		continue;

	    if(!result[0] || strcmp((list->entry[t].name)+4, result+4) > 0)
		strcpy(result, list->entry[t].name);
	}
	gp_list_free(list);

	return GP_OK;
}

static int
get_last_picture (Camera *camera, char *directory, char *destname, GPContext* context)
{
    GP_DEBUG ("get_last_picture()");
    return get_last_file(camera, directory, destname, context, 0, destname);
}

static int
get_last_dir (Camera *camera, char *directory, char *destname, GPContext* context)
{
    GP_DEBUG ("get_last_dir()");
    return get_last_file(camera, directory, destname, context, 1, destname);
}

/*
 * convert a filename to 8.3 format by truncating
 * the basename to 8 and the extension to 3 chars
 *
 */
static void
convert_filename_to_8_3(const char* filename, char* dest)
{
    char*c;
    GP_DEBUG ("convert_filename_to_8_3()");
    c = strchr(filename, '.');
    if(!c) {
	sprintf(dest, "%.8s", filename);
    }
    else {
	int l = c-filename;
	if(l>8)
	    l=8;
	memcpy(dest, filename, l);
	dest[l]=0;
	strcat(dest, c);
	dest[l+4]=0;
    }
}


/* XXX This function should be merged with the other one of the same name */
static int
put_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *data,
	       GPContext *context)
{
	Camera *camera = data;
	char destpath[300], destname[300], dir[300], dcf_root_dir[10];
	int j, dirnum = 0, r, status;
	char buf[10];
	CameraAbilities a;

	GP_DEBUG ("camera_folder_put_file()");

	if (type != GP_FILE_TYPE_NORMAL)
		return GP_ERROR_BAD_PARAMETERS;

	if (!check_readiness (camera, context))
		return GP_ERROR;

	gp_camera_get_abilities (camera, &a);
	/* Special case for A50 and Pro 70 */
	if ((camera->pl->speed > 57600) && ((camera->pl->md->model == CANON_CLASS_1)
					    || (camera->pl->md->model == CANON_CLASS_2))) {
		gp_context_error (context,
				  _("Speeds greater than 57600 are not supported for uploading to this camera"));
		return GP_ERROR_NOT_SUPPORTED;
	}

	if (!check_readiness (camera, context)) {
		return GP_ERROR;
	}

	for (j = 0; j < sizeof (destpath); j++) {
		destpath[j] = '\0';
		dir[j] = '\0';
		destname[j] = '\0';
	}

	if (camera->pl->cached_drive == NULL) {
		camera->pl->cached_drive = canon_int_get_disk_name (camera, context);
		if (camera->pl->cached_drive == NULL) {
			gp_context_error (context, _("Could not get flash drive letter"));
			return GP_ERROR;
		}
	}

	sprintf (dcf_root_dir, "%s\\DCIM", camera->pl->cached_drive);
	GP_DEBUG ("camera_folder_put_file(): dcf_root_dir=%s",dcf_root_dir);

	if (get_last_dir (camera, dcf_root_dir, dir, context) != GP_OK)
		return GP_ERROR;
	GP_DEBUG ("camera_folder_put_file(): dir=%s", dir?dir:"NULL");

	if (strlen (dir) == 0) {
		sprintf (dir, "100CANON");
		sprintf (destname, "AUT_0001.JPG");
		sprintf (destpath, "%s\\%s", dcf_root_dir, dir);
	} else {
		if(camera->pl->upload_keep_filename) {
		    char filename2[300];
		    if(!filename)
			return GP_ERROR;
		    convert_filename_to_8_3(filename, filename2);
		    sprintf(destname, "%s", filename2);
		} else {
		    char dir2[300];
		    sprintf(dir2, "%s/%s", dcf_root_dir, dir);
		    status = get_last_picture (camera, dir2, destname, context);
		    if ( status < 0 )
			    return status;

		    if (strlen (destname) == 0) {
			    sprintf (destname, "AUT_%c%c01.JPG", dir[1], dir[2]);
		    } else {
			    sprintf (buf, "%c%c", destname[6], destname[7]);
			    j = 1;
			    j = atoi (buf);
			    if (j == 99) {
				    j = 1;
				    sprintf (buf, "%c%c%c", dir[0], dir[1], dir[2]);
				    dirnum = atoi (buf);
				    if (dirnum == 999 && !camera->pl->upload_keep_filename) {
					    gp_context_error (context,
							      _("Could not upload, no free folder name available!\n"
							       "999CANON folder name exists and has an AUT_9999.JPG picture in it."));
					    return GP_ERROR;
				    } else {
					    dirnum++;
					    sprintf (dir, "%03iCANON", dirnum);
				    }
			    } else
				    j++;

			    sprintf (destname, "AUT_%c%c%02i.JPG", dir[1], dir[2], j);
		    }
		}
		sprintf (destpath, "%s\\%s", dcf_root_dir, dir);
		GP_DEBUG ("destpath: %s destname: %s", destpath, destname);
	}

	GP_DEBUG ("camera_folder_put_file(): destpath=%s", destpath);
	GP_DEBUG ("camera_folder_put_file(): destname=%s", destname);

	r = canon_int_directory_operations (camera, dcf_root_dir, DIR_CREATE, context);
	if (r < 0) {
		gp_context_error (context, _("Could not create \\DCIM directory."));
		return (r);
	}

	r = canon_int_directory_operations (camera, destpath, DIR_CREATE, context);
	if (r < 0) {
		gp_context_error (context, _("Could not create destination directory."));
		return (r);
	}

	j = strlen (destpath);
	destpath[j] = '\\';
	destpath[j + 1] = '\0';

	clear_readiness (camera);

	return canon_int_put_file (camera, file, filename, destname, destpath, context);
}

#else /* not CANON_EXPERIMENTAL_UPLOAD */

static int
put_file_func (CameraFilesystem __unused__ *fs, const char __unused__ *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *data,
	       GPContext *context)
{
	Camera *camera = data;
	char destpath[300], destname[300], dir[300], dcf_root_dir[10];
	unsigned int j;
	int dirnum = 0, r;
	char buf[10];
	CameraAbilities a;

	GP_DEBUG ("camera_folder_put_file()");
	if (type != GP_FILE_TYPE_NORMAL)
		return GP_ERROR_BAD_PARAMETERS;

	if (camera->port->type == GP_PORT_USB) {
		gp_context_error (context,
				  "File upload not implemented for USB yet");
		return GP_ERROR_NOT_SUPPORTED;
	}

	if (!check_readiness (camera, context))
		return GP_ERROR;

	gp_camera_get_abilities (camera, &a);
	/* Special case for A50 and Pro 70 */
	if ((camera->pl->speed > 57600) &&
	    ((camera->pl->md->model == CANON_CLASS_1)
	     || (camera->pl->md->model == CANON_CLASS_2))) {
		gp_context_error (context,
				  _("Speeds greater than 57600 are not "
				    "supported for uploading to this camera"));
		return GP_ERROR_NOT_SUPPORTED;
	}

	if (!check_readiness (camera, context)) {
		return GP_ERROR;
	}

	for (j = 0; j < sizeof (destpath); j++) {
		destpath[j] = '\0';
		dir[j] = '\0';
		destname[j] = '\0';
	}

	if (camera->pl->cached_drive == NULL) {
		camera->pl->cached_drive = canon_int_get_disk_name (camera, context);
		if (camera->pl->cached_drive == NULL) {
			gp_context_error (context, _("Could not get flash drive letter"));
			return GP_ERROR;
		}
	}

	sprintf (dcf_root_dir, "%s\\DCIM", camera->pl->cached_drive);

	if (strlen (dir) == 0) {
		sprintf (dir, "\\100CANON");
		sprintf (destname, "AUT_0001.JPG");
	} else {
		if (strlen (destname) == 0) {
			sprintf (destname, "AUT_%c%c01.JPG", dir[2], dir[3]);
		} else {
			sprintf (buf, "%c%c", destname[6], destname[7]);
			j = 1;
			j = atoi (buf);
			if (j == 99) {
				j = 1;
				sprintf (buf, "%c%c%c", dir[1], dir[2], dir[3]);
				dirnum = atoi (buf);
				if (dirnum == 999) {
					gp_context_error (context,
							  _("Could not upload, no free folder name available!\n"
							   "999CANON folder name exists and has an AUT_9999.JPG picture in it."));
					return GP_ERROR;
				} else {
					dirnum++;
					sprintf (dir, "\\%03iCANON", dirnum);
				}
			} else
				j++;

			sprintf (destname, "AUT_%c%c%02i.JPG", dir[2], dir[3], j);
		}

		sprintf (destpath, "%s%s", dcf_root_dir, dir);

		GP_DEBUG ("destpath: %s destname: %s", destpath, destname);
	}

	r = canon_int_directory_operations (camera, dcf_root_dir, DIR_CREATE, context);
	if (r < 0) {
		gp_context_error (context, _("Could not create \\DCIM directory."));
		return (r);
	}

	r = canon_int_directory_operations (camera, destpath, DIR_CREATE, context);
	if (r < 0) {
		gp_context_error (context, _("Could not create destination directory."));
		return (r);
	}


	j = strlen (destpath);
	destpath[j] = '\\';
	destpath[j + 1] = '\0';

	clear_readiness (camera);

	return canon_int_put_file (camera, file, filename, destname, destpath, context);
}

#endif /* CANON_EXPERIMENTAL_UPLOAD */



/****************************************************************************/

/* Wait for an event */
static int
camera_wait_for_event (Camera *camera, int timeout, CameraEventType *eventtype, void **eventdata, GPContext *context)
{
	return canon_int_wait_for_event (camera, timeout, eventtype, eventdata, context);
}


/* Get configuration into screen widgets for display. */

static int
camera_get_config (Camera *camera, CameraWidget **window, GPContext *context)
{
	CameraWidget *t, *section;
	char power_str[128], firm[64];

	int pwr_status, pwr_source, res, i, menuval;
	time_t camtime;
	char formatted_camera_time[30];
	float zoom;
	unsigned char zoomVal, zoomMax;

	GP_DEBUG ("camera_get_config()");

	if (!check_readiness (camera, context))
		return -1;

	gp_widget_new (GP_WIDGET_WINDOW, _("Camera and Driver Configuration"), window);
	gp_widget_set_name (*window, "main");

	gp_widget_new (GP_WIDGET_SECTION, _("Camera Settings"), &section);
	gp_widget_set_name (section, "settings");
	gp_widget_append (*window, section);

	gp_widget_new (GP_WIDGET_TEXT, _("Owner Name"), &t);
	gp_widget_set_name (t, "ownername");
	gp_widget_set_value (t, camera->pl->owner);
	gp_widget_append (section, t);

	/* Capture size class */
	gp_widget_new (GP_WIDGET_MENU, _("Capture Size Class"), &t);
	gp_widget_set_name (t, "capturesizeclass");

	/* Map it to the list of choices */
	i = 0;
	menuval = -1;
	while (captureSizeArray[i].label) {
		gp_widget_add_choice (t, _(captureSizeArray[i].label));
		if (camera->pl->capture_size == captureSizeArray[i].value) {
			gp_widget_set_value (t, _(captureSizeArray[i].label));
			menuval = i;
		}
		i++;
	}

	/* Set to "Compatibility mode" if not currently set */
	if (menuval == -1)
		gp_widget_set_value (t, _("Compatibility Mode"));

	gp_widget_append (section, t);


	/********************* Release params **********************/

	/* Necessary for release params: ISO, aperture, etc. */
	if (!camera->pl->remote_control) {
		res = canon_int_start_remote_control (camera, context);
		if (res != GP_OK)
			return -1;
	}

	res = canon_int_get_release_params(camera, context);
	if (res != GP_OK) {
		for (i = 0 ; i < RELEASE_PARAMS_LEN ; i++)
			camera->pl->release_params[i] = -1;
	}


	/* ISO speed */
	gp_widget_new (GP_WIDGET_MENU, _("ISO Speed"), &t);
	gp_widget_set_name (t, "iso");

	/* Map ISO speed to the list of choices */
	i = 0;
	menuval = -1;
	while (isoStateArray[i].label) {
		gp_widget_add_choice (t, isoStateArray[i].label);
		if (camera->pl->release_params[ISO_INDEX] ==
		    (int)isoStateArray[i].value) {
			gp_widget_set_value (t, isoStateArray[i].label);
			menuval = i;
		}
		i++;
	}

	/* Set an unknown ISO value if the
	 * camera is set to something weird */
	if (menuval == -1) {
		gp_widget_add_choice (t, _("Unknown"));
		gp_widget_set_value (t, _("Unknown"));
	};

	gp_widget_append (section, t);


	/* Shooting mode */
	gp_widget_new (GP_WIDGET_MENU, _("Shooting mode"), &t);
	gp_widget_set_name (t, "shootingmode");

	/* Map shooting mode to the list of choices */
	i = 0;
	menuval = -1;
	while (shootingModeStateArray[i].label) {
		gp_widget_add_choice (t, _(shootingModeStateArray[i].label));
		if (camera->pl->release_params[SHOOTING_MODE_INDEX] ==
		    (int)shootingModeStateArray[i].value) {
			gp_widget_set_value (t, _(shootingModeStateArray[i].label));
			menuval = i;
		}
		i++;
	}

	/* Set an unknown shooting mode value if the
	 * camera is set to something weird */
	if (menuval == -1) {
		gp_widget_add_choice (t, _("Unknown"));
		gp_widget_set_value (t, _("Unknown"));
	};

	gp_widget_append (section, t);


	/* Shutter speed */
	gp_widget_new (GP_WIDGET_MENU, _("Shutter Speed"), &t);
	gp_widget_set_name (t, "shutterspeed");

	/* Map shutter speed to the list of choices */
	i = 0;
	menuval = -1;
	while (shutterSpeedStateArray[i].label) {
		gp_widget_add_choice (t, _(shutterSpeedStateArray[i].label));
		if (camera->pl->release_params[SHUTTERSPEED_INDEX] ==
		    (int)shutterSpeedStateArray[i].value) {
			gp_widget_set_value (t, _(shutterSpeedStateArray[i].label));
			menuval = i;
		}
		i++;
	}

	/* Set an unknown shutter value if the
	 * camera is set to something weird */
	if (menuval == -1) {
		gp_widget_add_choice (t, _("Unknown"));
		gp_widget_set_value (t, _("Unknown"));
	};

	gp_widget_append (section, t);



	/* DSLRs have only "Manual" Zoom */
        if ( camera->pl->md->id_str && !strstr(camera->pl->md->id_str,"EOS") && !strstr(camera->pl->md->id_str,"Rebel")) {
		/* Zoom level */
		gp_widget_new (GP_WIDGET_RANGE, _("Zoom"), &t);
		gp_widget_set_name (t, "zoom");
		canon_int_get_zoom(camera, &zoomVal, &zoomMax, context);
		gp_widget_set_range (t, 0, zoomMax, 1);
		zoom = zoomVal;
		gp_widget_set_value (t, &zoom);
		gp_widget_append (section, t);
	}

	/* Aperture */
	gp_widget_new (GP_WIDGET_MENU, _("Aperture"), &t);
	gp_widget_set_name (t, "aperture");

	/* Map aperture to the list of choices */
	i = 0;
	menuval = -1;
	while (apertureStateArray[i].label) {
		gp_widget_add_choice (t, _(apertureStateArray[i].label));
		if (camera->pl->release_params[APERTURE_INDEX] ==
		    (int)apertureStateArray[i].value) {
			gp_widget_set_value (t, _(apertureStateArray[i].label));
			menuval = i;
		}
		i++;
	}

	/* Set an unknown aperture value if the
	 * camera is set to something weird */
	if (menuval == -1) {
		gp_widget_add_choice (t, _("Unknown"));
		gp_widget_set_value (t, _("Unknown"));
	};

	gp_widget_append (section, t);

	/* Exposure Compensation */
	gp_widget_new (GP_WIDGET_MENU, _("Exposure Compensation"), &t);
	gp_widget_set_name (t, "exposurecompensation");


	/* Map exposure compensation to the list of choices */
	i = 0;
	menuval = -1;
	while (exposureBiasStateArray[i].label) {
		gp_widget_add_choice (t, _(exposureBiasStateArray[i].label));
		if (camera->pl->release_params[EXPOSUREBIAS_INDEX] ==
		    ((camera->pl->md->id_str && !strstr(camera->pl->md->id_str,"EOS") && !strstr(camera->pl->md->id_str,"Rebel")) ? exposureBiasStateArray[i].valuePS : exposureBiasStateArray[i].valueEOS)) {
			gp_widget_set_value (t, _(exposureBiasStateArray[i].label));
			menuval = i;
		}
		i++;
	}

	/* Set an unknown exp bias value if the
	 * camera is set to something weird */
	if (menuval == -1) {
		gp_widget_add_choice (t, _("Unknown"));
		gp_widget_set_value (t, _("Unknown"));
	};
	gp_widget_append (section, t);


	/* Image Format */
	gp_widget_new (GP_WIDGET_MENU, _("Image Format"), &t);
	gp_widget_set_name (t, "imageformat");


	/* Map image format to the list of choices */
	i = 0;
	menuval = -1;
	while (imageFormatStateArray[i].label) {
		gp_widget_add_choice (t, _(imageFormatStateArray[i].label));
		if ((camera->pl->release_params[IMAGE_FORMAT_1_INDEX] ==
		     imageFormatStateArray[i].res_byte1) &&
		    (camera->pl->release_params[IMAGE_FORMAT_2_INDEX] ==
		     imageFormatStateArray[i].res_byte2) &&
		    (camera->pl->release_params[IMAGE_FORMAT_3_INDEX] ==
		     imageFormatStateArray[i].res_byte3)) {
			gp_widget_set_value (t, _(imageFormatStateArray[i].label));
			menuval = i;
		}
		i++;
	}

	/* Set an unknown imageFormat value if the
	 * camera is set to something weird */
	if (menuval == -1) {
		gp_widget_add_choice (t, _("Unknown"));
		gp_widget_set_value (t, _("Unknown"));
	};

	gp_widget_append (section, t);


	/* Focus Mode */
	gp_widget_new (GP_WIDGET_MENU, _("Focus Mode"), &t);
	gp_widget_set_name (t, "focusmode");


	/* Map focus to the list of choices */
	i = 0;
	menuval = -1;
	while (focusModeStateArray[i].label) {
		gp_widget_add_choice (t, _(focusModeStateArray[i].label));
		if (camera->pl->release_params[FOCUS_MODE_INDEX] ==
		    (int)focusModeStateArray[i].value) {
			gp_widget_set_value (t, _(focusModeStateArray[i].label));
			menuval = i;
		}
		i++;
	}

	/* Set an unknown focus mode value if the
	 * camera is set to something weird */
	if (menuval == -1) {
		gp_widget_add_choice (t, _("Unknown"));
		gp_widget_set_value (t, _("Unknown"));
	};

	gp_widget_append (section, t);


	/* Flash Mode */
	gp_widget_new (GP_WIDGET_MENU, _("Flash Mode"), &t);
	gp_widget_set_name (t, "flashmode");


	/* Map flash mode to the list of choices */
	i = 0;
	menuval = -1;
	while (flashModeStateArray[i].label) {
		gp_widget_add_choice (t, _(flashModeStateArray[i].label));
		if (camera->pl->release_params[FLASH_INDEX] ==
		    (int)flashModeStateArray[i].value) {
			gp_widget_set_value (t, _(flashModeStateArray[i].label));
			menuval = i;
		}
		i++;
	}

	/* Set an unknown flash value if the
	 * camera is set to something weird */
	if (menuval == -1) {
		gp_widget_add_choice (t, _("Unknown"));
		gp_widget_set_value (t, _("Unknown"));
	};

	gp_widget_append (section, t);


	/* Beep */
	gp_widget_new (GP_WIDGET_MENU, _("Beep"), &t);
	gp_widget_set_name (t, "beep");


	/* Map beep mode to the list of choices */
	i = 0;
	menuval = -1;
	while (beepModeStateArray[i].label) {
		gp_widget_add_choice (t, _(beepModeStateArray[i].label));
		if (camera->pl->release_params[BEEP_INDEX] ==
		    (int)beepModeStateArray[i].value) {
			gp_widget_set_value (t, _(beepModeStateArray[i].label));
			menuval = i;
		}
		i++;
	}

	/* Set an unknown beep value if the
	 * camera is set to something weird */
	if (menuval == -1) {
		gp_widget_add_choice (t, _("Unknown"));
		gp_widget_set_value (t, _("Unknown"));
	};

	gp_widget_append (section, t);


	/************************ end release params ************************/

	/* *** Actions */

	gp_widget_new (GP_WIDGET_SECTION, _("Camera Actions"), &section);
	gp_widget_set_name (section, "actions");
	gp_widget_append (*window, section);

	gp_widget_new (GP_WIDGET_TOGGLE, _("Synchronize camera date and time with PC"), &t);
	gp_widget_set_name (t, "syncdatetime");
	gp_widget_append (section, t);


	/* *** Status */

	gp_widget_new (GP_WIDGET_SECTION, _("Camera Status Information"), &section);
	gp_widget_set_name (section, "status");
	gp_widget_append (*window, section);

	gp_widget_new (GP_WIDGET_TEXT, _("Camera Model"), &t);
	gp_widget_set_name (t, "model");
	gp_widget_set_value (t, camera->pl->ident);
	gp_widget_set_readonly (t, 1);
	gp_widget_append (section, t);

	gp_widget_new (GP_WIDGET_TEXT, _("Date and Time"), &t);
	gp_widget_set_name (t, "datetime");
	res = canon_int_get_time (camera, &camtime, context);
	if (res == GP_OK) {
		strftime (formatted_camera_time, sizeof (formatted_camera_time),
                          "%Y-%m-%d %H:%M:%S", gmtime (&camtime));
		gp_widget_set_value (t, formatted_camera_time);
	} else {
		gp_widget_set_value (t, _("Error"));
	}
	gp_widget_set_readonly (t, 1);
	gp_widget_append (section, t);

	gp_widget_new (GP_WIDGET_TEXT, _("Firmware Version"), &t);
	gp_widget_set_name (t, "firmwareversion");
	sprintf (firm, "%i.%i.%i.%i", camera->pl->firmwrev[3], camera->pl->firmwrev[2],
		 camera->pl->firmwrev[1], camera->pl->firmwrev[0]);
	gp_widget_set_value (t, firm);
	gp_widget_set_readonly (t, 1);
	gp_widget_append (section, t);

	canon_get_batt_status (camera, &pwr_status, &pwr_source, context);

	if (pwr_status == CAMERA_POWER_OK || pwr_status == CAMERA_POWER_BAD)
		snprintf (power_str, sizeof (power_str), "%s (%s)",
			  ((pwr_source & CAMERA_MASK_BATTERY) ==
			   0) ? _("AC adapter") : _("on battery"),
			  pwr_status ==
			  CAMERA_POWER_OK ? _("power OK") : _("power bad"));
	else
		snprintf (power_str, sizeof (power_str), "%s - %i",
			  ((pwr_source & CAMERA_MASK_BATTERY) ==
			   0) ? _("AC adapter") : _("on battery"), pwr_status);

	gp_widget_new (GP_WIDGET_TEXT, _("Power"), &t);
	gp_widget_set_name (t, "power");
	gp_widget_set_value (t, power_str);
	gp_widget_set_readonly (t, 1);
	gp_widget_append (section, t);


	/* *** Driver */

	gp_widget_new (GP_WIDGET_SECTION, _("Driver"), &section);
	gp_widget_set_name (t, "driver");
	gp_widget_append (*window, section);

	gp_widget_new (GP_WIDGET_TOGGLE, _("List all files"), &t);
	gp_widget_set_name (t, "list_all_files");
	gp_widget_set_value (t, &camera->pl->list_all_files);
	gp_widget_append (section, t);

#ifdef CANON_EXPERIMENTAL_UPLOAD
	gp_widget_new (GP_WIDGET_TOGGLE, _("Keep filename on upload"), &t);
	gp_widget_set_name (t, "keep_filename_on_upload");
	gp_widget_set_value (t, &camera->pl->upload_keep_filename);
	gp_widget_append (section, t);
#endif /* CANON_EXPERIMENTAL_UPLOAD */

	return GP_OK;
}

/* Set camera configuration from screen widgets. */
static int
camera_set_config (Camera *camera, CameraWidget *window, GPContext *context)
{
	CameraWidget *w;
	char *wvalue;
	int i, val;
	int res;
	unsigned char iso, shutter_speed, aperture, focus_mode, flash_mode, beep, shooting_mode;
	char str[16];

	GP_DEBUG ("camera_set_config()");

	gp_widget_get_child_by_label (window, _("Owner Name"), &w);
	if (gp_widget_changed (w)) {
	        gp_widget_set_changed (w, 0);
		gp_widget_get_value (w, &wvalue);
		if (!check_readiness (camera, context)) {
			gp_context_status (context, _("Camera unavailable"));
		} else {
			if (canon_int_set_owner_name (camera, wvalue, context) == GP_OK)
				gp_context_status (context, _("Owner name changed"));
			else
				gp_context_status (context, _("could not change owner name"));
		}
	}

	gp_widget_get_child_by_label (window, _("Capture Size Class"), &w);
	if (gp_widget_changed (w)) {
	        gp_widget_set_changed (w, 0);
		gp_widget_get_value (w, &wvalue);

		i = 0;
		while (captureSizeArray[i].label) {
			if (strcmp (_(captureSizeArray[i].label), wvalue) == 0) {
				camera->pl->capture_size = captureSizeArray[i].value;
				gp_context_status (context, _("Capture size class changed"));
				break;
			}
			i++;
		}

		if (!captureSizeArray[i].label)
			gp_context_status (context, _("Invalid capture size class setting"));

	}

        /* Enable remote control, if not already enabled */
	if (!camera->pl->remote_control) {
		res = canon_int_start_remote_control (camera, context);
		if (res != GP_OK)
			return -1;
	}

	gp_widget_get_child_by_label (window, _("ISO Speed"), &w);
	if (gp_widget_changed (w)) {
	        gp_widget_set_changed (w, 0);
		gp_widget_get_value (w, &wvalue);
		if (!check_readiness (camera, context)) {
			gp_context_status (context, _("Camera unavailable"));
		} else {

			/* Map the menu option setting to the camera binary value */
			i = 0;
			while (isoStateArray[i].label) {
				if (strcmp (isoStateArray[i].label, wvalue) == 0) {
					iso = isoStateArray[i].value;
					break;
				}
				i++;
			}

			if (!isoStateArray[i].label) {
				gp_context_status (context, _("Invalid ISO speed setting"));
			} else {
				if (canon_int_set_iso (camera, iso, context) == GP_OK)
					gp_context_status (context, _("ISO speed changed"));
				else
					gp_context_status (context, _("Could not change ISO speed"));
			}
		}
	}


	gp_widget_get_child_by_label (window, _("Shooting mode"), &w);
	if (gp_widget_changed (w)) {
	        gp_widget_set_changed (w, 0);
		gp_widget_get_value (w, &wvalue);
		if (!check_readiness (camera, context)) {
			gp_context_status (context, _("Camera unavailable"));
		} else {

			/* Map the menu option setting to the camera binary value */
			i = 0;
			while (shootingModeStateArray[i].label) {
				if (strcmp (_(shootingModeStateArray[i].label), wvalue) == 0) {
					shooting_mode = shootingModeStateArray[i].value;
					break;
				}
				i++;
			}

			if (!shootingModeStateArray[i].label) {
				gp_context_status (context, _("Invalid shooting mode setting"));
			} else {
				if (canon_int_set_shooting_mode (camera, shooting_mode, context) == GP_OK)
					gp_context_status (context, _("Shooting mode changed"));
				else
					gp_context_status (context, _("Could not change shooting mode"));
			}
		}
	}


	gp_widget_get_child_by_label (window, _("Shutter Speed"), &w);
	if (gp_widget_changed (w)) {
	        gp_widget_set_changed (w, 0);
		gp_widget_get_value (w, &wvalue);
		if (!check_readiness (camera, context)) {
			gp_context_status (context, _("Camera unavailable"));
		} else {

			/* Map the menu option setting to the camera binary value */
			i = 0;
			while (shutterSpeedStateArray[i].label) {
				if (strcmp (_(shutterSpeedStateArray[i].label), wvalue) == 0) {
					shutter_speed = shutterSpeedStateArray[i].value;
					break;
				}
				i++;
			}

			if (!shutterSpeedStateArray[i].label) {
				gp_context_status (context, _("Invalid shutter speed setting"));
			} else {
				if (canon_int_set_shutter_speed (camera, shutter_speed, context) == GP_OK)
					gp_context_status (context, _("Shutter speed changed"));
				else
					gp_context_status (context, _("Could not change shutter speed"));
			}
		}
	}

	gp_widget_get_child_by_label (window, _("Aperture"), &w);
	if (gp_widget_changed (w)) {
	        gp_widget_set_changed (w, 0);
		gp_widget_get_value (w, &wvalue);
		if (!check_readiness (camera, context)) {
			gp_context_status (context, _("Camera unavailable"));
		} else {

			/* Map the menu option setting to the camera binary value */
			i = 0;
			while (apertureStateArray[i].label) {
				if (strcmp (_(apertureStateArray[i].label), wvalue) == 0) {
					aperture = apertureStateArray[i].value;
					break;
				}
				i++;
			}

			if (!apertureStateArray[i].label) {
				gp_context_status (context, _("Invalid aperture setting"));
			} else {
				if (canon_int_set_aperture (camera, aperture, context) == GP_OK)
					gp_context_status (context, _("Aperture changed"));
				else
					gp_context_status (context, _("Could not change aperture"));
			}
		}
	}
	gp_widget_get_child_by_label (window, _("Exposure Compensation"), &w);
	if (gp_widget_changed (w)) {
		unsigned char expbias;

	        gp_widget_set_changed (w, 0);
		gp_widget_get_value (w, &wvalue);
		if (!check_readiness (camera, context)) {
			gp_context_status (context, _("Camera unavailable"));
		} else {

			/* Map the menu option setting to the camera binary value */
			i = 0;
			while (exposureBiasStateArray[i].label) {
				if (strcmp (_(exposureBiasStateArray[i].label), wvalue) == 0) {
					expbias = ((camera->pl->md->id_str && !strstr(camera->pl->md->id_str,"EOS") && !strstr(camera->pl->md->id_str,"Rebel")) ? exposureBiasStateArray[i].valuePS : exposureBiasStateArray[i].valueEOS);
					break;
				}
				i++;
			}

			if (!exposureBiasStateArray[i].label) {
				gp_context_status (context, _("Invalid exposure compensation setting"));
			} else {
				if (canon_int_set_exposurebias (camera, expbias, context) == GP_OK)
					gp_context_status (context, _("Exposure compensation changed"));
				else
					gp_context_status (context, _("Could not change exposure compensation"));
			}
		}
	}

	gp_widget_get_child_by_label (window, _("Image Format"), &w);
	if (gp_widget_changed (w)) {
	        gp_widget_set_changed (w, 0);
		gp_widget_get_value (w, &wvalue);
		if (!check_readiness (camera, context)) {
			gp_context_status (context, _("Camera unavailable"));
		} else {

			/* Map the menu option setting to the camera binary value */
			i = 0;
			while (imageFormatStateArray[i].label) {
				if (strcmp (_(imageFormatStateArray[i].label), wvalue) == 0)
					break;

				i++;
			}

			if (!imageFormatStateArray[i].label) {
				gp_context_status (context, _("Invalid image format setting"));
			} else {
				if (canon_int_set_image_format (camera, imageFormatStateArray[i].res_byte1, imageFormatStateArray[i].res_byte2,
								  imageFormatStateArray[i].res_byte3, context) == GP_OK)
					gp_context_status (context, _("Image format changed"));
				else
					gp_context_status (context, _("Could not change image format"));
			}
		}
	}

	gp_widget_get_child_by_label (window, _("Focus Mode"), &w);
	if (gp_widget_changed (w)) {
	        gp_widget_set_changed (w, 0);
		gp_widget_get_value (w, &wvalue);
		if (!check_readiness (camera, context)) {
			gp_context_status (context, _("Camera unavailable"));
		} else {

			/* Map the menu option setting to the camera binary value */
			i = 0;
			while (focusModeStateArray[i].label) {
				if (strcmp (_(focusModeStateArray[i].label), wvalue) == 0) {
					focus_mode = focusModeStateArray[i].value;
					break;
				}
				i++;
			}

			if (!focusModeStateArray[i].label) {
				gp_context_status (context, _("Invalid focus mode setting"));
			} else {
				if (canon_int_set_focus_mode (camera, focus_mode, context) == GP_OK)
					gp_context_status (context, _("Focus mode changed"));
				else
					gp_context_status (context, _("Could not change focus mode"));
			}
		}
	}

	/* Beep */
	gp_widget_get_child_by_label (window, _("Beep"), &w);
	if (gp_widget_changed (w)) {
	        gp_widget_set_changed (w, 0);
		gp_widget_get_value (w, &wvalue);
		if (!check_readiness (camera, context)) {
			gp_context_status (context, _("Camera unavailable"));
		} else {

			/* Map the menu option setting to the camera binary value */
			i = 0;
			while (beepModeStateArray[i].label) {
				if (strcmp (_(beepModeStateArray[i].label), wvalue) == 0) {
					beep = beepModeStateArray[i].value;
					break;
				}
				i++;
			}

			if (!beepModeStateArray[i].label) {
				gp_context_status (context, _("Invalid beep mode setting"));
			} else {
				if (canon_int_set_beep (camera, beep, context) == GP_OK)
					gp_context_status (context, _("Beep mode changed"));
				else
					gp_context_status (context, _("Could not change beep mode"));
			}
		}
	}


	/* DSLRs have only "Manual" Zoom */
        if ( camera->pl->md->id_str && !strstr(camera->pl->md->id_str,"EOS") && !strstr(camera->pl->md->id_str,"Rebel")) {
		/* Zoom */
		gp_widget_get_child_by_label (window, _("Zoom"), &w);
		if (gp_widget_changed (w)) {
			float zoom;

	        	gp_widget_set_changed (w, 0);
			gp_widget_get_value (w, &zoom);
			if (!check_readiness (camera, context)) {
				gp_context_status (context, _("Camera unavailable"));
			} else {
					if (canon_int_set_zoom (camera, zoom, context) == GP_OK)
						gp_context_status (context, _("Zoom level changed"));
					else
						gp_context_status (context, _("Could not change zoom level"));
				}
			}
	}

	/* Aperture */
	gp_widget_get_child_by_label (window, _("Aperture"), &w);
	if (gp_widget_changed (w)) {
	        gp_widget_set_changed (w, 0);
		gp_widget_get_value (w, &wvalue);
		if (!check_readiness (camera, context)) {
			gp_context_status (context, _("Camera unavailable"));
		} else {

			/* Map the menu option setting to the camera binary value */
			i = 0;
			while (apertureStateArray[i].label) {
				if (strcmp (_(apertureStateArray[i].label), wvalue) == 0) {
					aperture = apertureStateArray[i].value;
					break;
				}
				i++;
			}

			if (!apertureStateArray[i].label) {
				gp_context_status (context, _("Invalid aperture setting"));
			} else {
				if (canon_int_set_aperture (camera, aperture, context) == GP_OK)
					gp_context_status (context, _("Aperture changed"));
				else
					gp_context_status (context, _("Could not change aperture"));
			}
		}
	}

	/* Flash mode */
	gp_widget_get_child_by_label (window, _("Flash Mode"), &w);
	if (gp_widget_changed (w)) {
	        gp_widget_set_changed (w, 0);
		gp_widget_get_value (w, &wvalue);
		if (!check_readiness (camera, context)) {
			gp_context_status (context, _("Camera unavailable"));
		} else {

			/* Map the menu option setting to the camera binary value */
			i = 0;
			while (flashModeStateArray[i].label) {
				if (strcmp (_(flashModeStateArray[i].label), wvalue) == 0) {
					flash_mode = flashModeStateArray[i].value;
					break;
				}
				i++;
			}

			if (!flashModeStateArray[i].label) {
				gp_context_status (context, _("Invalid flash mode setting"));
			} else {
				if (canon_int_set_flash (camera, flash_mode, context) == GP_OK)
					gp_context_status (context, _("Flash mode changed"));
				else
					gp_context_status (context, _("Could not change flash mode"));
			}
		}
	}


	gp_widget_get_child_by_label (window, _("Synchronize camera date and time with PC"), &w);
	if (gp_widget_changed (w)) {
	        gp_widget_set_changed (w, 0);
		gp_widget_get_value (w, &wvalue);
		if (!check_readiness (camera, context)) {
			gp_context_status (context, _("Camera unavailable"));
		} else {
			if (canon_int_set_time (camera, time (NULL), context) == GP_OK) {
				gp_context_status (context, _("time set"));
			} else {
				gp_context_status (context, _("could not set time"));
			}
		}
	}

	gp_widget_get_child_by_label (window, _("List all files"), &w);
	if (gp_widget_changed (w)) {
		/* XXXXX mark CameraFS as dirty */
	        gp_widget_set_changed (w, 0);
		gp_widget_get_value (w, &val);
		camera->pl->list_all_files = val;
		sprintf (str, "%d", val);
		gp_setting_set ("canon", "list_all_files", str);
		gp_widget_get_value (w, &camera->pl->list_all_files);
		GP_DEBUG ("New config value for \"List all files\" %i",
			  camera->pl->list_all_files);
	}

#ifdef CANON_EXPERIMENTAL_UPLOAD
	gp_widget_get_child_by_label (window, _("Keep filename on upload"), &w);
	if (gp_widget_changed (w)) {
	        gp_widget_set_changed (w, 0);
		gp_widget_get_value (w, &camera->pl->upload_keep_filename);
		GP_DEBUG ("New config value for \"Keep filename on upload\": %i",
			  camera->pl->upload_keep_filename);
	}
#endif /* CANON_EXPERIMENTAL_UPLOAD */

	GP_DEBUG ("done configuring camera.");

	return GP_OK;
}

static int
get_info_func (CameraFilesystem __unused__ *fs, const char *folder,
	       const char *filename,
	       CameraFileInfo * info,
	       void *data, GPContext *context)
{
	Camera *camera = data;
	GP_DEBUG ("get_info_func() called for '%s'/'%s'", folder, filename);

	info->preview.fields = GP_FILE_INFO_TYPE;

	/* thumbnails are always jpeg on Canon Cameras */
	strcpy (info->preview.type, GP_MIME_JPEG);

	/* FIXME GP_FILE_INFO_PERMISSIONS to add */
	info->file.fields = GP_FILE_INFO_TYPE;
	/* | GP_FILE_INFO_PERMISSIONS | GP_FILE_INFO_SIZE; */
	/* info->file.fields.permissions =  */

	if (is_movie (filename))
		strcpy (info->file.type, GP_MIME_AVI);
	else if (is_image (filename))
		strcpy (info->file.type, GP_MIME_JPEG);
	else if (is_audio (filename))
		strcpy (info->file.type, GP_MIME_WAV);
	else
		strcpy (info->file.type, GP_MIME_UNKNOWN);
	/* let it fill driver specific info */
	return canon_int_get_info_func (camera, folder, filename, info, context);
}

static int
make_dir_func (CameraFilesystem __unused__ *fs, const char *folder,
	       const char *name, void *data,
	       GPContext *context)
{
	Camera *camera = data;
	char gppath[2048];
	const char *canonpath;
	int r;

	GP_DEBUG ("make_dir_func folder '%s' name '%s'", folder, name);

	if (strlen (folder) > 1) {
		/* folder is something more than / */

		if (strlen (folder) + 1 + strlen (name) > sizeof (gppath) - 1) {
			GP_DEBUG ("make_dir_func: Arguments too long");
			return GP_ERROR_BAD_PARAMETERS;
		}

		sprintf (gppath, "%s/%s", folder, name);
	} else {
		if (1 + strlen (name) > sizeof (gppath) - 1) {
			GP_DEBUG ("make_dir_func: Arguments too long");
			return GP_ERROR_BAD_PARAMETERS;
		}

		sprintf (gppath, "/%s", name);
	}

	canonpath = gphoto2canonpath (camera, gppath, context);
	if (canonpath == NULL)
		return GP_ERROR_BAD_PARAMETERS;

	r = canon_int_directory_operations (camera, canonpath,
					    DIR_CREATE, context);
	if (r != GP_OK)
		return (r);

	return (GP_OK);
}

static int
remove_dir_func (CameraFilesystem __unused__ *fs, const char *folder,
		 const char *name, void *data,
		 GPContext *context)
{
	Camera *camera = data;
	char gppath[2048];
	const char *canonpath;
	int r;

	GP_DEBUG ("remove_dir_func folder '%s' name '%s'", folder, name);

	if (strlen (folder) > 1) {
		/* folder is something more than / */

		if (strlen (folder) + 1 + strlen (name) > sizeof (gppath) - 1) {
			GP_DEBUG ("make_dir_func: Arguments too long");
			return GP_ERROR_BAD_PARAMETERS;
		}

		sprintf (gppath, "%s/%s", folder, name);
	} else {
		if (1 + strlen (name) > sizeof (gppath) - 1) {
			GP_DEBUG ("make_dir_func: Arguments too long");
			return GP_ERROR_BAD_PARAMETERS;
		}

		sprintf (gppath, "/%s", name);
	}

	canonpath = gphoto2canonpath (camera, gppath, context);
	if (canonpath == NULL)
		return GP_ERROR_BAD_PARAMETERS;

	r = canon_int_directory_operations (camera, canonpath,
					    DIR_REMOVE, context);
	if (r != GP_OK)
		return (r);

	return (GP_OK);
}

/****************************************************************************/

/**
 * camera_init:
 * @camera: the camera to initialize
 * @context: a #GPContext
 *
 * This routine initializes the serial/USB port and also loads the
 * camera settings. Right now it is only the speed that is
 * saved.
 *
 * Returns: gphoto2 error code
 *
 */
static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.folder_list_func = folder_list_func,
	.get_info_func = get_info_func,
	.get_file_func = get_file_func,
	.del_file_func = delete_file_func,
	.put_file_func = put_file_func,
	.make_dir_func = make_dir_func,
	.remove_dir_func = remove_dir_func,
	.storage_info_func = storage_info_func
};

int
camera_init (Camera *camera, GPContext *context)
{
	GPPortSettings settings;
	char buf[1024];

	GP_DEBUG ("canon camera_init()");

	/* First, set up all the function pointers */
	camera->functions->exit = camera_exit;
	camera->functions->capture = camera_capture;
	camera->functions->capture_preview = camera_capture_preview;
	camera->functions->get_config = camera_get_config;
	camera->functions->set_config = camera_set_config;
	camera->functions->summary = camera_summary;
	camera->functions->manual = camera_manual;
	camera->functions->about = camera_about;
	camera->functions->wait_for_event = camera_wait_for_event;

	/* Set up the CameraFilesystem */
	gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera);
	camera->pl = calloc (1, sizeof (CameraPrivateLibrary));
	if (!camera->pl)
		return GP_ERROR_NO_MEMORY;
	camera->pl->first_init = 1;
	camera->pl->seq_tx = 1;
	camera->pl->seq_rx = 1;

	/* default to false, i.e. list only known file types, use DCIF filenames */
	if (gp_setting_get ("canon", "list_all_files", buf) == GP_OK)
		camera->pl->list_all_files = atoi(buf);
	else
		camera->pl->list_all_files = FALSE;
#ifdef CANON_EXPERIMENTAL_UPLOAD
	camera->pl->upload_keep_filename = FALSE;
#endif

	switch (camera->port->type) {
		case GP_PORT_USB:
			GP_DEBUG ("GPhoto tells us that we should use a USB link.");

			return canon_usb_init (camera, context);
			break;
		case GP_PORT_SERIAL:
			GP_DEBUG ("GPhoto tells us that we should use a RS232 link.");

			/* Figure out the speed (and set to default speed if 0) */
			gp_port_get_settings (camera->port, &settings);
			camera->pl->speed = settings.serial.speed;

			if (camera->pl->speed == 0)
				camera->pl->speed = 9600;

			GP_DEBUG ("Camera transmission speed : %i", camera->pl->speed);

			return canon_serial_init (camera);
			break;
		default:
			gp_context_error (context,
					  _("Unsupported port type %i = 0x%x given. "
					    "Initialization impossible."), camera->port->type,
					  camera->port->type);
			return GP_ERROR_NOT_SUPPORTED;
			break;
	}

	/* NOT REACHED */
	return GP_ERROR;
}

/*
 * Local Variables:
 * c-file-style:"linux"
 * indent-tabs-mode:t
 * End:
 */
