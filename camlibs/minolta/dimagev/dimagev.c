/**********************************************************************;
*       Minolta Dimage V digital camera communication library         *
*               Copyright (C) 2000,2001 Gus Hartmann                  *
*                                                                     *
*    This program is free software; you can redistribute it and/or    *
*    modify it under the terms of the GNU General Public License as   *
*    published by the Free Software Foundation; either version 2 of   *
*    the License, or (at your option) any later version.              *
*                                                                     *
*    This program is distributed in the hope that it will be useful,  *
*    but WITHOUT ANY WARRANTY; without even the implied warranty of   *
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the    *
*    GNU General Public License for more details.                     *
*                                                                     *
*    You should have received a copy of the GNU General Public        *
*    License along with this program; if not, write to the Free       *
*    Software Foundation, Inc., 59 Temple Place, Suite 330,           *
*    Boston, MA 02111-1307 USA                                        *
*                                                                     *
**********************************************************************/

/* $Id$ */

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define _(String) (String)
#    define N_(String) (String)
#  endif
#else
#  define _(String) (String)
#  define N_(String) (String)
#endif

#include "dimagev.h"

int camera_id (CameraText *id) 
{
#if defined HAVE_STRNCPY
	strncpy(id->text, "minolta-dimagev", sizeof(id->text));
#else
	strcpy(id->text, "minolta-dimagev");
#endif

	return GP_OK;
}

int camera_abilities (CameraAbilitiesList *list) 
{
	CameraAbilities *a;

	gp_abilities_new(&a);

#if defined HAVE_STRNCPY
	strncpy(a->model, "Minolta Dimage V", sizeof(a->model));
#else
	strcpy(a->model, "Minolta Dimage V");
#endif
	a->port     = GP_PORT_SERIAL;
	a->speed[0] = 38400;
	a->speed[1] = 0;
	a->operations        = 	GP_OPERATION_CAPTURE_IMAGE |
				GP_OPERATION_CAPTURE_PREVIEW;
	a->file_operations   = 	GP_FILE_OPERATION_DELETE | 
				GP_FILE_OPERATION_PREVIEW;
	a->folder_operations = 	GP_FOLDER_OPERATION_PUT_FILE | 
				GP_FOLDER_OPERATION_DELETE_ALL;

	gp_abilities_list_append(list, a);

	return GP_OK;
}

int camera_init (Camera *camera) 
{
        int ret;
        dimagev_t *dimagev = NULL;

	/* First, set up all the function pointers */
	camera->functions->id 			= camera_id;
	camera->functions->abilities 		= camera_abilities;
	camera->functions->init 		= camera_init;
	camera->functions->exit 		= camera_exit;
	camera->functions->folder_list_folders 	= camera_folder_list_folders;
	camera->functions->folder_list_files   	= camera_folder_list_files;
	camera->functions->file_get 		= camera_file_get;
	camera->functions->file_delete 		= camera_file_delete;
	camera->functions->folder_put_file      = camera_folder_put_file;
	camera->functions->folder_delete_all    = camera_folder_delete_all;
	camera->functions->capture 		= camera_capture;
	camera->functions->summary		= camera_summary;
	camera->functions->manual 		= camera_manual;
	camera->functions->about 		= camera_about;

	gp_debug_printf(GP_DEBUG_LOW, "dimagev", "initializing the camera");

	if ( ( dimagev = (dimagev_t*) malloc(sizeof(dimagev_t))) == NULL ) {
		gp_debug_printf(GP_DEBUG_LOW, "dimagev", "camera_init::unable to allocate dimagev_t");
		return GP_ERROR_NO_MEMORY;
	}

	camera->camlib_data = dimagev;

	/* Now open a port. */
	if (( ret = gp_port_new(&(dimagev->dev), GP_PORT_SERIAL)) < 0 ) {
		gp_debug_printf(GP_DEBUG_LOW, "dimagev", "camera_init::unable to allocate gp_port_dev");
		return ret;
	}

	gp_port_timeout_set(dimagev->dev, 5000);

#if defined HAVE_STRNCPY
	strncpy(dimagev->settings.serial.port, camera->port->path, sizeof(dimagev->settings.serial.port));
#else
	strcpy(dimagev->settings.serial.port, camera->port->path);
#endif
	dimagev->settings.serial.speed = 38400;
	dimagev->settings.serial.bits = 8;
	dimagev->settings.serial.parity = 0;
	dimagev->settings.serial.stopbits = 1;

	if (( ret = gp_filesystem_new(&dimagev->fs)) < GP_OK ) {
		gp_debug_printf(GP_DEBUG_LOW, "dimagev", "camera_init::unable to allocate filesystem");
		return (ret);
	}

	gp_port_settings_set(dimagev->dev, dimagev->settings);
	gp_port_open(dimagev->dev);

	if  ( dimagev_get_camera_data(dimagev) < GP_OK ) {
		gp_debug_printf(GP_DEBUG_LOW, "dimagev", "camera_init::unable to get current camera data");
		return GP_ERROR_IO;
	}

	if  ( dimagev_get_camera_status(dimagev) < GP_OK ) {
		gp_debug_printf(GP_DEBUG_LOW, "dimagev", "camera_init::unable to get current camera status");
		return GP_ERROR_IO;
	}

	/* Let's make this non-fatal. An incorrect date doesn't affect much. */
	if ( dimagev_set_date(dimagev) < GP_OK ) {
		gp_debug_printf(GP_DEBUG_LOW, "dimagev", "camera_init::unable to set camera to system time");
	}

	return GP_OK;
}

