/* library.c
 *
 * Copyright 2001 Lutz Mueller
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
#include <time.h>

#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-port-log.h>

#include "konica.h"

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

#define C(r) {int ret=(r);if(ret<0) return(ret);}
#define C_NULL(r) {if (!(r)) return (GP_ERROR_BAD_PARAMETERS);}

#define GP_MODULE "konica"
#define PING_TIMEOUT 60

static struct {
        const char *model;
        int image_id_long;
        int vendor;
        int product;
} konica_cameras[] = {
        {"Konica Q-EZ",        0, 0, 0},
        {"Konica Q-M100",      0, 0, 0},
        {"Konica Q-M100V",     0, 0, 0},
        {"Konica Q-M200",      1, 0, 0},
        {"HP PhotoSmart",      0, 0, 0},
        {"HP PhotoSmart C20",  0, 0, 0},
        {"HP PhotoSmart C30",  0, 0, 0},
        {"HP PhotoSmart C200", 0, 0, 0},
        {NULL,                 0, 0, 0}
};

struct _CameraPrivateLibrary {
	unsigned int speed, timeout;
        int image_id_long;
};


static int localization_file_read (Camera* camera, const char* file_name,
                                   unsigned char** data, long int* data_size,
				   GPContext *context);

static int
timeout_func (Camera *camera, GPContext *context)
{
	C(k_ping (camera->port, context));

	return (GP_OK);
}

static int
get_info (Camera *camera, unsigned int n, CameraFileInfo *info,
	  char *fn, CameraFile *file, GPContext *context)
{
	unsigned long image_id;
	unsigned int buffer_size, exif_size;
	unsigned char *buffer = NULL;
	int protected, r;

	/*
	 * Remove the timeout, get the information and restart the
	 * timeout afterwards.
	 */
	gp_camera_stop_timeout (camera, camera->pl->timeout);
	r = k_get_image_information (camera->port, context,
		camera->pl->image_id_long, n, &image_id, &exif_size,
		&protected, &buffer, &buffer_size);
	camera->pl->timeout = gp_camera_start_timeout (camera, PING_TIMEOUT,
						       timeout_func);
	C(r);

	info->audio.fields = GP_FILE_INFO_NONE;

	info->preview.fields = GP_FILE_INFO_TYPE;
	strcpy (info->preview.type, GP_MIME_JPEG);

	info->file.fields = GP_FILE_INFO_SIZE | GP_FILE_INFO_PERMISSIONS |
			    GP_FILE_INFO_TYPE;
	info->file.size = exif_size * 1000;
	info->file.permissions = GP_FILE_PERM_READ;
	if (!protected)
		info->file.permissions |= GP_FILE_PERM_DELETE;
	strcpy (info->file.type, GP_MIME_JPEG);
	sprintf (fn, "%06i.jpeg", (int) image_id);

	if (file)
		gp_file_set_data_and_size (file, (char*)buffer, buffer_size);
	else
		free (buffer);

	return (GP_OK);
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
                void *data, GPContext *context)
{
	CameraFile *file;
	CameraFileInfo info;
        KStatus status;
        unsigned int i, id;
        Camera *camera = data;
	int result;

        /*
         * We can't get the filename from the camera.
         * But we decide to call the images %6i.jpeg', with the image id as
         * parameter. Therefore, let's get the image ids.
         */
        C(k_get_status (camera->port, context, &status));

	id = gp_context_progress_start (context, status.pictures,
					_("Getting file list..."));
        for (i = 0; i < status.pictures; i++) {
		char fn[40];
                /* Get information */
		gp_file_new (&file);
		result = get_info (camera, i + 1, &info, fn, file, context);
		if (result < 0) {
			gp_file_unref (file);
			return (result);
		}

                /*
		 * Append directly to the filesystem instead of to the list,
		 * because we have additional information.
		 */
		gp_filesystem_append (camera->fs, folder, fn, context);
		gp_filesystem_set_info_noop (camera->fs, folder, fn, info, context);
		gp_filesystem_set_file_noop (camera->fs, folder, fn, GP_FILE_TYPE_PREVIEW, file, context);
		gp_file_unref (file);

		gp_context_idle (context);
		gp_context_progress_update (context, id, i + 1);
		if (gp_context_cancel (context) == GP_CONTEXT_FEEDBACK_CANCEL)
			return (GP_ERROR_CANCEL);
        }
	gp_context_progress_stop (context, id);

        return (GP_OK);
}

int
camera_id (CameraText* id)
{
        strcpy (id->text, "konica");

        return (GP_OK);
}

