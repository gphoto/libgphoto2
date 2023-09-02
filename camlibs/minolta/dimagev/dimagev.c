/**********************************************************************;
*       Minolta Dimage V digital camera communication library         *
*               Copyright 2000,2001 Gus Hartmann                      *
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
*    License along with this program; if not, write to the *
*    Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
*    Boston, MA  02110-1301  USA
*                                                                     *
**********************************************************************/

#define _DEFAULT_SOURCE

#include "config.h"

#include <stdio.h>

#include "libgphoto2/i18n.h"

#include "dimagev.h"


#define GP_MODULE "dimagev"

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
	CameraAbilities a;

	memset(&a, 0, sizeof(a));
#if defined HAVE_STRNCPY
	strncpy(a.model, "Minolta:Dimage V", sizeof(a.model));
#else
	strcpy(a.model, "Minolta:Dimage V");
#endif
	a.status = GP_DRIVER_STATUS_PRODUCTION;
	a.port     = GP_PORT_SERIAL;
	a.speed[0] = 38400;
	a.speed[1] = 0;
	a.operations        = 	GP_OPERATION_CAPTURE_IMAGE |
				GP_OPERATION_CAPTURE_PREVIEW;
	a.file_operations   = 	GP_FILE_OPERATION_DELETE |
				GP_FILE_OPERATION_PREVIEW;
	a.folder_operations = 	GP_FOLDER_OPERATION_PUT_FILE |
				GP_FOLDER_OPERATION_DELETE_ALL;

	gp_abilities_list_append(list, a);

	return GP_OK;
}

static int camera_exit (Camera *camera, GPContext *context)
{
	/* Set the camera back into a normal mode. */
	if (camera->pl) {

		if (camera->pl->data) {
			camera->pl->data->host_mode = (unsigned char) 0;

			/* This will also send the host mode of zero. */
			if ( dimagev_set_date(camera->pl) < GP_OK ) {
				GP_DEBUG( "camera_init::unable to set camera to system time");
				return GP_ERROR_IO;
			}
			free (camera->pl->data);
			camera->pl->data = NULL;
		}

		if (camera->pl->status) {
			free (camera->pl->status);
			camera->pl->status = NULL;
		}

		if (camera->pl->info) {
			free (camera->pl->info);
			camera->pl->info = NULL;
		}

		free (camera->pl);
	}

	return GP_OK;
}

static int file_list_func (CameraFilesystem *fs, const char *folder,
			   CameraList *list, void *data, GPContext *context)
{
	Camera *camera = data;
	int ret;

	if ( dimagev_get_camera_status(camera->pl) < GP_OK ) {
		GP_DEBUG( "camera_file_list::unable to get camera status");
		return GP_ERROR_IO;
	}

	if ((ret = gp_list_populate(list, DIMAGEV_FILENAME_FMT, camera->pl->status->number_images)) < GP_OK ) {
		GP_DEBUG( "camera_file_list::unable to populate list");
		return ret;
	}

	return GP_OK;
}

static int get_file_func (CameraFilesystem *fs, const char *folder,
			  const char *filename, CameraFileType type,
			  CameraFile *file, void *data, GPContext *context)
{
	Camera *camera = data;
	int file_number=0, result;

	file_number = gp_filesystem_number(fs, folder, filename, context);
	if (file_number < 0)
		return (file_number);

	switch (type) {
	case GP_FILE_TYPE_NORMAL:
		gp_file_set_mime_type (file, GP_MIME_JPEG);
		result = dimagev_get_picture (camera->pl, file_number + 1, file);
		break;
	case GP_FILE_TYPE_PREVIEW:
		gp_file_set_mime_type (file, GP_MIME_PPM);
		result = dimagev_get_thumbnail (camera->pl, file_number + 1, file);
		break;
	default:
		return (GP_ERROR_NOT_SUPPORTED);
	}

	if (result < 0) {
		GP_DEBUG( "camera_file_get::unable to retrieve image file");
		return result;
	}

	sleep(2);
	return GP_OK;
}