int camera_exit (Camera *camera) 
{
	dimagev_t *dimagev;

	dimagev = camera->camlib_data;

	/* Set the camera back into a normal mode. */

	if ( ( dimagev == NULL ) || ( dimagev->data == NULL ) ) {
		return GP_ERROR_BAD_PARAMETERS;
	}

	dimagev->data->host_mode = (unsigned char) 0;

	/* This will also send the host mode of zero. */
	if ( dimagev_set_date(dimagev) < GP_OK ) {
		gp_debug_printf(GP_DEBUG_LOW, "dimagev", "camera_init::unable to set camera to system time");
		return GP_ERROR_IO;
	}

	if ( dimagev->dev != NULL ) {
		gp_port_close(dimagev->dev);
		gp_port_free(dimagev->dev);
	}

	if ( dimagev->fs != NULL ) {
		gp_filesystem_free(dimagev->fs);
	}

	if ( dimagev->data != NULL ) {
		free(dimagev->data);
	}

	if ( dimagev->status != NULL ) {
		free(dimagev->status);
	}

/*
	if ( dimagev->info != NULL ) {
		free(dimagev->info);
	}
*/

	free(dimagev);

	return GP_OK;
}

int camera_folder_list_folders (Camera *camera, const char *folder, 
				CameraList *list) 
{
	/* Taking yet another lead from the Panasonic drivers, I'll just */
	/* return okay, since folders aren't supported on the Minolta Dimage V. */
	return GP_OK;
}

int camera_folder_list_files (Camera *camera, const char *folder, 
			      CameraList *list) 
{
	dimagev_t *dimagev;
	const char *name;
	int i=0;

	dimagev = camera->camlib_data;

	if ( dimagev_get_camera_status(dimagev) < GP_OK ) {
		gp_debug_printf(GP_DEBUG_LOW, "dimagev", "camera_file_list::unable to get camera status");
		return GP_ERROR_IO;
	}

	if ( gp_filesystem_populate(dimagev->fs, "/", DIMAGEV_FILENAME_FMT, dimagev->status->number_images) < GP_OK ) {
		gp_debug_printf(GP_DEBUG_LOW, "dimagev", "camera_file_list::unable to popukate filesystem");
		return GP_ERROR_NO_MEMORY;
	}
		

	for ( i = 0 ; i < dimagev->status->number_images ; i++ ) {
		gp_filesystem_name(dimagev->fs, "/", i, &name);
		gp_list_append(list, name, NULL);
	}

	return GP_OK;
}