int
camera_abilities (CameraAbilitiesList* list)
{
        int i;
        CameraAbilities a;

        for (i = 0; konica_cameras [i].model; i++) {
		memset(&a, 0, sizeof(a));
		a.status = GP_DRIVER_STATUS_PRODUCTION;
                strcpy (a.model, konica_cameras [i].model);
		a.usb_vendor  = konica_cameras [i].vendor;
		a.usb_product = konica_cameras [i].product;
                if (konica_cameras [i].vendor)
                        a.port = GP_PORT_USB;
                else {
                        a.port = GP_PORT_SERIAL;
                        a.speed[0]     = 300;
                        a.speed[1]     = 600;
                        a.speed[2]     = 1200;
                        a.speed[3]     = 2400;
                        a.speed[4]     = 4800;
                        a.speed[5]     = 9600;
                        a.speed[6]     = 19200;
                        a.speed[7]     = 38400;
                        a.speed[8]     = 57600;
                        a.speed[9]     = 115200;
                        a.speed[10]    = 0;
                }
                a.operations = GP_OPERATION_CONFIG |
                               GP_OPERATION_CAPTURE_IMAGE |
                               GP_OPERATION_CAPTURE_PREVIEW;
                a.file_operations = GP_FILE_OPERATION_DELETE |
                                    GP_FILE_OPERATION_PREVIEW |
				    GP_FILE_OPERATION_EXIF;
                a.folder_operations = GP_FOLDER_OPERATION_DELETE_ALL;
                gp_abilities_list_append (list, a);
        }

        return (GP_OK);
}

static int
set_speed (Camera *camera, int speed, GPContext *context)
{
	GPPortSettings settings;
	KBitRate bit_rates;
	KBitFlag bit_flags;
	int i;
	int speeds[] = {300, 600, 1200, 2400, 4800, 9600, 19200,
				 38400, 57600, 115200};

	C(gp_port_get_settings (camera->port, &settings));
	if ((settings.serial.speed == speed) ||
	    (settings.serial.speed == 115200))
		return (GP_OK);

	switch (speed) {
	case 0:

		/* Set the highest possible speed */
		C(k_get_io_capability (camera->port, context, &bit_rates,
						    &bit_flags));
		for (i = 9; i >= 0; i--)
			if ((1 << i) & bit_rates)
				break;
		if (i < 0)
			return (GP_ERROR_IO_SERIAL_SPEED);
		bit_rates = (1 << i);
		speed = speeds[i];
		break;
	case 300:
		bit_rates = 1 << 0;
		break;
	case 600:
		bit_rates = 1 << 1;
		break;
	case 1200:
		bit_rates = 1 << 2;
		break;
	case 2400:
		bit_rates = 1 << 3;
		break;
	case 4800:
		  bit_rates = 1 << 4;
		  break;
	case 9600:
		  bit_rates = 1 << 5;
		  break;
	case 19200:
		  bit_rates = 1 << 6;
		  break;
	case 38400:
		  bit_rates = 1 << 7;
		  break;
	case 57600:
		  bit_rates = 1 << 8;
		  break;
	case 115200:
		  bit_rates = 1 << 9;
		  break;
	default:
		  return (GP_ERROR_IO_SERIAL_SPEED);
	}

        /* Request the new speed */
	bit_flags = K_BIT_FLAG_8_BITS;
	C(k_set_io_capability (camera->port, context, bit_rates,
					    bit_flags));
	gp_log (GP_LOG_DEBUG, "konica", "Reconnecting at speed %d", speed);
	settings.serial.speed = speed;
	C(gp_port_set_settings (camera->port, settings));
	C(k_init (camera->port, context));

	return (GP_OK);
}

static int
test_speed (Camera *camera, GPContext *context)
{
	int i;
	unsigned int speeds[] = {115200, 9600, 57600, 38400, 19200,
				 4800, 2400, 1200, 600, 300};
	unsigned int id;
	GPPortSettings settings;

	C(gp_port_get_settings (camera->port, &settings));

	id = gp_context_progress_start (context, 10,
					_("Testing different speeds..."));
	for (i = 0; i < 10; i++) {
		gp_log (GP_LOG_DEBUG, "konica", "Testing speed %d",
			speeds[i]);
		settings.serial.speed = speeds[i];
		C(gp_port_set_settings (camera->port, settings));
		if (k_init (camera->port, context) == GP_OK)
			break;
		gp_context_idle (context);
		gp_context_progress_update (context, id, i + 1);
		if (gp_context_cancel (context) == GP_CONTEXT_FEEDBACK_CANCEL)
			return (GP_ERROR_CANCEL);
	}
	gp_context_progress_stop (context, id);
	if (i == 10) {
		gp_context_error (context, _("The camera could not be "
			"contacted. Please make sure it is connected to the "
			"computer and turned on."));
		return (GP_ERROR_IO);
	}

	return (speeds[i]);
}

static int
set_info_func (CameraFilesystem *fs, const char *folder, const char *file,
               CameraFileInfo info, void *data, GPContext *context)
{
        Camera *camera = data;
        char tmp[7];
        int protected;
        unsigned long image_id;

        /* Permissions? */
        if (info.file.fields & GP_FILE_INFO_PERMISSIONS) {
                strncpy (tmp, file, 6);
                tmp[6] = '\0';
                image_id = atol (tmp);
                if (info.file.permissions & GP_FILE_PERM_DELETE)
                        protected = FALSE;
                else
                        protected = TRUE;
                C(k_set_protect_status (camera->port, context,
			camera->pl->image_id_long, image_id, protected));
        }
        return (GP_OK);
}

