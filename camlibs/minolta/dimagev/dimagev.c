/**********************************************************************
*       Minolta Dimage V digital camera communication library         *
*               Copyright (C) 2000 Gus Hartmann                       *
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

#include "dimagev.h"

int camera_id (CameraText *id) {

#if defined HAVE_STRNCPY
	strncpy(id->text, "minolta-dimagev", sizeof(id->text));
#else
	strcpy(id->text, "minolta-dimagev");
#endif

	return GP_OK;
}

int camera_abilities (CameraAbilitiesList *list) {

	CameraAbilities *a;

	a = gp_abilities_new();

#if defined HAVE_STRNCPY
	strncpy(a->model, "Minolta Dimage V", sizeof(a->model));
#else
	strcpy(a->model, "Minolta Dimage V");
#endif
	a->port     = GP_PORT_SERIAL;
	a->speed[0] = 38400;
	a->speed[1] = 0;
	a->capture  = 1;
	a->config   = 1;
	a->file_delete  = 1;
	a->file_preview = 1;
	a->file_put = 1;

	gp_abilities_list_append(list, a);

	return GP_OK;
}

int camera_init (Camera *camera, CameraInit *init) {
	dimagev_t *dimagev = NULL;

	/* First, set up all the function pointers */
	camera->functions->id 		= camera_id;
	camera->functions->abilities 	= camera_abilities;
	camera->functions->init 	= camera_init;
	camera->functions->exit 	= camera_exit;
	camera->functions->folder_list  = camera_folder_list;
	camera->functions->file_list	= camera_file_list;
	camera->functions->file_get 	= camera_file_get;
	camera->functions->file_get_preview =  camera_file_get_preview;
	camera->functions->file_put 	= NULL;
	camera->functions->file_delete 	= camera_file_delete;
/*	camera->functions->config_get   = camera_config_get;*/
/*	camera->functions->config_set   = camera_config_set;*/
	camera->functions->capture 	= camera_capture;
	camera->functions->summary	= camera_summary;
	camera->functions->manual 	= camera_manual;
	camera->functions->about 	= camera_about;

	if ( camera->debug != 0 ) {
		printf("dimagev: initializing the camera\n");
	}

	if ( ( dimagev = (dimagev_t*) malloc(sizeof(dimagev_t))) == NULL ) {
		if ( camera->debug != 0 ) {
			perror("camera_init::unable to allocate dimagev_t");
		}
		return GP_ERROR;
	}

	camera->camlib_data = dimagev;
	dimagev->debug = camera->debug;

	/* Now open a port. */
	if ( ( dimagev->dev = gpio_new(GPIO_DEVICE_SERIAL) ) == NULL ) {
		if ( camera->debug != 0 ) {
			perror("camera_init::unable to allocate gpio_dev");
		}
		return GP_ERROR;
	}

	gpio_set_timeout(dimagev->dev, 5000);

#if defined HAVE_STRNCPY
	strncpy(dimagev->settings.serial.port, init->port.path, sizeof(dimagev->settings.serial.port));
#else
	strcpy(dimagev->settings.serial.port, init->port.path);
#endif
	dimagev->settings.serial.speed = 38400;
	dimagev->settings.serial.bits = 8;
	dimagev->settings.serial.parity = 0;
	dimagev->settings.serial.stopbits = 1;

	if ( ( dimagev->fs = gp_filesystem_new()) == NULL ) {
		if ( camera->debug != 0 ) {
			perror("camera_init::unable to allocate filesystem");
		}
		return GP_ERROR;
	}

	gpio_set_settings(dimagev->dev, dimagev->settings);
	gpio_open(dimagev->dev);

	if  ( dimagev_get_camera_data(dimagev) == GP_ERROR ) {
		if ( camera->debug != 0 ) {
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "camera_init::unable to get current camera data");
		}
		return GP_ERROR;
	}

	if  ( dimagev_get_camera_status(dimagev) == GP_ERROR ) {
		if ( camera->debug != 0 ) {
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "camera_init::unable to get current camera status");
		}
		return GP_ERROR;
	}

	/* Let's make this non-fatal. An incorrect date doesn't affect much. */
	if ( dimagev_set_date(dimagev) == GP_ERROR ) {
		if ( camera->debug != 0 ) {
			gp_debug_printf(GP_DEBUG_LOW, "dimagev", "camera_init::unable to set camera to system time");
		}
	}

	return GP_OK;
}

