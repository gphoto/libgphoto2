/*
 * STV0680 Vision Camera Chipset Driver
 * Copyright (C) 2000 Adam Harrison <adam@antispin.org> 
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA   
 */
#include <string.h>
#include <gphoto2.h>
#include <gphoto2-port.h>

#include "stv0680.h"
#include "library.h"

int camera_id (CameraText *id) {

	strcpy(id->text, "STV0680");

	return (GP_OK);
}

int camera_abilities (CameraAbilitiesList *list) {

	CameraAbilities *a;

	a = gp_abilities_new();

	strcpy(a->model, "STV0680");
	a->port     = GP_PORT_SERIAL|GP_PORT_USB;
	a->speed[0] = 115200;
	a->speed[1] = 0;
	a->capture  = GP_CAPTURE_NONE;
	a->config   = 0;
	a->file_delete  = 0;
	a->file_preview = 1;
	a->file_put = 0;

	gp_abilities_list_append(list, a);

	return (GP_OK);
}

int camera_init (Camera *camera) {

	gp_port_settings gpiod_settings;
	struct stv0680_s *device;

	/* First, set up all the function pointers */
	camera->functions->id 		= camera_id;
	camera->functions->abilities 	= camera_abilities;
	camera->functions->init 	= camera_init;
	camera->functions->exit 	= camera_exit;
	camera->functions->folder_list  = camera_folder_list;
	camera->functions->file_list	= camera_file_list;
	camera->functions->file_get 	= camera_file_get;
	camera->functions->file_get_preview =  camera_file_get_preview;
	camera->functions->file_put 	= camera_file_put;
	camera->functions->file_delete 	= camera_file_delete;
	camera->functions->config       = camera_config;
	camera->functions->capture 	= camera_capture;
	camera->functions->summary	= camera_summary;
	camera->functions->manual 	= camera_manual;
	camera->functions->about 	= camera_about;
	camera->functions->result_as_string = camera_result_as_string;

	if((device = malloc(sizeof(struct stv0680_s))) == NULL) {
		return GP_ERROR_NO_MEMORY;
	}

	camera->camlib_data = device;

	/* open and configure serial port */
	device->gpiod = gp_port_new(GP_PORT_SERIAL);
	gp_port_timeout_set(device->gpiod, 1000);

	strcpy(gpiod_settings.serial.port, camera->port->path);
	gpiod_settings.serial.speed = camera->port->speed;
	gpiod_settings.serial.bits = 8;
	gpiod_settings.serial.parity = 0;
	gpiod_settings.serial.stopbits = 1;

	gp_port_settings_set(device->gpiod, gpiod_settings);
	gp_port_open(device->gpiod);

	/* create camera filesystem */
	device->fs = gp_filesystem_new();

	/* test camera */
	return stv0680_ping(device);
}

int camera_exit (Camera *camera) {

	struct stv0680_s *device = camera->camlib_data;

	/* close serial port */
	gp_port_close(device->gpiod);

	/* free camera filesystem */
	gp_filesystem_free(device->fs);

	return (GP_OK);
}

int camera_folder_list	(Camera *camera, CameraList *list, char *folder) {

	/* stv0680 has no folder support */

	return (GP_OK);
}

int camera_file_list (Camera *camera, CameraList *list, char *folder) {

	struct stv0680_s *device = camera->camlib_data;
	int i, count, result;

	result = stv0680_file_count(device, &count);
	if (result != GP_OK)
		return result;

	gp_filesystem_populate(device->fs, "/", "image%02i.pnm", count);

	for(i = 0; i < gp_filesystem_count(device->fs, folder); ++i)
		gp_list_append(list,
			       gp_filesystem_name(device->fs, folder, i),
			       GP_LIST_FILE);

	return (GP_OK);
}

int camera_file_get (Camera *camera, CameraFile *file, 
		     char *folder, char *filename) { 

	struct stv0680_s *device = camera->camlib_data;
	int image_no, count, result;

	strcpy(file->name, filename);
	strcpy(file->type, "image/pnm");

	result = stv0680_file_count(device, &count);
	if (result != GP_OK)
		return result;

	gp_filesystem_populate(device->fs, "/", "image%02i.pnm", count);

	image_no = gp_filesystem_number(device->fs, folder, filename);

	if(image_no < 0)
		return image_no;

	return stv0680_get_image(device, image_no, &file->data, (int*) &file->size);
}

int camera_file_get_preview (Camera *camera, CameraFile *file,
			     char *folder, char *filename) {

	struct stv0680_s *device = camera->camlib_data;
	int image_no, count, result;

	strcpy(file->name, filename);
	strcpy(file->type, "image/pnm");

	result = stv0680_file_count(device, &count);
	if (result != GP_OK)
		return result;

	gp_filesystem_populate(device->fs, "/", "image%02i.pnm", count);

	image_no = gp_filesystem_number(device->fs, folder, filename);

	if(image_no < 0)
		return image_no;

	return stv0680_get_image_preview(device, image_no,
					&file->data, (int*) &file->size);
}

int camera_file_put (Camera *camera, CameraFile *file, char *folder) {

	/* stv0680 has no put support */

	return (GP_ERROR_NOT_SUPPORTED);
}

int camera_file_delete (Camera *camera, char *folder, char *filename) {

	/* stv0680 has no delete support */

	return (GP_ERROR_NOT_SUPPORTED);
}

int camera_config (Camera *camera) {

	/* stv0680 has no configureable options */

	return (GP_ERROR_NOT_SUPPORTED);
}

int camera_capture (Camera *camera, CameraFile *file, CameraCaptureInfo *info) {

	/* XXX implement */

	return (GP_ERROR_NOT_SUPPORTED);
}

int camera_summary (Camera *camera, CameraText *summary) {

	strcpy(summary->text, "No summary available.");

	return (GP_OK);
}

int camera_manual (Camera *camera, CameraText *manual) {

	strcpy(manual->text, "No manual available.");

	return (GP_OK);
}

int camera_about (Camera *camera, CameraText *about) {

	strcpy(about->text, 
"STV0680 
 Adam Harrison <adam@antispin.org>
 Driver for cameras using the STV0680 processor ASIC.
 Protocol reverse engineered using CommLite Beta 5");

	return (GP_OK);
}

char* camera_result_as_string (Camera *camera, int result) {
	
	if (result >= 0) return ("This is not an error...");
	if (-result < 100) return gp_result_as_string (result);
	return ("This is a template specific error.");
}