static int
get_info_func (CameraFilesystem *fs, const char *folder, const char *filename,
               CameraFileInfo *info, void *data, GPContext *context)
{
        Camera *camera = data;
	CameraFile *file;
        int n, result;
	char fn[40];

	/* We need image numbers starting with 1 */
	n = gp_filesystem_number (camera->fs, folder, filename, context);
	if (n < 0)
		return (n);
	n++;

	gp_file_new (&file);
	result = get_info (camera, n, info, fn, file, context);
	if (result < 0) {
		gp_file_unref (file);
		return (result);
	}
	gp_filesystem_set_file_noop (fs, folder, filename, GP_FILE_TYPE_PREVIEW, file, context);
	gp_file_unref (file);
        return (GP_OK);
}

static int
camera_exit (Camera* camera, GPContext *context)
{
        if (camera->pl) {
		gp_camera_stop_timeout (camera, camera->pl->timeout);
                free (camera->pl);
                camera->pl = NULL;
        }

        return (GP_OK);
}

static int
delete_all_func (CameraFilesystem *fs, const char* folder, void *data,
		 GPContext *context)
{
	Camera *camera = data;
        unsigned int not_erased = 0;

        if (strcmp (folder, "/"))
                return (GP_ERROR_DIRECTORY_NOT_FOUND);

        C(k_erase_all (camera->port, context, &not_erased));

        if (not_erased) {
		gp_context_error (context, _("%i pictures could not be "
			"deleted because they are protected"), not_erased);
		gp_filesystem_reset (camera->fs);
		return (GP_ERROR);
        }

        return (GP_OK);
}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
               CameraFileType type, CameraFile *file, void *data,
	       GPContext *context)
{
        Camera *camera = data;
        unsigned long image_id;
	char image_id_string[] = {0, 0, 0, 0, 0, 0, 0};
        unsigned char *fdata = NULL;
        unsigned int size;
	CameraFileInfo info;
	int r;

        if (strlen (filename) != 11)
                return (GP_ERROR_FILE_NOT_FOUND);
        if (strcmp (folder, "/"))
                return (GP_ERROR_DIRECTORY_NOT_FOUND);

	/* Check if we can get the image id from the filename. */
	strncpy (image_id_string, filename, 6);
	image_id = atol (image_id_string);

	/* Get information about the image */
	if (type == GP_FILE_TYPE_NORMAL)
		C(gp_filesystem_get_info (camera->fs, folder,
						filename, &info, context));

	/*
	 * Remove the timeout, get the image and start the timeout
	 * afterwards.
	 */
	gp_camera_stop_timeout (camera, camera->pl->timeout);
        switch (type) {
        case GP_FILE_TYPE_PREVIEW:
		size = 2048;
		r = k_get_image (camera->port, context,
			camera->pl->image_id_long,
			image_id, K_THUMBNAIL, &fdata,
			&size);
                break;
        case GP_FILE_TYPE_NORMAL:
		size = info.file.size;
		r = k_get_image (camera->port, context,
			camera->pl->image_id_long,
			image_id, K_IMAGE_EXIF, &fdata,
			&size);
                break;
        default:
		return (GP_ERROR_NOT_SUPPORTED);
        }
	C(r);
	camera->pl->timeout = gp_camera_start_timeout (camera, PING_TIMEOUT,
						       timeout_func);

        C(gp_file_set_data_and_size (file, (char*)fdata, size));
        return gp_file_set_mime_type (file, GP_MIME_JPEG);
}

static int
delete_file_func (CameraFilesystem *fs, const char *folder,
		  const char *filename, void *data, GPContext *context)
{
	Camera *camera = data;
        char tmp[] = {0, 0, 0, 0, 0, 0, 0};
        unsigned long image_id;

	C_NULL (camera && folder && filename);

	/* We don't support folders */
        if (strcmp (folder, "/")) return (GP_ERROR_DIRECTORY_NOT_FOUND);

	/* Extract the image id from the filename */
	strncpy (tmp, filename, 6);
        image_id = atol (tmp);

        C(k_erase_image (camera->port, context, camera->pl->image_id_long,
			      image_id));

        return (GP_OK);
}

static int
camera_summary (Camera* camera, CameraText* summary, GPContext *context)
{
	KInformation info;

        GP_DEBUG ("*** ENTER: camera_summary "
                         "***");
        C(k_get_information (camera->port, context, &info));

	snprintf (summary->text, sizeof (summary->text),
                _("Model: %s\n"
                "Serial Number: %s,\n"
                "Hardware Version: %i.%i\n"
                "Software Version: %i.%i\n"
                "Testing Software Version: %i.%i\n"
                "Name: %s,\n"
                "Manufacturer: %s\n"),
		info.model, info.serial_number,
		info.hardware.major, info.hardware.minor,
		info.software.major, info.software.minor,
		info.testing.major, info.testing.minor,
                info.name, info.manufacturer);

        return (GP_OK);
}

static int
camera_capture_preview (Camera* camera, CameraFile* file, GPContext *context)
{
        unsigned char *data = NULL;
        unsigned int size = 0;

        C(k_get_preview (camera->port, context, TRUE, &data, &size));
        C(gp_file_set_data_and_size (file, (char*)data, size));
        C(gp_file_set_mime_type (file, GP_MIME_JPEG));

        return (GP_OK);
}