static int delete_file_func (CameraFilesystem *fs, const char *folder,
			     const char *filename, void *data,
			     GPContext *context)
{
	Camera *camera = data;
	int file_number=0;

	file_number = gp_filesystem_number(camera->fs, folder, filename, context);
	if (file_number < 0)
		return (file_number);

	return dimagev_delete_picture(camera->pl, (file_number + 1 ));
}

static int camera_capture (Camera *camera, CameraCaptureType type, CameraFilePath *path, GPContext *context)
{
	if (type != GP_CAPTURE_IMAGE)
		return (GP_ERROR_NOT_SUPPORTED);

	if ( dimagev_shutter(camera->pl) < GP_OK ) {
		GP_DEBUG( "camera_capture::unable to open shutter");
		return GP_ERROR_IO;
	}

	/* Now check how many pictures are taken, and return the last one. */
	if ( dimagev_get_camera_status(camera->pl) != 0 ) {
		GP_DEBUG( "camera_capture::unable to get camera status");
		return GP_ERROR_IO;
	}

#if defined HAVE_SNPRINTF
	snprintf(path->folder, sizeof(path->folder), "/");
	snprintf(path->name, sizeof(path->name), DIMAGEV_FILENAME_FMT, camera->pl->status->number_images);
#else
	sprintf(path->folder, "/");
	sprintf(path->name, DIMAGEV_FILENAME_FMT, camera->pl->status->number_images);
#endif

	/* Tell the CameraFilesystem about this picture */
	gp_filesystem_append (camera->fs, path->folder, path->name, context);

	return GP_OK;
}

static int put_file_func (CameraFilesystem *fs, const char *folder, const char *name,
			  CameraFileType type, CameraFile *file, void *data, GPContext *context)
{
	Camera *camera = data;

	return dimagev_put_file(camera->pl, file);
}

static int delete_all_func (CameraFilesystem *fs, const char *folder,
			    void *data, GPContext *context)
{
	Camera *camera = data;

	return dimagev_delete_all(camera->pl);
}

static int camera_summary (Camera *camera, CameraText *summary, GPContext *context)
{
	int i = 0, count = 0;

	if ( dimagev_get_camera_status(camera->pl) < GP_OK ) {
		GP_DEBUG( "camera_summary::unable to get camera status");
		return GP_ERROR_IO;
	}

	if ( dimagev_get_camera_data(camera->pl) < GP_OK ) {
		GP_DEBUG( "camera_summary::unable to get camera data");
		return GP_ERROR_IO;
	}

	if ( dimagev_get_camera_info(camera->pl) < GP_OK ) {
		GP_DEBUG( "camera_summary::unable to get camera info");
		return GP_ERROR_IO;
	}

	dimagev_dump_camera_status(camera->pl->status);
	dimagev_dump_camera_data(camera->pl->data);
	dimagev_dump_camera_info(camera->pl->info);

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
		camera->pl->info->model, camera->pl->info->hardware_rev,
		camera->pl->info->firmware_rev);

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

		( camera->pl->data->host_mode != (unsigned char) 0 ? _("Remote") : _("Local") ),
		( camera->pl->data->exposure_valid != (unsigned char) 0 ? _("Yes") : _("No") ),
		(signed char)camera->pl->data->exposure_correction,
		( camera->pl->data->date_valid != (unsigned char) 0 ? _("Yes") : _("No") ),
		( camera->pl->data->year < (unsigned char) 70 ? 2000 + (int) camera->pl->data->year : 1900 + (int) camera->pl->data->year ),
		camera->pl->data->month, camera->pl->data->day, camera->pl->data->hour,
		camera->pl->data->minute, camera->pl->data->second,
		( camera->pl->data->self_timer_mode != (unsigned char) 0 ? _("Yes") : _("No")),
		( camera->pl->data->quality_setting != (unsigned char) 0 ? _("Fine") : _("Standard") ),
		( camera->pl->data->play_rec_mode != (unsigned char) 0 ? _("Record") : _("Play") ),
		( camera->pl->data->valid != (unsigned char) 0 ? _("Yes") : _("No")), camera->pl->data->id_number );

	if ( i > 0 ) {
		count += i;
	}

	/* Flash is a special case, a switch is needed. */
	switch ( camera->pl->data->flash_mode ) {
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
				_("Invalid Value ( %d )\n"), camera->pl->data->flash_mode);
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
		( camera->pl->status->battery_level ? _("Not Full") : _("Full") ),
		camera->pl->status->number_images,
		camera->pl->status->minimum_images_can_take,
		( camera->pl->status->busy ? _("Busy") : _("Idle") ),
		( camera->pl->status->flash_charging ? _("Charging") : _("Ready") )

	);

	if ( i > 0 ) {
		count += i;
	}

	/* Lens status is another switch. */
	switch ( camera->pl->status->lens_status ) {
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
			i = snprintf(&(summary->text[count]), sizeof(summary->text) - count, _("Bad value for lens status %d\n"), camera->pl->status->lens_status);
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

	switch ( camera->pl->status->card_status ) {
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
			i = snprintf(&(summary->text[count]), sizeof(summary->text) - count, _("Bad value for card status %d"), camera->pl->status->card_status);
			break;
	}

	if ( i > 0 ) {
		count += i;
	}


	return GP_OK;
}

