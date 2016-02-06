/* library.c
 *
 * Copyright 2002 Lutz Mueller <lutz@users.sourceforge.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details. 
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#define _BSD_SOURCE

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-port-log.h>

#include "ricoh.h"

#define GP_MODULE "ricoh"

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
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define _(String) (String)
#  define N_(String) (String)
#endif

#define CR(result) {int r=(result); if (r<0) return r;}
#define CRW(result,widget) {int r=(result); if (r<0) { gp_widget_free (widget); return r; } }

static struct {
	RicohModel id;
	const char *model;
} models[] = {
	{RICOH_MODEL_1,     "Ricoh:RDC-1"},
	{RICOH_MODEL_2,     "Ricoh:RDC-2"},
	{RICOH_MODEL_2E,    "Ricoh:RDC-2E"},
	{RICOH_MODEL_100G,  "Ricoh:RDC-100G"},
	{RICOH_MODEL_300,   "Ricoh:RDC-300"},
	{RICOH_MODEL_300Z,  "Ricoh:RDC-300Z"},
	{RICOH_MODEL_4200,  "Ricoh:RDC-4200"},
	{RICOH_MODEL_4300,  "Ricoh:RDC-4300"},
	{RICOH_MODEL_5000,  "Ricoh:RDC-5000"},
	{RICOH_MODEL_ESP2,  "Philips:ESP2"},
	{RICOH_MODEL_ESP50, "Philips:ESP50"},
	{RICOH_MODEL_ESP60, "Philips:ESP60"},
	{RICOH_MODEL_ESP70, "Philips:ESP70"},
	{RICOH_MODEL_ESP80, "Philips:ESP80"},
	{RICOH_MODEL_ESP80SXG, "Philips:ESP80SXG"},
	{0, NULL}
};

struct _CameraPrivateLibrary {
	RicohModel model;
};

int
camera_abilities (CameraAbilitiesList *list)
{
	int i;
	CameraAbilities a;

	memset (&a, 0, sizeof (CameraAbilities));
	for (i = 0; models[i].model; i++) {
		strcpy (a.model, models[i].model);
		a.status = GP_DRIVER_STATUS_EXPERIMENTAL;
		a.port = GP_PORT_SERIAL;
		a.operations = GP_OPERATION_CAPTURE_IMAGE |
			       GP_OPERATION_CONFIG;
		a.file_operations = GP_FILE_OPERATION_DELETE |
				    GP_FILE_OPERATION_PREVIEW;
		a.folder_operations = GP_FOLDER_OPERATION_PUT_FILE;
		CR (gp_abilities_list_append (list, a));
	}

	return (GP_OK);
}

static int
camera_exit (Camera *camera, GPContext *context)
{
	if (camera->pl) {
		free (camera->pl);
		camera->pl = NULL;
	}

	ricoh_disconnect (camera, context);

	return GP_OK;
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	Camera *camera = data;
	unsigned int n, i;
	const char *name;

	CR (ricoh_get_num (camera, context, &n));
	for (i = 0; i < n; i++) {
		CR (ricoh_get_pic_name (camera, context, i + 1, &name));
		CR (gp_list_append (list, name, NULL));
	}

	return (GP_OK);
}

static int
get_info_func (CameraFilesystem *fs, const char *folder, const char *filename,
         CameraFileInfo *info, void *data, GPContext *context)
{
	Camera *camera = data;
	int n;
	const char *name;

	CR (n = gp_filesystem_number (fs, folder, filename, context));
	n++;
	
	info->audio.fields = GP_FILE_INFO_NONE; 	/* no info anbout audio files */
	
	info->preview.width = 80;
	info->preview.height = 60;
	info->preview.fields = GP_FILE_INFO_WIDTH | GP_FILE_INFO_HEIGHT; 
	
	CR (ricoh_get_pic_name (camera, context, n, &name));
	CR (ricoh_get_pic_date (camera, context, n, &info->file.mtime));
	CR (ricoh_get_pic_size (camera, context, n, &info->file.size));
	strcpy (info->file.type, GP_MIME_EXIF);
	info->file.fields = GP_FILE_INFO_SIZE | GP_FILE_INFO_MTIME
		| GP_FILE_INFO_TYPE;
	
	return (GP_OK);
}
					 