static int
camera_capture (Camera* camera, CameraCaptureType type, CameraFilePath* path,
		GPContext *context)
{
        unsigned long image_id;
	unsigned int exif_size;
	unsigned char *buffer = NULL;
	unsigned int buffer_size;
	int protected, r;
	CameraFile *file = NULL;
	CameraFileInfo info;
	char fn[40];

	C_NULL (camera && path);

	/* We only support capturing of images */
	if (type != GP_CAPTURE_IMAGE)
		return (GP_ERROR_NOT_SUPPORTED);

	/* Stop the timeout, take the picture, and restart the timeout. */
	gp_camera_stop_timeout (camera, camera->pl->timeout);
	r = k_take_picture (camera->port, context, camera->pl->image_id_long,
		&image_id, &exif_size, &buffer, &buffer_size, &protected);
	camera->pl->timeout = gp_camera_start_timeout (camera, PING_TIMEOUT,
						       timeout_func);
	C(r);

	sprintf (path->name, "%06i.jpeg", (int) image_id);
	strcpy (path->folder, "/");
	C(gp_filesystem_append (camera->fs, path->folder,
					     path->name, context));

	info.preview.fields = GP_FILE_INFO_SIZE | GP_FILE_INFO_TYPE;
	info.preview.size = buffer_size;
	strcpy (info.preview.type, GP_MIME_JPEG);

	info.file.fields = GP_FILE_INFO_SIZE | GP_FILE_INFO_PERMISSIONS |
			    GP_FILE_INFO_TYPE;
	info.file.size = exif_size;
	info.file.permissions = GP_FILE_PERM_READ;
	if (!protected)
		info.file.permissions |= GP_FILE_PERM_DELETE;
	strcpy (info.file.type, GP_MIME_JPEG);
	sprintf (fn, "%06i.jpeg", (int) image_id);
	gp_filesystem_set_info_noop (camera->fs, path->folder, fn, info, context);

	gp_file_new (&file);
	gp_file_set_mime_type (file, GP_MIME_JPEG);
	gp_file_set_data_and_size (file, (char*)buffer, buffer_size);
	gp_filesystem_set_file_noop (camera->fs, path->folder, fn, GP_FILE_TYPE_EXIF, file, context);
	gp_file_unref (file);

        return (GP_OK);
}

static int
camera_about (Camera* camera, CameraText* about, GPContext *context)
{
	C_NULL (camera && about);

	/* Translators: please write 'M"uller' (that is, with u-umlaut)
	   if your charset allows it.  If not, use "Mueller". */
        strcpy (about->text, _("Konica library\n"
                "Lutz Mueller <lutz@users.sourceforge.net>\n"
                "Support for all Konica and several HP cameras."));

        return (GP_OK);
}