int camera_file_get (Camera *camera, const char *folder, const char *filename, 
		     CameraFileType type, CameraFile *file) 
{
	dimagev_t *dimagev;
	int file_number=0, result;
	char buffer[128];

	dimagev = camera->camlib_data;

	file_number = gp_filesystem_number(dimagev->fs, folder, filename);
	if (file_number < 0)
		return (file_number);

	switch (type) {
	case GP_FILE_TYPE_NORMAL:
		gp_file_set_mime_type (file, "image/jpeg");
		gp_file_set_name (file, filename);
		result = dimagev_get_picture (dimagev, file_number + 1, file);
		break;
	case GP_FILE_TYPE_PREVIEW:
		gp_file_set_mime_type (file, "image/ppm");
#if defined HAVE_SNPRINTF
		snprintf(buffer, sizeof(buffer), DIMAGEV_THUMBNAIL_FMT, ( file_number + 1) );
#else
		sprintf(buffer, DIMAGEV_THUMBNAIL_FMT, ( file_number + 1) );
#endif 
		gp_file_set_name (file, buffer);
		result = dimagev_get_thumbnail (dimagev, file_number + 1, file);
		break;
	default:
		return (GP_ERROR_NOT_SUPPORTED);
	}

	if (result < 0) {
		gp_debug_printf(GP_DEBUG_LOW, "dimagev", "camera_file_get::unable to retrieve image file");
		return result;
	}

	gp_file_set_name (file, filename);

	sleep(2);
	return GP_OK;
}

int camera_file_delete (Camera *camera, const char *folder, 
			const char *filename) 
{
	dimagev_t *dimagev;
	int file_number=0;

	dimagev = camera->camlib_data;

	file_number = gp_filesystem_number(dimagev->fs, folder, filename);

	return dimagev_delete_picture(dimagev, (file_number + 1 ));
}

int camera_capture (Camera *camera, int capture_type, CameraFilePath *path) 
{
	dimagev_t *dimagev;

	dimagev=camera->camlib_data;

	switch ( capture_type ) {
		case GP_OPERATION_CAPTURE_VIDEO:
			gp_debug_printf(GP_DEBUG_LOW, "dimagev", "camera_capture::unable to capture video");
			return GP_ERROR_BAD_PARAMETERS;
		case GP_OPERATION_CAPTURE_PREVIEW: 
		case GP_OPERATION_CAPTURE_IMAGE:
			/* Proceed with the code below. Since the Dimage V doesn't support
			grabbing just the input (to the best of my knowledge), we take the
			picture either way. */
			break;
		default:
			gp_debug_printf(GP_DEBUG_LOW, "dimagev", "camera_capture::unkown capture type %02x", capture_type);
			return GP_ERROR_BAD_PARAMETERS;
		}

	if ( dimagev_shutter(dimagev) < GP_OK ) {
		gp_debug_printf(GP_DEBUG_LOW, "dimagev", "camera_capture::unable to open shutter");
		return GP_ERROR_IO;
	}

	/* Now check how many pictures are taken, and return the last one. */
	if ( dimagev_get_camera_status(dimagev) != 0 ) {
		gp_debug_printf(GP_DEBUG_LOW, "dimagev", "camera_capture::unable to get camera status");
		return GP_ERROR_IO;
	}

	if ( capture_type == GP_OPERATION_CAPTURE_PREVIEW ) {
#if defined HAVE_SNPRINTF
		snprintf(path->folder, sizeof(path->folder), "/");
		snprintf(path->name, sizeof(path->name), DIMAGEV_THUMBNAIL_FMT, dimagev->status->number_images);
#else
		sprintf(path->folder, "/");
		sprintf(path->name, DIMAGEV_THUMBNAIL_FMT, dimagev->status->number_images);
#endif
	} else if (capture_type == GP_OPERATION_CAPTURE_IMAGE) {
#if defined HAVE_SNPRINTF
		snprintf(path->folder, sizeof(path->folder), "/");
		snprintf(path->name, sizeof(path->name), DIMAGEV_FILENAME_FMT, dimagev->status->number_images);
#else
		sprintf(path->folder, "/");
		sprintf(path->name, DIMAGEV_FILENAME_FMT, dimagev->status->number_images);
#endif

	}

	return GP_OK;
}

int camera_folder_put_file (Camera *camera, const char *folder, 
			    CameraFile *file) 
{
	dimagev_t *dimagev;

	dimagev = camera->camlib_data;

	return dimagev_put_file(dimagev, file);
}