static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *user_data,
	       GPContext *context)
{
	Camera *camera = user_data;
	int n;
	unsigned int size;
	unsigned char *data;

	CR (n = gp_filesystem_number (fs, folder, filename, context));
	n++;

	switch (type) {
	case GP_FILE_TYPE_NORMAL:
		CR (ricoh_get_pic (camera, context, n,
				   RICOH_FILE_TYPE_NORMAL, &data, &size));
		gp_file_set_mime_type (file, GP_MIME_EXIF);
		break;
	case GP_FILE_TYPE_PREVIEW:
		CR (ricoh_get_pic (camera, context, n,
				   RICOH_FILE_TYPE_PREVIEW, &data, &size));
		gp_file_set_mime_type (file, GP_MIME_TIFF);
		break;		
	default:
		return (GP_ERROR_NOT_SUPPORTED);
	}

	gp_file_set_data_and_size (file, (char*)data, size);

	return (GP_OK);
}

static int
del_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       void *user_data, GPContext *context)
{
	Camera *camera = user_data;
	int n;

	CR (n = gp_filesystem_number (fs, folder, filename, context));
	n++;

	CR (ricoh_del_pic (camera, context, n));

	return (GP_OK);
}

static int
camera_about (Camera *camera, CameraText *about, GPContext *context)
{
	GP_DEBUG ("camera_about()");

	strcpy (about->text,
		_("Ricoh / Philips driver by\n"
		  "Lutz Mueller <lutz@users.sourceforge.net>,\n"
		  "Martin Fischer <martin.fischer@inka.de>,\n"
		  "based on Bob Paauwe's driver\n" )
		);

	return GP_OK;
}

int
camera_id (CameraText *id)
{
	strcpy (id->text, "Ricoh");

	return (GP_OK);
}

static int
camera_summary (Camera *camera, CameraText *about, GPContext *context)
{
	int avail_mem, total_mem;
	char model[128];
	unsigned int i;

	CR (ricoh_get_cam_amem (camera, context, &avail_mem));
	CR (ricoh_get_cam_mem  (camera, context, &total_mem));

	memset (model, 0, sizeof (model));
	for (i = 0; models[i].model; i++)
		if (models[i].id == camera->pl->model)
			break;
	if (models[i].model)
		strncpy (model, models[i].model, sizeof (model) - 1);
	else
		snprintf (model, sizeof (model) - 1, _("unknown (0x%02x)"),
			  camera->pl->model);

	sprintf (about->text, _("Model: %s\n"
			        "Memory: %d byte(s) of %d available"),
		model, avail_mem, total_mem);

	return (GP_OK);
}

static int
camera_capture (Camera *camera, CameraCaptureType type,
		CameraFilePath *path, GPContext *context)
{
	unsigned int n;

	if (type != GP_CAPTURE_IMAGE)
		return (GP_ERROR_NOT_SUPPORTED);

	CR (ricoh_get_num (camera, context, &n));
	CR (ricoh_take_pic (camera, context));

	sprintf (path->name, "rdc%04i.jpg", n + 1);
	strcpy (path->folder, "/");
	CR (gp_filesystem_append (camera->fs, path->folder,
				  path->name, context));

	return (GP_OK);
}

static int
put_file_func (CameraFilesystem *fs, const char *folder, const char *name,
	       CameraFileType type, CameraFile *file, void *user_data, GPContext *context)
{
	const char *data;
	unsigned long int size;
	Camera *camera = user_data;

	if (type != GP_FILE_TYPE_NORMAL)
		return GP_ERROR_BAD_PARAMETERS;
	CR (gp_file_get_data_and_size (file, &data, &size));
	return ricoh_put_file (camera, context, name, (const unsigned char *)data, size);
}

#undef N_ELEMENTS
#define N_ELEMENTS(v) (sizeof(v)/sizeof(v[0]))