static int
camera_get_config (Camera* camera, CameraWidget** window, GPContext *context)
{
        CameraWidget *widget;
        CameraWidget *section;
        KStatus status;
        KPreferences preferences;
        int     year_4_digits;
        struct tm       tm_struct;
        time_t          t;
        float value_float;
        const char *name;
        gp_system_dir d;
        gp_system_dirent de;
	unsigned int id;

        /* Get the current settings. */
	id = gp_context_progress_start (context, 2,
					_("Getting configuration..."));
        C(k_get_status (camera->port, context, &status));
	gp_context_progress_update (context, id, 1);
        C(k_get_preferences (camera->port, context, &preferences));
	gp_context_progress_stop (context, id);

        /* Create the window. */
        gp_widget_new (GP_WIDGET_WINDOW, _("Konica Configuration"), window);

        /************************/
        /* Persistent Settings  */
        /************************/
        gp_widget_new (GP_WIDGET_SECTION, _("Persistent Settings"), &section);
        gp_widget_append (*window, section);

        /* Date */
        gp_widget_new (GP_WIDGET_DATE, _("Date and Time"), &widget);
        gp_widget_append (section, widget);
        if (status.date.year > 80) year_4_digits = status.date.year + 1900;
        else year_4_digits = status.date.year + 2000;
        tm_struct.tm_year = year_4_digits - 1900;
        tm_struct.tm_mon = status.date.month - 1;
        tm_struct.tm_mday = status.date.day;
        tm_struct.tm_hour = status.date.hour;
        tm_struct.tm_min = status.date.minute;
        tm_struct.tm_sec = status.date.second;
        t = mktime (&tm_struct);
        gp_widget_set_value (widget, &t);

        /* Beep */
        gp_widget_new (GP_WIDGET_RADIO, _("Beep"), &widget);
        gp_widget_append (section, widget);
        gp_widget_add_choice (widget, _("On"));
        gp_widget_add_choice (widget, _("Off"));
        switch (preferences.beep) {
        case 0:
                gp_widget_set_value (widget, _("Off"));
                break;
        default:
                gp_widget_set_value (widget, _("On"));
                break;
        }
        gp_widget_set_info (widget, _("Shall the camera beep when taking a "
                            "picture?"));

        /* Self Timer Time */
        gp_widget_new (GP_WIDGET_RANGE, _("Self Timer Time"), &widget);
        gp_widget_append (section, widget);
        gp_widget_set_range (widget, 3, 40, 1);
        value_float = preferences.self_timer_time;
        gp_widget_set_value (widget, &value_float);

        /* Auto Off Time */
        gp_widget_new (GP_WIDGET_RANGE, _("Auto Off Time"), &widget);
        gp_widget_append (section, widget);
        gp_widget_set_range (widget, 1, 255, 1);
        value_float = preferences.shutoff_time;
        gp_widget_set_value (widget, &value_float);

        /* Slide Show Interval */
        gp_widget_new (GP_WIDGET_RANGE, _("Slide Show Interval"), &widget);
        gp_widget_append (section, widget);
        gp_widget_set_range (widget, 1, 30, 1);
        value_float = preferences.slide_show_interval;
        gp_widget_set_value (widget, &value_float);

        /* Resolution */
        gp_widget_new (GP_WIDGET_RADIO, _("Resolution"), &widget);
        gp_widget_append (section, widget);
        gp_widget_add_choice (widget, _("Low (576 x 436)"));
        gp_widget_add_choice (widget, _("Medium (1152 x 872)"));
        gp_widget_add_choice (widget, _("High (1152 x 872)"));
        switch (status.resolution) {
        case 1:
                gp_widget_set_value (widget, _("High (1152 x 872)"));
                break;
        case 3:
                gp_widget_set_value (widget, _("Low (576 x 436)"));
                break;
        default:
                gp_widget_set_value (widget, _("Medium (1152 x 872)"));
                break;
        }

        /****************/
        /* Localization */
        /****************/
        gp_widget_new (GP_WIDGET_SECTION, _("Localization"), &section);
        gp_widget_append (*window, section);

        /* Language */
        d = gp_system_opendir (LOCALIZATION);
        if (d) {
                gp_widget_new (GP_WIDGET_MENU, _("Language"), &widget);
                gp_widget_append (section, widget);
                while ((de = gp_system_readdir (d))) {
                        name = gp_system_filename (de);
                        if (name && (*name != '.'))
                                gp_widget_add_choice (widget, name);
                }
                gp_widget_set_value (widget, _("None selected"));
                gp_system_closedir (d);
        }

        /* TV output format */
        gp_widget_new (GP_WIDGET_MENU, _("TV Output Format"), &widget);
        gp_widget_append (section, widget);
        gp_widget_add_choice (widget, _("NTSC"));
        gp_widget_add_choice (widget, _("PAL"));
        gp_widget_add_choice (widget, _("Do not display TV menu"));
        gp_widget_set_value (widget, _("None selected"));

        /* Date format */
        gp_widget_new (GP_WIDGET_MENU, _("Date Format"), &widget);
        gp_widget_append (section, widget);
        gp_widget_add_choice (widget, _("Month/Day/Year"));
        gp_widget_add_choice (widget, _("Day/Month/Year"));
        gp_widget_add_choice (widget, _("Year/Month/Day"));
        gp_widget_set_value (widget, _("None selected"));

        /********************************/
        /* Session-persistent Settings  */
        /********************************/
        gp_widget_new (GP_WIDGET_SECTION, _("Session-persistent Settings"),
                       &section);
        gp_widget_append (*window, section);

        /* Flash */
        gp_widget_new (GP_WIDGET_RADIO, _("Flash"), &widget);
        gp_widget_append (section, widget);
        gp_widget_add_choice (widget, _("Off"));
        gp_widget_add_choice (widget, _("On"));
        gp_widget_add_choice (widget, _("On, red-eye reduction"));
        gp_widget_add_choice (widget, _("Auto"));
        gp_widget_add_choice (widget, _("Auto, red-eye reduction"));
        switch (status.flash) {
        case 0:
                gp_widget_set_value (widget, _("Off"));
                break;
        case 1:
                gp_widget_set_value (widget, _("On"));
                break;
        case 5:
                gp_widget_set_value (widget, _("On, red-eye reduction"));
                break;
        case 6:
                gp_widget_set_value (widget, _("Auto, red-eye reduction"));
                break;
        default:
                gp_widget_set_value (widget, _("Auto"));
                break;
        }

        /* Exposure */
        gp_widget_new (GP_WIDGET_RANGE, _("Exposure"), &widget);
        gp_widget_append (section, widget);
        gp_widget_set_range (widget, 0, 255, 1);
        value_float = status.exposure;
        gp_widget_set_value (widget, &value_float);

        /* Focus */
        gp_widget_new (GP_WIDGET_RADIO, _("Focus"), &widget);
        gp_widget_append (section, widget);
        gp_widget_add_choice (widget, _("Fixed"));
        gp_widget_add_choice (widget, _("Auto"));
        switch ((unsigned int) (status.focus / 2)) {
        case 1:
                gp_widget_set_value (widget, _("Auto"));
                break;
        default:
                gp_widget_set_value (widget, _("Fixed"));
                break;
        }

        /************************/
        /* Volatile Settings    */
        /************************/
        gp_widget_new (GP_WIDGET_SECTION, _("Volatile Settings"), &section);
        gp_widget_append (*window, section);

        /* Self Timer */
        gp_widget_new (GP_WIDGET_RADIO, _("Self Timer"), &widget);
        gp_widget_append (section, widget);
        gp_widget_add_choice (widget, _("Self Timer (next picture only)"));
        gp_widget_add_choice (widget, _("Normal"));
        switch (status.focus % 2) {
        case 1:
                gp_widget_set_value (widget, _("Self Timer ("
                                     "next picture only)"));
                break;
        default:
                gp_widget_set_value (widget, _("Normal"));
                break;
        }

        /* That's it. */
        return (GP_OK);
}