int camera_folder_delete_all (Camera *camera, const char *folder) 
{
	dimagev_t *dimagev;

	dimagev = camera->camlib_data;

	return dimagev_delete_all(dimagev);
}

int camera_summary (Camera *camera, CameraText *summary) 
{
	dimagev_t *dimagev;
	int i = 0, count = 0;

	dimagev = camera->camlib_data;

	if ( dimagev_get_camera_status(dimagev) < GP_OK ) {
		gp_debug_printf(GP_DEBUG_LOW, "dimagev", "camera_summary::unable to get camera status");
		return GP_ERROR_IO;
	}

	if ( dimagev_get_camera_data(dimagev) < GP_OK ) {
		gp_debug_printf(GP_DEBUG_LOW, "dimagev", "camera_summary::unable to get camera data");
		return GP_ERROR_IO;
	}

	if ( dimagev_get_camera_info(dimagev) < GP_OK ) {
		gp_debug_printf(GP_DEBUG_LOW, "dimagev", "camera_summary::unable to get camera info");
		return GP_ERROR_IO;
	}

	dimagev_dump_camera_status(dimagev->status);
	dimagev_dump_camera_data(dimagev->data);
	dimagev_dump_camera_info(dimagev->info);

	/* Now put all of the information into a reasonably formatted string. */
	/* i keeps track of the length. */
	i = 0;
	/* First the stuff from the dimagev_info_t */
#if defined HAVE_SNPRINTF
	i = snprintf(summary->text, sizeof(summary->text),
#else
	i = sprintf(summary->text,
#endif
		_("Model:\t\t\tMinolta Dimage V (%s)\n"
		"Hardware Revision:\t%s\nFirmware Revision:\t%s\n"),
		dimagev->info->model, dimagev->info->hardware_rev,
		dimagev->info->firmware_rev);

	if ( i > 0 ) {
		count += i;
	}

	/* Now the stuff from dimagev_data_t */
#if defined HAVE_SNPRINTF
	i = snprintf(&(summary->text[count]), sizeof(summary->text) - count,
#else
	i = sprintf(&(summary->text[count]),
#endif
		_("Host Mode:\t\t%s\n"
		"Exposure Correction:\t%s\n"
		"Exposure Data:\t\t%d\n"
		"Date Valid:\t\t%s\n"
		"Date:\t\t\t%d/%02d/%02d %02d:%02d:%02d\n"
		"Self Timer Set:\t\t%s\n"
		"Quality Setting:\t%s\n"
		"Play/Record Mode:\t%s\n"
		"Card ID Valid:\t\t%s\n"
		"Card ID:\t\t%d\n"
		"Flash Mode:\t\t"),

		( dimagev->data->host_mode != (unsigned char) 0 ? _("Remote") : _("Local") ),
		( dimagev->data->exposure_valid != (unsigned char) 0 ? _("Yes") : _("No") ),
		(signed char)dimagev->data->exposure_correction,
		( dimagev->data->date_valid != (unsigned char) 0 ? _("Yes") : _("No") ),
		( dimagev->data->year < (unsigned char) 70 ? 2000 + (int) dimagev->data->year : 1900 + (int) dimagev->data->year ),
		dimagev->data->month, dimagev->data->day, dimagev->data->hour,
		dimagev->data->minute, dimagev->data->second,
		( dimagev->data->self_timer_mode != (unsigned char) 0 ? _("Yes") : _("No")),
		( dimagev->data->quality_setting != (unsigned char) 0 ? _("Fine") : _("Standard") ),
		( dimagev->data->play_rec_mode != (unsigned char) 0 ? _("Record") : _("Play") ),
		( dimagev->data->valid != (unsigned char) 0 ? _("Yes") : _("No")), dimagev->data->id_number );

	if ( i > 0 ) {
		count += i;
	}

	/* Flash is a special case, a switch is needed. */
	switch ( dimagev->data->flash_mode ) {
		case 0:
#if defined HAVE_SNPRINTF
			i = snprintf(&(summary->text[count]), sizeof(summary->text) - count,
#else
			i = sprintf(&(summary->text[count]),
#endif
				_("Automatic\n"));
			break;
		case 1:
#if defined HAVE_SNPRINTF
			i = snprintf(&(summary->text[count]), sizeof(summary->text) - count,
#else
			i = sprintf(&(summary->text[count]),
#endif
				_("Force Flash\n"));
			break;
		case 2:
#if defined HAVE_SNPRINTF
			i = snprintf(&(summary->text[count]), sizeof(summary->text) - count,
#else
			i = sprintf(&(summary->text[count]),
#endif
				_("Prohibit Flash\n"));
			break;
		default:
#if defined HAVE_SNPRINTF
			i = snprintf(&(summary->text[count]), sizeof(summary->text) - count,
#else
			i = sprintf(&(summary->text[count]),
#endif
				_("Invalid Value ( %d )\n"), dimagev->data->flash_mode);
			break;
	}

	if ( i > 0 ) {
		count += i;
	}

	/* Now for the information in dimagev_status_t */
#if defined HAVE_SNPRINTF
	i = snprintf(&(summary->text[count]), sizeof(summary->text) - count,
#else
	i = sprintf(&(summary->text[count]),
#endif
		_("Battery Level:\t\t%s\n"
		"Number of Images:\t%d\n"
		"Minimum Capacity Left:\t%d\n"
		"Busy:\t\t\t%s\n"
		"Flash Charging:\t\t%s\n"
		"Lens Status:\t\t"),
		( dimagev->status->battery_level ? _("Not Full") : _("Full") ),
		dimagev->status->number_images,
		dimagev->status->minimum_images_can_take,
		( dimagev->status->busy ? _("Busy") : _("Idle") ),
		( dimagev->status->flash_charging ? _("Charging") : _("Ready") )

	);

	if ( i > 0 ) {
		count += i;
	}

	/* Lens status is another switch. */
	switch ( dimagev->status->lens_status ) {
		case 0:
			i = snprintf(&(summary->text[count]), sizeof(summary->text) - count, _("Normal\n"));
			break;
		case 1: case 2:
			i = snprintf(&(summary->text[count]), sizeof(summary->text) - count, _("Lens direction does not match flash light\n"));
			break;
		case 3:
			i = snprintf(&(summary->text[count]), sizeof(summary->text) - count, _("Lens is not connected\n"));
			break;
		default:
			i = snprintf(&(summary->text[count]), sizeof(summary->text) - count, _("Bad value for lens status %d\n"), dimagev->status->lens_status);
			break;
	}

	if ( i > 0 ) {
		count += i;
	}

	/* Card status is another switch. */
	i = snprintf(&(summary->text[count]), sizeof(summary->text) - count, _("Card Status:\t\t"));
	if ( i > 0 ) {
		count += i;
	}

	switch ( dimagev->status->card_status ) {
		case 0:
			i = snprintf(&(summary->text[count]), sizeof(summary->text) - count, _("Normal"));
			break;
		case 1:
			i = snprintf(&(summary->text[count]), sizeof(summary->text) - count, _("Full"));
			break;
		case 2:
			i = snprintf(&(summary->text[count]), sizeof(summary->text) - count, _("Write-protected"));
			break;
		case 3:
			i = snprintf(&(summary->text[count]), sizeof(summary->text) - count, _("Unsuitable card"));
			break;
		default:
			i = snprintf(&(summary->text[count]), sizeof(summary->text) - count, _("Bade value for card status %d"), dimagev->status->card_status);
			break;
	}
	
	if ( i > 0 ) {
		count += i;
	}


	return GP_OK;
}

int camera_manual (Camera *camera, CameraText *manual) 
{
#if defined HAVE_STRNCPY
	strncpy(manual->text, _("Manual not available."), sizeof(manual->text));
#else
	strcpy(manual->text, _("Manual not available."));
#endif
	return GP_OK;
}

int camera_about (Camera *camera, CameraText *about) 
{
#if defined HAVE_SNPRINTF
	snprintf(about->text, sizeof(about->text),
#else
	sprintf(about->text,
#endif
_("Minolta Dimage V Camera Library\n%s\nGus Hartmann <gphoto@gus-the-cat.org>\nSpecial thanks to Minolta for the spec."), DIMAGEV_VERSION);

	return GP_OK;
}