static struct {
	RicohExposure exposure;
	const char *name;
} ricoh_exposures[] = {
	{RICOH_EXPOSURE_M20,  N_("-2.0")},
	{RICOH_EXPOSURE_M15,  N_("-1.5")},
	{RICOH_EXPOSURE_M10,  N_("-1.0")},
	{RICOH_EXPOSURE_M05,  N_("-0.5")},
	{RICOH_EXPOSURE_00,   N_("0.0")},
	{RICOH_EXPOSURE_05,   N_("0.5")},
	{RICOH_EXPOSURE_10,   N_("1.0")},
	{RICOH_EXPOSURE_15,   N_("1.5")},
	{RICOH_EXPOSURE_20,   N_("2.0")},
	{RICOH_EXPOSURE_AUTO, N_("Auto")}
};

static struct {
	RicohResolution resolution;
	const char *name;
} ricoh_resolutions[] = {
	{RICOH_RESOLUTION_640_480,  N_("640 x 480")},
	{RICOH_RESOLUTION_1280_960, N_("1280 x 960")}
};

static struct {
	RicohWhiteLevel white_level;
	const char *name;
} ricoh_white_levels[] = {
	{RICOH_WHITE_LEVEL_AUTO,		N_("Auto")},
	{RICOH_WHITE_LEVEL_OUTDOOR,		N_("Outdoor")},
	{RICOH_WHITE_LEVEL_FLUORESCENT,		N_("Fluorescent")},
	{RICOH_WHITE_LEVEL_INCANDESCENT,	N_("Incandescent")},
	{RICOH_WHITE_LEVEL_BLACK_WHITE,		N_("Black & White")},
	{RICOH_WHITE_LEVEL_SEPIA,		N_("Sepia")}
};

static struct {
	RicohMacro macro;
	const char *name;
} ricoh_macros[] = {
	{RICOH_MACRO_ON, N_("On")},
	{RICOH_MACRO_OFF, N_("Off")}
};

static struct {
	RicohCompression compression;
	const char *name;
} ricoh_compressions[] = {
	{RICOH_COMPRESSION_NONE, N_("None")},
	{RICOH_COMPRESSION_MAX,  N_("Maximal")},
	{RICOH_COMPRESSION_NORM, N_("Normal")},
	{RICOH_COMPRESSION_MIN,  N_("Minimal")}
};

static struct {
	RicohRecMode rec_mode;
	const char *name;
} ricoh_rec_modes[] = {
	{RICOH_REC_MODE_IMAGE,           N_("Image")},
	{RICOH_REC_MODE_CHARACTER,       N_("Character")},
	{RICOH_REC_MODE_SOUND,           N_("Sound")},
	{RICOH_REC_MODE_IMAGE_SOUND,     N_("Image & Sound")},
	{RICOH_REC_MODE_CHARACTER_SOUND, N_("Character & Sound")}
};

static struct {
	RicohFlash flash;
	const char *name;
} ricoh_flashs[] = {
	{RICOH_FLASH_AUTO, N_("Auto")},
	{RICOH_FLASH_OFF,  N_("Off")},
	{RICOH_FLASH_ON,   N_("On")}
};

static struct {
	RicohZoom zoom;
	const char *name;
} ricoh_zooms[] = {
	{RICOH_ZOOM_OFF, N_("Off")},
	{RICOH_ZOOM_1,   N_("2x")},
	{RICOH_ZOOM_2,   N_("3x")},
	{RICOH_ZOOM_3,   N_("4x")},
	{RICOH_ZOOM_4,   N_("5x")},
	{RICOH_ZOOM_5,   N_("6x")},
	{RICOH_ZOOM_6,   N_("7x")},
	{RICOH_ZOOM_7,   N_("8x")},
	{RICOH_ZOOM_8,   N_("9x")}
};