static int
camera_set_config (Camera *camera, CameraWidget *window, GPContext *context)
{
        CameraWidget *section;
        CameraWidget *widget_focus;
        CameraWidget *widget_self_timer;
        CameraWidget *widget;
        KDate date;
        KDateFormat date_format = K_DATE_FORMAT_YEAR_MONTH_DAY;
        KTVOutputFormat tv_output_format = K_TV_OUTPUT_FORMAT_HIDE;
        unsigned int beep = 0;
	int i, j = 0;
        unsigned char *data;
        long int data_size;
        unsigned char focus_self_timer = 0;
	float f;
        char *c;
        struct tm *tm_struct;
        int result;

        GP_DEBUG ("*** ENTER: camera_set_config ***");

        /************************/
        /* Persistent Settings  */
        /************************/
        gp_widget_get_child_by_label (window, _("Persistent Settings"),
                                      &section);

        /* Date & Time */
        gp_widget_get_child_by_label (section, _("Date and Time"), &widget);
        if (gp_widget_changed (widget)) {
                time_t xtime;

                gp_widget_get_value (widget, &i);
                xtime = i;
                tm_struct = localtime (&xtime);
                date.year   = tm_struct->tm_year - 100;
                date.month  = tm_struct->tm_mon + 1;
                date.day    = tm_struct->tm_mday;
                date.hour   = tm_struct->tm_hour;
                date.minute = tm_struct->tm_min;
                date.second = tm_struct->tm_sec;
                C(k_set_date_and_time (camera->port, context, date));
        }

        /* Beep */
        gp_widget_get_child_by_label (section, _("Beep"), &widget);
        if (gp_widget_changed (widget)) {
                gp_widget_get_value (widget, &c);
                if (strcmp (c, _("Off")) == 0)
                        beep = 0;
                else
                        beep = 1;
                C(k_set_preference (camera->port, context,
						 K_PREFERENCE_BEEP, beep));
        }

        /* Self Timer Time */
        gp_widget_get_child_by_label (section, _("Self Timer Time"), &widget);
        if (gp_widget_changed (widget)) {
                gp_widget_get_value (widget, &f);
                C(k_set_preference (camera->port, context,
                        K_PREFERENCE_SELF_TIMER_TIME, (int) f));
        }

        /* Auto Off Time */
        gp_widget_get_child_by_label (section, _("Auto Off Time"), &widget);
        if (gp_widget_changed (widget)) {
                gp_widget_get_value (widget, &f);
                C(k_set_preference (camera->port, context,
                                        K_PREFERENCE_AUTO_OFF_TIME, (int) f));
        }

        /* Slide Show Interval */
        gp_widget_get_child_by_label (section, _("Slide Show Interval"),
                                      &widget);
        if (gp_widget_changed (widget)) {
                gp_widget_get_value (widget, &f);
                C(k_set_preference (camera->port, context,
                                K_PREFERENCE_SLIDE_SHOW_INTERVAL, (int) f));
        }

        /* Resolution */
        gp_widget_get_child_by_label (section, _("Resolution"), &widget);
        if (gp_widget_changed (widget)) {
                gp_widget_get_value (widget, &c);
                if (!strcmp (c, _("High (1152 x 872)")))
                        j = 1;
                else if (!strcmp (c, _("Low (576 x 436)")))
                        j = 3;
                else
                        j = 0;
                C(k_set_preference (camera->port, context,
                                        K_PREFERENCE_RESOLUTION, j));
        }

        /****************/
        /* Localization */
        /****************/
        gp_widget_get_child_by_label (window, _("Localization"), &section);

        /* Localization File */
        C(gp_widget_get_child_by_label (section, _("Language"), &widget));
	C(result = gp_widget_changed (widget));
	if (result) {
		C(gp_widget_get_value (widget, &c));
                if (strcmp (c, _("None selected"))) {
                        data = NULL;
                        data_size = 0;

                        /* Read localization file */
                        result = localization_file_read (camera, c,
                                                         &data, &data_size,
							 context);
                        if (result != GP_OK) {
                                free (data);
                                return (result);
                        }

                        /* Go! */
                        result = k_localization_data_put (camera->port, context,
                                                          data, data_size);
                        free (data);
                        C(result);
                }
        }

        /* TV Output Format */
        gp_widget_get_child_by_label (section, _("TV Output Format"), &widget);
        if (gp_widget_changed (widget)) {
                gp_widget_get_value (widget, &c);
                if (strcmp (c, _("None selected"))) {
                        if (!strcmp (c, _("NTSC")))
                                tv_output_format = K_TV_OUTPUT_FORMAT_NTSC;
                        else if (!strcmp (c, _("PAL")))
                                tv_output_format = K_TV_OUTPUT_FORMAT_PAL;
                        else if (!strcmp (c, _("Do not display TV menu")))
                                tv_output_format = K_TV_OUTPUT_FORMAT_HIDE;
                        else
				return (GP_ERROR);
                        C(k_localization_tv_output_format_set (
                                        camera->port, context, tv_output_format));
                }
        }

        /* Date Format */
        gp_widget_get_child_by_label (section, _("Date Format"), &widget);
        if (gp_widget_changed (widget)) {
                gp_widget_get_value (widget, &c);
                if (strcmp (c, _("None selected"))) {
                        if (!strcmp (c, _("Month/Day/Year")))
                                date_format = K_DATE_FORMAT_MONTH_DAY_YEAR;
                        else if (!strcmp (c, _("Day/Month/Year")))
                                date_format = K_DATE_FORMAT_DAY_MONTH_YEAR;
                        else if (!strcmp (c, _("Year/Month/Day")))
                                date_format = K_DATE_FORMAT_YEAR_MONTH_DAY;
                        else
				return (GP_ERROR);
                        C(k_localization_date_format_set (
						camera->port, context, date_format));
                }
        }

        /********************************/
        /* Session-persistent Settings  */
        /********************************/
        gp_widget_get_child_by_label (window, _("Session-persistent Settings"),
                                      &section);

        /* Flash */
        gp_widget_get_child_by_label (section, _("Flash"), &widget);
        if (gp_widget_changed (widget)) {
                gp_widget_get_value (widget, &c);
                if (!strcmp (c, _("Off")))
                        j = 0;
                else if (!strcmp (c, _("On")))
                        j = 1;
                else if (!strcmp (c, _("On, red-eye reduction")))
                        j = 5;
                else if (!strcmp (c, _("Auto")))
                        j = 2;
                else
                        j = 6;
                C(k_set_preference (camera->port, context,
						 K_PREFERENCE_FLASH, j));
        }

        /* Exposure */
        gp_widget_get_child_by_label (section, _("Exposure"), &widget);
        if (gp_widget_changed (widget)) {
                gp_widget_get_value (widget, &f);
                C(k_set_preference (camera->port, context,
                                        K_PREFERENCE_EXPOSURE, (int) f));
        }

        /* Focus will be set together with self timer. */
        gp_widget_get_child_by_label (section, _("Focus"), &widget_focus);

        /************************/
        /* Volatile Settings    */
        /************************/
        gp_widget_get_child_by_label (window, _("Volatile Settings"), &section);

        /* Self Timer (and Focus) */
        gp_widget_get_child_by_label (section, _("Self Timer"),
                                      &widget_self_timer);
        if (gp_widget_changed (widget_focus) &&
            gp_widget_changed (widget_self_timer)) {
                gp_widget_get_value (widget_focus, &c);
                if (!strcmp (c, _("Auto")))
                        focus_self_timer = 2;
                else
                        focus_self_timer = 0;
                gp_widget_get_value (widget_self_timer, &c);
                if (!strcmp (c, _("Self Timer (next picture only)")))
                        focus_self_timer++;
                C(k_set_preference (camera->port, context,
                        K_PREFERENCE_FOCUS_SELF_TIMER, focus_self_timer));
        }

        /* We are done. */
        return (GP_OK);
}