int camera_exit (Camera *camera) {
	dimagev_t *dimagev;

	dimagev = camera->camlib_data;

	/* Set the camera back into a normal mode. */

	if ( ( dimagev == NULL ) || ( dimagev->data == NULL ) ) {
		return GP_ERROR;
	}

	dimagev->data->host_mode = 0;

	/* This will also send the host mode of zero. */
	if ( dimagev_set_date(dimagev) == GP_ERROR ) {
		if ( camera->debug != 0 ) {
			gp_debug_printf(GP_DEBUG_LOW, "dimagev", "camera_init::unable to set camera to system time");
		}
		return GP_ERROR;
	}

	if ( dimagev->dev != NULL ) {
		gpio_close(dimagev->dev);
		gpio_free(dimagev->dev);
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

	if ( dimagev->info != NULL ) {
		free(dimagev->info);
	}

	free(dimagev);

	return GP_OK;
}

int camera_folder_list	(Camera *camera, CameraList *list, char *folder) {
	/* Taking yet another lead from the Panasonic drivers, I'll just */
	/* return okay, since folders aren't supported on the Minolta Dimage V. */
	return GP_OK;
}

int camera_file_list (Camera *camera, CameraList *list, char *folder) {
	dimagev_t *dimagev;
	int i=0;

	dimagev = camera->camlib_data;

	if ( dimagev_get_camera_status(dimagev) != GP_OK ) {
		if ( camera->debug != 0 ) {
			perror("camera_file_list::unable to get camera status");
		}
		return GP_ERROR;
	}

	gp_filesystem_populate(dimagev->fs, "/", DIMAGEV_FILENAME_FMT, dimagev->status->number_images);

	for ( i = 0 ; i < dimagev->status->number_images ; i++ ) {
		gp_list_append(list, gp_filesystem_name(dimagev->fs, "/", i), GP_LIST_FILE);
	}

	return GP_OK;
}

int camera_file_get (Camera *camera, CameraFile *file, char *folder, char *filename) {
	dimagev_t *dimagev;
	int file_number=0;

	dimagev = camera->camlib_data;

	file_number = gp_filesystem_number(dimagev->fs, folder, filename);

	if ( dimagev_get_picture(dimagev, file_number + 1, file) == GP_ERROR ) {
		if ( dimagev->debug != 0 ) {
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "camera_file_get::unable to retrieve image file");
		}
		return GP_ERROR;
	}

#if defined HAVE_SNPRINTF
	snprintf(file->type, sizeof(file->type), "image/jpg");
	snprintf(file->name, sizeof(file->name), "%s", filename);
#else
	sprintf(file->type, "image/jpg");
	sprintf(file->name, "%s", filename);
#endif

	return GP_OK;
}

int camera_file_get_preview (Camera *camera, CameraFile *file,
			     char *folder, char *filename) {

	dimagev_t *dimagev;
	int file_number=0;

	dimagev = camera->camlib_data;

	file_number = gp_filesystem_number(dimagev->fs, folder, filename);

	if ( dimagev_get_thumbnail(dimagev, file_number + 1, file) == GP_ERROR ) {
		if ( dimagev->debug != 0 ) {
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "camera_file_get_preview::unable to retrieve image file");
		}
		return GP_ERROR;
	}

	/* The previews are stored in PPM format. See util.c for more details. */
#if defined HAVE_SNPRINTF
	snprintf(file->type, sizeof(file->type), "image/ppm");
	snprintf(file->name, sizeof(file->name), DIMAGEV_THUMBNAIL_FMT, ( file_number + 1) );
#else
	sprintf(file->type, "image/ppm");
	sprintf(file->name, DIMAGEV_THUMBNAIL_FMT, ( file_number + 1) );
#endif
	return GP_OK;
}

int camera_file_put (Camera *camera, CameraFile *file, char *folder) {

	return GP_OK;
}

int camera_file_delete (Camera *camera, char *folder, char *filename) {
	dimagev_t *dimagev;
	int file_number=0;

	dimagev = camera->camlib_data;

	file_number = gp_filesystem_number(dimagev->fs, folder, filename);

	return dimagev_delete_picture(dimagev, (file_number + 1 ));
}

int camera_config_get (Camera *camera, CameraWidget *window) {
	printf("Getting config.\n");

	return GP_OK;
}

int camera_config_set (Camera *camera, CameraSetting *conf, int count) {

	return GP_OK;
}