static int camera_about (Camera *camera, CameraText *about, GPContext *context)
{
#if defined HAVE_SNPRINTF
	snprintf(about->text, sizeof(about->text),
#else
	sprintf(about->text,
#endif
_("Minolta Dimage V Camera Library\n%s\nGus Hartmann <gphoto@gus-the-cat.org>\nSpecial thanks to Minolta for the spec."), DIMAGEV_VERSION);

	return GP_OK;
}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.get_file_func = get_file_func,
	.del_file_func = delete_file_func,
	.put_file_func = put_file_func,
	.delete_all_func = delete_all_func,
};

int camera_init (Camera *camera, GPContext *context)
{
	GPPortSettings settings;

        /* First, set up all the function pointers */
        camera->functions->exit                 = camera_exit;
        camera->functions->capture              = camera_capture;
        camera->functions->summary              = camera_summary;
        camera->functions->about                = camera_about;

        GP_DEBUG( "initializing the camera");

	camera->pl = malloc (sizeof (CameraPrivateLibrary));
	if (!camera->pl)
		return (GP_ERROR_NO_MEMORY);
	memset (camera->pl, 0, sizeof (CameraPrivateLibrary));
	camera->pl->dev = camera->port;

	/* Configure the port */
        gp_port_set_timeout(camera->port, 5000);
	gp_port_get_settings(camera->port, &settings);
        settings.serial.speed = 38400;
        settings.serial.bits = 8;
        settings.serial.parity = 0;
        settings.serial.stopbits = 1;
        gp_port_set_settings(camera->port, settings);

        if  ( dimagev_get_camera_data(camera->pl) < GP_OK ) {
                GP_DEBUG( "camera_init::unable to get current camera data");
		free (camera->pl);
		camera->pl = NULL;
                return GP_ERROR_IO;
        }

        if  ( dimagev_get_camera_status(camera->pl) < GP_OK ) {
                GP_DEBUG( "camera_init::unable to get current camera status");
		free (camera->pl);
		camera->pl = NULL;
                return GP_ERROR_IO;
        }

	/* Apparently, trying to set the clock now leaves the camera in an
	   unstable state. Skipping it for now. */
        /* if ( dimagev_set_date(camera->pl) < GP_OK ) {
                GP_DEBUG( "camera_init::unable to set camera to system time");
        } */

	/* Set up the filesystem */
	gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera);
        return GP_OK;
}