int
localization_file_read (Camera *camera, const char *file_name,
                        unsigned char **data, long int *data_size,
			GPContext *context)
{
        FILE *file;
        unsigned int j;
        int f;
        unsigned char c[] = "\0\0";
        unsigned long line_number;
        unsigned int d;
        char path[1024];

        strcpy (path, LOCALIZATION);
        strcat (path, "/");
        strcat (path, file_name);
	gp_log (GP_LOG_DEBUG, "konica", "Uploading '%s'...", path);
        file = fopen (path, "r");
        if (!file) {
		gp_context_error (context, _("Could not find localization "
			"data at '%s'"), path);
                return (GP_ERROR_FILE_NOT_FOUND);
	}

        /* Allocate the memory */
        *data_size = 0;
        *data = malloc (sizeof (char) * 65536);
        if (!*data) {
		fclose (file);
                return (GP_ERROR_NO_MEMORY);
	}

        j = 0;
        line_number = 1;
        do {
                f = fgetc (file);
                switch (f) {
                case '\n':
                        line_number++;
                        continue;
                case EOF:
                        break;
                case '#':
                        /************************/
                        /* Comment: Discard.    */
                        /************************/
                        do {
                                f = fgetc (file);
                        } while ((f != '\n') && (f != EOF));
                        if (f == '\n') line_number++;
                        continue;
                case '\t':
                        continue;
                case ' ':
                        continue;
                default:
                        /****************************************/
                        /* j == 0: We have to read the second   */
                        /* half of the byte to send.            */
                        /* j == 1: We'll compose our byte.      */
                        /****************************************/
                        if ((f != '0') && (f != '1') && (f != '2') &&
                            (f != '3') && (f != '4') && (f != '5') &&
                            (f != '6') && (f != '7') && (f != '8') &&
                            (f != '9') && (f != 'A') && (f != 'B') &&
                            (f != 'C') && (f != 'D') && (f != 'E') &&
                            (f != 'F')) {
                                GP_DEBUG ("Error in localization "
					  "file: '%c' in line %i is "
					  "not allowed.", f,
					  (int) line_number);
                                fclose (file);
                                return (GP_ERROR_CORRUPTED_DATA);
                        }
                        c[j] = (char) f;
                        if (j == 1) {
                                if (sscanf ((char *)&c[0], "%X", &d) != 1) {
					GP_DEBUG ("Error in localization "
						  "file.");
                                	fclose (file);
                                        return (GP_ERROR_CORRUPTED_DATA);
                                }
                                (*data)[*data_size] = d;
                                (*data_size)++;
                                if (*data_size == 65536) {
                                        gp_context_error (context,
						_("Localization file too long!"));
                                        fclose (file);
                                        return (GP_ERROR_CORRUPTED_DATA);
                                }
                        }
                        j = 1 - j;
                        continue;
                }
        } while (f != EOF);
        fclose (file);

        /* Calculate and check checksum. */
	gp_log (GP_LOG_DEBUG, "konica", "Checksum not implemented!");
        /*FIXME: There's a checksum at (*data)[100]. I could not
          figure out how it is calculated. */

        /* Calculate and check frame check sequence. */

	gp_log (GP_LOG_DEBUG, "konica", "Frame check sequence "
		"not implemented!");
        /* FIXME: There's a frame check sequence at (*data)[108]
           and (*data)[109]. I could not figure out how it is
           calculated. */
        gp_log (GP_LOG_DEBUG, "konica", "-> %i bytes read.\n",
		(int) *data_size);
        return (GP_OK);
}