int camera_capture (Camera *camera, CameraFile *file, CameraCaptureInfo *info) {

	dimagev_t *dimagev;

	dimagev=camera->camlib_data;

	switch ( info->type ) {
		case GP_CAPTURE_VIDEO:
			if ( dimagev->debug != 0 ) {
				gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "camera_capture::unable to capture video");
			}
			return GP_ERROR;
			break;
		case GP_CAPTURE_PREVIEW: case GP_CAPTURE_IMAGE:
			/* Proceed with the code below. Since the Dimage V doesn't support
			grabbing just the input (to the best of my knowledge), we take the
			picture either way. */
			break;
		default:
			if ( dimagev->debug != 0 ) {
				gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "camera_capture::unkown capture type %02x", info->type);
			}
			return GP_ERROR;
			break;
		}

	if ( dimagev_shutter(dimagev) == GP_ERROR ) {
		if ( dimagev->debug != 0 ) {
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "camera_capture::unable to open shutter");
		}
		return GP_ERROR;
	}

	/* Now check how many pictures are taken, and return the last one. */
	if ( dimagev_get_camera_status(dimagev) ) {
		if ( dimagev->debug != 0 ) {
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "camera_capture::unable to get camera status");
		}
		return GP_ERROR;
	}

	if ( info->type == GP_CAPTURE_PREVIEW ) {
		if ( dimagev_get_thumbnail(dimagev, ( dimagev->status->number_images ), file ) == GP_ERROR ) {
			if ( dimagev->debug != 0 ) {
				gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "camera_capture::unable to retrieve thumbnail");
			}
			return GP_ERROR;
		}

		/* Always name the image 0, for predictablity reasons. */
#if defined HAVE_SNPRINTF
		snprintf(file->type, sizeof(file->type), "image/ppm");
		snprintf(file->name, sizeof(file->name), DIMAGEV_THUMBNAIL_FMT, 0);
#else
		sprintf(file->type, "image/ppm");
		sprintf(file->name, DIMAGEV_THUMBNAIL_FMT, 0);
#endif

	}
	else if ( info->type == GP_CAPTURE_IMAGE ) {
		if ( dimagev_get_picture(dimagev, ( dimagev->status->number_images ), file ) == GP_ERROR ) {
			if ( dimagev->debug != 0 ) {
				gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "camera_capture::unable to retrieve new image");
			}
			return GP_ERROR;
		}

#if defined HAVE_SNPRINTF
		snprintf(file->type, sizeof(file->type), "image/jpg");
		snprintf(file->name, sizeof(file->name), DIMAGEV_FILENAME_FMT, 0);
#else
		sprintf(file->type, "image/jpg");
		sprintf(file->name, DIMAGEV_FILENAME_FMT, 0);
#endif

	}
	
	/* Now delete it. */
	/* If deletion fails, don't bother returning an error, just print an error */

	if ( dimagev_delete_picture(dimagev, dimagev->status->number_images ) == GP_ERROR ) {
		if ( dimagev->debug != 0 ) {
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "camera_capture::unable to delete new image");
		}
		printf("Unable to delete image. Please delete image %d\n", dimagev->status->number_images);
		return GP_ERROR_NONCRITICAL;
	}

	return GP_OK;
}

int camera_summary (Camera *camera, CameraText *summary) {
	dimagev_t *dimagev;
	int i = 0, count = 0;

	dimagev = camera->camlib_data;

	if ( dimagev_get_camera_status(dimagev) != GP_OK ) {
		if ( camera->debug != 0 ) {
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "camera_summary::unable to get camera status");
		}
		return GP_ERROR;
	}

	if ( dimagev_get_camera_data(dimagev) != GP_OK ) {
		if ( camera->debug != 0 ) {
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "camera_summary::unable to get camera data");
		}
		return GP_ERROR;
	}

	if ( dimagev_get_camera_info(dimagev) != GP_OK ) {
		if ( camera->debug != 0 ) {
			gp_debug_printf(GP_DEBUG_HIGH, "dimagev", "camera_summary::unable to get camera info");
		}
		return GP_ERROR;
	}

	if ( camera->debug != 0 ) {
		dimagev_dump_camera_status(dimagev->status);
		dimagev_dump_camera_data(dimagev->data);
		dimagev_dump_camera_info(dimagev->info);
	}

	/* Now put all of the information into a reasonably formatted string. */
	/* i keeps track of the length. */
	i = 0;
	/* First the stuff from the dimagev_info_t */