#undef R_ADD_RADIO
#define R_ADD_RADIO(ca,co,s,type,n,Name)				\
{									\
	CameraWidget *__w = NULL;					\
	type __v;							\
	unsigned int __i;						\
									\
	CR (gp_widget_new (GP_WIDGET_RADIO, _(Name), &__w));		\
	CRW (gp_widget_set_name (__w, (Name)), __w);			\
	CRW (gp_widget_append ((s), __w), __w);				\
	CR (ricoh_get_##n ((ca), (co), &__v));				\
	for (__i = 0; __i < N_ELEMENTS (ricoh_##n##s); __i++) {		\
		CR (gp_widget_add_choice (__w, _(ricoh_##n##s[__i].name)));	\
		if (__v == ricoh_##n##s[__i].n)				\
			CR (gp_widget_set_value (__w,			\
				_(ricoh_##n##s[__i].name)));		\
	}								\
}

#undef R_CHECK_RADIO
#define R_CHECK_RADIO(c,co,wi,n,Name)					\
{									\
	CameraWidget *__w = NULL;					\
	const char *__v = NULL;						\
	unsigned int __i;						\
									\
        CR (gp_widget_get_child_by_name (wi, Name, &__w));		\
	if (gp_widget_changed (__w)) {					\
		CR (gp_widget_get_value (__w, &__v));			\
		for (__i = 0; __i < N_ELEMENTS (ricoh_##n##s); __i++)	\
			if (!strcmp (__v, _(ricoh_##n##s[__i].name))) {	\
				CR (ricoh_set_##n (c, co, ricoh_##n##s[__i].n));		\
				break;					\
			}						\
	}								\
}

static int
camera_get_config (Camera *c, CameraWidget **window, GPContext *co)
{
	CameraWidget *s, *w;
	const char *copyright;
	time_t time;

	CR (gp_widget_new (GP_WIDGET_WINDOW, _("Configuration"), window));

	/* General settings */
	CR (gp_widget_new (GP_WIDGET_SECTION, _("General"), &s));
	CRW (gp_widget_append (*window, s), s);

	/* Copyright */
	CR (gp_widget_new (GP_WIDGET_TEXT, _("Copyright"), &w));
	CRW (gp_widget_set_name (w, "copyright"), w);
	CRW (gp_widget_set_info (w, _("Copyright (max. 20 characters)")), w);
	CRW (gp_widget_append (s, w), w);
	CR (ricoh_get_copyright (c, co, &copyright));
	CR (gp_widget_set_value (w, (void *) copyright));

	/* Date */
	CR (gp_widget_new (GP_WIDGET_DATE, _("Date & Time"), &w));
	CRW (gp_widget_set_name (w, "date"), w);
	CRW (gp_widget_set_info (w, _("Date & Time")), w);
	CRW (gp_widget_append (s, w), w);
	CR (ricoh_get_date (c, co, &time));
	CR (gp_widget_set_value (w, &time));

	/* Picture related settings */
	CR (gp_widget_new (GP_WIDGET_SECTION, _("Pictures"), &s));
	CRW (gp_widget_append (*window, s), w);

	R_ADD_RADIO (c, co, s, RicohResolution,  resolution,  "Resolution")
	R_ADD_RADIO (c, co, s, RicohExposure,    exposure,    "Exposure")
	R_ADD_RADIO (c, co, s, RicohMacro,       macro,       "Macro")
	R_ADD_RADIO (c, co, s, RicohFlash,       flash,       "Flash")
	R_ADD_RADIO (c, co, s, RicohZoom,        zoom,        "Zoom")
	R_ADD_RADIO (c, co, s, RicohCompression, compression, "Compression")
	R_ADD_RADIO (c, co, s, RicohWhiteLevel,  white_level, "White Level")
	R_ADD_RADIO (c, co, s, RicohRecMode,     rec_mode,    "Record Mode")

	return (GP_OK);
}

static int
camera_set_config (Camera *c, CameraWidget *window, GPContext *co)
{
	CameraWidget *w;
	const char *v_char;
	time_t time;
	RicohMode mode;

	/* We need to be in record mode to set settings. */
	CR (ricoh_get_mode (c, co, &mode));
	if (mode != RICOH_MODE_RECORD)
		CR (ricoh_set_mode (c, co, RICOH_MODE_RECORD));

	/* Copyright */
	CR (gp_widget_get_child_by_name (window, "copyright", &w));
	if (gp_widget_changed (w)) {
		CR (gp_widget_get_value (w, &v_char));
		CR (ricoh_set_copyright (c, co, v_char));
	}

	/* Date */
	CR (gp_widget_get_child_by_name (window, "date", &w));
	if (gp_widget_changed (w)) {
		CR (gp_widget_get_value (w, &time));
		CR (ricoh_set_date (c, co, time));
	}

	R_CHECK_RADIO (c, co, window, resolution,  N_("Resolution"))
	R_CHECK_RADIO (c, co, window, exposure,    N_("Exposure"))
	R_CHECK_RADIO (c, co, window, white_level, N_("White level"))
	R_CHECK_RADIO (c, co, window, macro,       N_("Macro"))
	R_CHECK_RADIO (c, co, window, zoom,        N_("Zoom"))
	R_CHECK_RADIO (c, co, window, flash,       N_("Flash"))
	R_CHECK_RADIO (c, co, window, rec_mode,    N_("Record Mode"))
	R_CHECK_RADIO (c, co, window, compression, N_("Compression"))

	return (GP_OK);
}

static struct {
	unsigned int speed;
	RicohSpeed rspeed;
} speeds[] = {
	{  2400, RICOH_SPEED_2400},
	{115200, RICOH_SPEED_115200},
	{  4800, RICOH_SPEED_4800},
	{ 19200, RICOH_SPEED_19200},
	{ 38400, RICOH_SPEED_38400},
	{ 57600, RICOH_SPEED_57600},
	{     0, 0}
};

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.get_file_func = get_file_func,
	.del_file_func = del_file_func,
	.put_file_func = put_file_func,
	.get_info_func = get_info_func
};

int
camera_init (Camera *camera, GPContext *context)
{
	GPPortSettings settings;
	unsigned int speed, i;
	int result;
	RicohModel model = 0;

	/* Try to contact the camera. */
	CR (gp_port_set_timeout (camera->port, 5000));
	CR (gp_port_get_settings (camera->port, &settings));
	speed = (settings.serial.speed ? settings.serial.speed : 115200);
	for (i = 0; speeds[i].speed; i++) {
		GP_DEBUG ("Trying speed %i...", speeds[i].speed);
		settings.serial.speed = speeds[i].speed;
		CR (gp_port_set_settings (camera->port, settings));

		/*
		 * Note that ricoh_connect can only be called to 
		 * initialize the connection at 2400 bps. At other
		 * speeds, a different function needs to be used.
		 */
		result = (speeds[i].rspeed == RICOH_SPEED_2400) ? 
				ricoh_connect (camera, NULL, &model) :
				ricoh_get_mode (camera, NULL, NULL);
		if (result == GP_OK)
			break;
	}

	/* Contact made? If not, report error. */
	if (!speeds[i].speed) {
		gp_context_error (context, _("Could not contact camera."));
		return (GP_ERROR);
	}

	/* Contact made. Do we need to change the speed? */
	if (settings.serial.speed != speed) {
		for (i = 0; speeds[i].speed; i++)
			if (speeds[i].speed == speed)
				break;
		if (!speeds[i].speed) {
			gp_context_error (context, _("Speed %i is not "
				"supported!"), speed);
			return (GP_ERROR);
		}
		CR (ricoh_set_speed (camera, context, speeds[i].rspeed));
		settings.serial.speed = speed;
		CR (gp_port_set_settings (camera->port, settings));

		/* Check if the camera is still there. */
		CR (ricoh_get_mode (camera, context, NULL));
	}

	/* setup the function calls */
	camera->functions->exit = camera_exit;
	camera->functions->summary = camera_summary;
	camera->functions->capture = camera_capture;
	camera->functions->about = camera_about;
	camera->functions->get_config = camera_get_config;
	camera->functions->set_config = camera_set_config;
	CR (gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera));
	/*
	 * Remember the model. It could be that there hasn't been the 
	 * need to call ricoh_connect. Then we don't have a model. Should
	 * we disconnect and reconnect in this case?
	 */
	camera->pl = malloc (sizeof (CameraPrivateLibrary));
	if (!camera->pl)
		return (GP_ERROR_NO_MEMORY);
	memset (camera->pl, 0, sizeof (CameraPrivateLibrary));
	camera->pl->model = model;

	return (GP_OK);
}