static int
camera_pre_func (Camera *camera, GPContext *context)
{
	/* Set best speed */
	set_speed (camera, 0, context);

	return (GP_OK);
}

static int
camera_post_func (Camera *camera, GPContext *context)
{
	/* Set default speed */
	set_speed (camera, 9600, context);

	return (GP_OK);
}

static CameraFilesystemFuncs fsfuncs = {
	.get_info_func = get_info_func,
	.set_info_func = set_info_func,
	.file_list_func = file_list_func,
	.get_file_func = get_file_func,
	.del_file_func = delete_file_func,
	.delete_all_func = delete_all_func,
};

int
camera_init (Camera* camera, GPContext *context)
{
        int i, speed;
        GPPortSettings settings;
	CameraAbilities a;

        /* First, set up all the function pointers. */
	camera->functions->pre_func		= camera_pre_func;
	camera->functions->post_func		= camera_post_func;
        camera->functions->exit                 = camera_exit;
        camera->functions->get_config           = camera_get_config;
        camera->functions->set_config           = camera_set_config;
        camera->functions->capture              = camera_capture;
        camera->functions->capture_preview      = camera_capture_preview;
        camera->functions->summary              = camera_summary;
        camera->functions->about                = camera_about;

        /* Lookup model information */
	gp_camera_get_abilities (camera, &a);
        for (i = 0; konica_cameras [i].model; i++)
                if (!strcmp (konica_cameras [i].model, a.model))
                        break;
        if (!konica_cameras [i].model)
                return (GP_ERROR_MODEL_NOT_FOUND);

	/* Store some data we constantly need. */
	camera->pl = malloc (sizeof (CameraPrivateLibrary));
	memset (camera->pl, 0, sizeof (CameraPrivateLibrary));
	camera->pl->image_id_long = konica_cameras [i].image_id_long;

        /* Initiate the connection */
	C(gp_port_get_settings (camera->port, &settings));
        switch (camera->port->type) {
        case GP_PORT_SERIAL:

                /* Set up the device */
                settings.serial.bits = 8;
                settings.serial.parity = 0;
                settings.serial.stopbits = 1;
                C(gp_port_set_settings (camera->port, settings));

                /* Initiate the connection */
		C(speed = test_speed (camera, context));
#if 0
/* Ideally, we need to reset the speed to the speed that we encountered
   after each operation (multiple programs accessing the camera). However,
   that takes quite a bit of time for HP cameras... */
		camera->pl->speed = speed;
#endif

                break;
        case GP_PORT_USB:

		/* Use the defaults the core parsed */
                C(gp_port_set_settings (camera->port, settings));

                /* Initiate the connection */
                C(k_init (camera->port, context));

                break;
        default:
                return (GP_ERROR_UNKNOWN_PORT);
        }
        /* Set up the filesystem */
	C(gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera));

	/* Ping the camera every minute to prevent shut-down. */
	camera->pl->timeout = gp_camera_start_timeout (camera, PING_TIMEOUT,
						       timeout_func);
        return (GP_OK);
}