#if defined HAVE_SNPRINTF
	i = snprintf(summary->text, sizeof(summary->text),
#else
	i = snprintf(summary->text,
#endif
		"\t\tGeneral Information\nModel:\t\t\t%s Dimage V (%s)\n"
		"Hardware Revision:\t%s\nFirmware Revision:\t%s\n\n",
		dimagev->info->vendor, dimagev->info->model,
		dimagev->info->hardware_rev, dimagev->info->firmware_rev);

	if ( i > 0 ) {
		count += i;
	}

	/* Now the stuff from dimagev_data_t */
#if defined HAVE_SNPRINTF
	i = snprintf(&(summary->text[count]), sizeof(summary->text) - count,
#else
	i = sprintf(&(summary->text[count]),
#endif
		"\t\tCurrent Information\n"
		"Host Mode:\t\t%s\n"
		"Exposure Correction:\t%s\n"
		"Exposure Data:\t\t%d\n"
		"Date Valid:\t\t%s\n"
		"Date:\t\t\t%d/%02d/%02d %02d:%02d:%02d\n"
		"Self Timer Set:\t\t%s\n"
		"Quality Setting:\t%s\n"
		"Play/Record Mode:\t%s\n"
		"Card ID Valid:\t\t%s\n"
		"Card ID:\t\t%d\n"
		"Flash Mode:\t\t",

		( dimagev->data->host_mode ? "Remote" : "Local" ),
		( dimagev->data->exposure_valid ? "Yes" : "No" ),
		(signed char)dimagev->data->exposure_correction,
		( dimagev->data->date_valid ? "Yes" : "No" ),
		( dimagev->data->year < 70 ? 2000 + dimagev->data->year : 1900 + dimagev->data->year ),
		dimagev->data->month, dimagev->data->day, dimagev->data->hour,
		dimagev->data->minute, dimagev->data->second,
		( dimagev->data->self_timer_mode ? "Yes" : "No"),
		( dimagev->data->quality_setting ? "Fine" : "Standard" ),
		( dimagev->data->play_rec_mode ? "Record" : "Play" ),
		( dimagev->data->valid ? "Yes" : "No"),
		dimagev->data->id_number );

	if ( i > 0 ) {
		count += i;
	}

	/* Flash is a special case, a switch is needed. */
	switch ( dimagev->data->flash_mode ) {
		case 0:
			i = snprintf(&(summary->text[count]), sizeof(summary->text) - count, "Automatic\n");
			break;
		case 1:
			i = snprintf(&(summary->text[count]), sizeof(summary->text) - count, "Force Flash\n");
			break;
		case 2:
			i = snprintf(&(summary->text[count]), sizeof(summary->text) - count, "Prohibit Flash\n");
			break;
		default:
			i = snprintf(&(summary->text[count]), sizeof(summary->text) - count, "Invalid Value ( %d )\n", dimagev->data->flash_mode);
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
		"Battery Level:\t\t%s\n"
		"Number of Images:\t%d\n"
		"Minimum Capacity Left:\t%d\n"
		"Busy:\t\t\t%s\n"
		"Flash Charging:\t\t%s\n"
		"Lens Status:\t\t",
		( dimagev->status->battery_level ? "Not Full" : "Full" ),
		dimagev->status->number_images,
		dimagev->status->minimum_images_can_take,
		( dimagev->status->busy ? "Busy" : "Idle" ),
		( dimagev->status->flash_charging ? "Charging" : "Ready" )

	);

	if ( i > 0 ) {
		count += i;
	}

	/* Lens status is another switch. */
	switch ( dimagev->status->lens_status ) {
		case 0:
			i = snprintf(&(summary->text[count]), sizeof(summary->text) - count, "Normal\n");
			break;
		case 1: case 2:
			i = snprintf(&(summary->text[count]), sizeof(summary->text) - count, "Lens direction does not match flash light\n");
			break;
		case 3:
			i = snprintf(&(summary->text[count]), sizeof(summary->text) - count, "Lens is not connected\n");
			break;
		default:
			i = snprintf(&(summary->text[count]), sizeof(summary->text) - count, "Bad value for lens status %d\n", dimagev->status->lens_status);
			break;
	}

	if ( i > 0 ) {
		count += i;
	}

	/* Card status is another switch. */
	i = snprintf(&(summary->text[count]), sizeof(summary->text) - count, "Card Status:\t\t");
	if ( i > 0 ) {
		count += i;
	}

	switch ( dimagev->status->card_status ) {
		case 0:
			i = snprintf(&(summary->text[count]), sizeof(summary->text) - count, "Normal\n");
			break;
		case 1:
			i = snprintf(&(summary->text[count]), sizeof(summary->text) - count, "Full\n");
			break;
		case 2:
			i = snprintf(&(summary->text[count]), sizeof(summary->text) - count, "Write-protected\n");
			break;
		case 3:
			i = snprintf(&(summary->text[count]), sizeof(summary->text) - count, "Unsuitable card\n");
			break;
		default:
			i = snprintf(&(summary->text[count]), sizeof(summary->text) - count, "Bade value for card status %d\n", dimagev->status->card_status);
			break;
	}
	
	if ( i > 0 ) {
		count += i;
	}


	return GP_OK;
}

int camera_manual (Camera *camera, CameraText *manual) {

#if defined HAVE_STRNCPY
	strncpy(manual->text, "Manual not available.", sizeof(manual->text));
#else
	strcpy(manual->text, "Manual not available.");
#endif
	return GP_OK;
}

int camera_about (Camera *camera, CameraText *about) {
#if defined HAVE_SNPRINTF
	snprintf(about->text, sizeof(about->text),
#else
	sprintf(about->text,
#endif
"Minolta Dimage V Camera Library
%s
Gus Hartmann <gphoto@gus-the-cat.org>
Special thanks to Minolta for the spec.", DIMAGEV_VERSION);

	return GP_OK;
}
