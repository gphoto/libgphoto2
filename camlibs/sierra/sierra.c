#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gphoto2.h>

#include "library.h"
#include "sierra.h"

#define TIMEOUT	   2000

int camera_start(Camera *camera);
int camera_stop(Camera *camera);

int camera_id (CameraText *id) 
{
	strcpy(id->text, "sierra");

	return (GP_OK);
}

SierraCamera sierra_cameras[] = {
	/* Camera Model,    USB(vendor id, product id, in endpoint, out endpoint) */ 
	{"Agfa ePhoto 307", 	0, 0, 0, 0 },
	{"Agfa ePhoto 780", 	0, 0, 0, 0 },
	{"Agfa ePhoto 780C", 	0, 0, 0, 0 },
	{"Agfa ePhoto 1280", 	0, 0, 0, 0 },
	{"Agfa ePhoto 1680", 	0, 0, 0, 0 },
	{"Apple QuickTake 200", 0, 0, 0, 0 },
	{"Chinon ES-1000", 	0, 0, 0, 0 },
	{"Epson PhotoPC 500", 	0, 0, 0, 0 },
	{"Epson PhotoPC 550", 	0, 0, 0, 0 },
	{"Epson PhotoPC 600", 	0, 0, 0, 0 },
	{"Epson PhotoPC 700", 	0, 0, 0, 0 },
	{"Epson PhotoPC 800", 	0, 0, 0, 0 },
	{"Nikon CoolPix 100", 	0, 0, 0, 0 },
	{"Nikon CoolPix 300", 	0, 0, 0, 0 },
	{"Nikon CoolPix 700", 	0, 0, 0, 0 },
	{"Nikon CoolPix 800", 	0, 0, 0, 0 },
        {"Nikon CoolPix 880",	0x04b0, 0x0103, 0x83, 0x04},
        {"Nikon CoolPix 900", 	0, 0, 0, 0 },
	{"Nikon CoolPix 900S", 	0, 0, 0, 0 },
	{"Nikon CoolPix 910", 	0, 0, 0, 0 },
	{"Nikon CoolPix 950", 	0, 0, 0, 0 },
	{"Nikon CoolPix 950S", 	0, 0, 0, 0 },
	{"Nikon CoolPix 990",	0x04b0, 0x0102, 0x83, 0x04},
	{"Olympus D-100Z", 	0, 0, 0, 0 },
	{"Olympus D-200L", 	0, 0, 0, 0 },
	{"Olympus D-220L", 	0, 0, 0, 0 },
	{"Olympus D-300L", 	0, 0, 0, 0 },
	{"Olympus D-320L", 	0, 0, 0, 0 },
	{"Olympus D-330R", 	0, 0, 0, 0 },
	{"Olympus D-340L", 	0, 0, 0, 0 },
	{"Olympus D-340R", 	0, 0, 0, 0 },
	{"Olympus D-360L", 	0, 0, 0, 0 },
	{"Olympus D-400L Zoom", 0, 0, 0, 0 },
	{"Olympus D-450Z", 	0, 0, 0, 0 },
	{"Olympus D-460Z", 	0, 0, 0, 0 },
	{"Olympus D-500L", 	0, 0, 0, 0 },
	{"Olympus D-600L", 	0, 0, 0, 0 },
	{"Olympus D-600XL", 	0, 0, 0, 0 },
	{"Olympus D-620L", 	0, 0, 0, 0 },
	{"Olympus C-400", 	0, 0, 0, 0 },
	{"Olympus C-400L", 	0, 0, 0, 0 },
	{"Olympus C-410", 	0, 0, 0, 0 },
	{"Olympus C-410L", 	0, 0, 0, 0 },
	{"Olympus C-420", 	0, 0, 0, 0 },
	{"Olympus C-420L", 	0, 0, 0, 0 },
	{"Olympus C-800", 	0, 0, 0, 0 },
	{"Olympus C-800L", 	0, 0, 0, 0 },
	{"Olympus C-820", 	0, 0, 0, 0 },
	{"Olympus C-820L", 	0, 0, 0, 0 },
	{"Olympus C-830L", 	0, 0, 0, 0 },
	{"Olympus C-840L", 	0, 0, 0, 0 },
	{"Olympus C-900 Zoom", 	0, 0, 0, 0 },
	{"Olympus C-900L Zoom", 0, 0, 0, 0 },
	{"Olympus C-1000L", 	0, 0, 0, 0 },
	{"Olympus C-1400L", 	0, 0, 0, 0 },
	{"Olympus C-1400XL", 	0, 0, 0, 0 },
	{"Olympus C-2000Z", 	0, 0, 0, 0 },
	{"Olympus C-2020Z",	0, 0, 0, 0 },
        {"Olympus C-2040Z", 	0x07b4, 0x105, 0x83, 0x04},
        {"Olympus C-2500Z", 	0, 0, 0, 0 },
	{"Olympus C-3000Z", 	0x07b4, 0x100, 0x83, 0x04},
	{"Olympus C-3030Z", 	0x07b4, 0x100, 0x83, 0x04},
	{"Panasonic Coolshot KXl-600A", 0, 0, 0, 0 },
	{"Panasonic Coolshot NV-DCF5E", 0, 0, 0, 0 },
	{"Polaroid PDC 640", 	0, 0, 0, 0 },
	{"Sanyo DSC-X300", 	0, 0, 0, 0 },
	{"Sanyo DSC-X350", 	0, 0, 0, 0 },
	{"Sanyo VPC-G200", 	0, 0, 0, 0 },
	{"Sanyo VPC-G200EX", 	0, 0, 0, 0 },
	{"Sanyo VPC-G210", 	0, 0, 0, 0 },
	{"Sanyo VPC-G250", 	0, 0, 0, 0 },
	{"Sierra Imaging SD640",0, 0, 0, 0 },
	{"", 0, 0}
};

int camera_abilities (CameraAbilitiesList *list) 
{
	int x=0;
	CameraAbilities *a;

	while (strlen(sierra_cameras[x].model)>0) {
		a = gp_abilities_new();
		strcpy(a->model, sierra_cameras[x].model);
		a->port     = GP_PORT_SERIAL;
		if ((sierra_cameras[x].usb_vendor  > 0) &&
		    (sierra_cameras[x].usb_product > 0))
			a->port |= GP_PORT_USB;
		a->speed[0] = 9600;
		a->speed[1] = 19200;
		a->speed[2] = 38400;
		a->speed[3] = 57600;
		a->speed[4] = 115200;
		a->speed[5] = 0;
		a->operations        = 	GP_OPERATION_CAPTURE_IMAGE |
					GP_OPERATION_CAPTURE_PREVIEW;
		a->file_operations   = 	GP_FILE_OPERATION_DELETE | 
					GP_FILE_OPERATION_PREVIEW;
		a->folder_operations = 	GP_FOLDER_OPERATION_NONE;
		a->usb_vendor  = sierra_cameras[x].usb_vendor;
		a->usb_product = sierra_cameras[x].usb_product;
		gp_abilities_list_append(list, a);

		x++;
	}

	return (GP_OK);
}

int camera_init (Camera *camera) 
{
	int value=0, count;
	int x=0, ret;
	int vendor=0, product=0, inep=0, outep=0;
        gp_port_settings settings;
	SierraData *fd;

	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** camera_init");

	if (!camera)
		return (GP_ERROR_BAD_PARAMETERS);
	
	/* First, set up all the function pointers */
	camera->functions->id 			= camera_id;
	camera->functions->abilities 		= camera_abilities;
	camera->functions->init 		= camera_init;
	camera->functions->exit 		= camera_exit;
	camera->functions->folder_list_folders 	= camera_folder_list_folders;
	camera->functions->folder_list_files   	= camera_folder_list_files;
	camera->functions->file_get 		= camera_file_get;
	camera->functions->file_get_preview 	= camera_file_get_preview;
	camera->functions->file_delete 		= camera_file_delete;
	camera->functions->capture_preview	= camera_capture_preview;
	camera->functions->capture 		= camera_capture;
	camera->functions->summary		= camera_summary;
	camera->functions->manual 		= camera_manual;
	camera->functions->about 		= camera_about;

	fd = (SierraData*)malloc(sizeof(SierraData));	
	camera->camlib_data = fd;

	fd->first_packet = 1;

	switch (camera->port->type) {
	case GP_PORT_SERIAL:
		ret = gp_port_new (&(fd->dev), GP_PORT_SERIAL);
		if (ret != GP_OK) {
			free (fd);
			return (ret);
		}

		strcpy (settings.serial.port, camera->port->path);
		settings.serial.speed 	 = 19200;
		settings.serial.bits 	 = 8;
		settings.serial.parity 	 = 0;
		settings.serial.stopbits = 1;
		break;
	case GP_PORT_USB:

		/* Lookup the USB information */
		for (x = 0; strlen (sierra_cameras[x].model) > 0; x++) {
			if (strcmp(sierra_cameras[x].model, camera->model)==0) {
				vendor = sierra_cameras[x].usb_vendor;
				product = sierra_cameras[x].usb_product;
				inep = sierra_cameras[x].usb_inep;
				outep = sierra_cameras[x].usb_outep;
			}
		}

		/* Couldn't find the usb information */
		if ((vendor == 0) && (product == 0))
			return (GP_ERROR);

		ret = gp_port_new (&(fd->dev), GP_PORT_USB);
		if (ret != GP_OK) {
			free (fd);
			return (ret);
		}

	        ret = gp_port_usb_find_device (fd->dev, vendor, product);
		if (ret != GP_OK) {
			gp_port_free (fd->dev);
			free (fd);
	                return (ret);
		}

	        gp_port_timeout_set (fd->dev, 5000);
       		settings.usb.inep 	= inep;
       		settings.usb.outep 	= outep;
       		settings.usb.config 	= 1;
       		settings.usb.interface 	= 0;
       		settings.usb.altsetting = 0;
		break;
	default:
		free (fd);
                return (GP_ERROR_IO_UNKNOWN_PORT);
	}

	ret = gp_port_settings_set (fd->dev, settings);
	if (ret != GP_OK) {
		gp_port_free (fd->dev);
		free (fd);
                return (ret);
	}

	gp_port_timeout_set (fd->dev, TIMEOUT);
	fd->type = camera->port->type;

	ret = gp_port_open (fd->dev);
	if (ret != GP_OK) {
		gp_port_free (fd->dev);
		free (fd);
		return (ret);
	}

	switch (camera->port->type) {
	case GP_PORT_SERIAL:

		ret = sierra_ping (camera);
		if (ret != GP_OK) {
			gp_port_free (fd->dev);
			free (fd);
			return (ret);
		}

		ret = sierra_set_speed (camera, camera->port->speed);
		if (ret != GP_OK) {
			gp_port_free (fd->dev);
			free (fd);
			return (ret);
		}

		fd->speed = camera->port->speed;
		break;
	case GP_PORT_USB:
		gp_port_usb_clear_halt (fd->dev, GP_PORT_USB_ENDPOINT_IN);
		break;
	default:
		break;
	}

	ret = sierra_get_int_register (camera, 1, &value);
	if (ret != GP_OK) {
		gp_port_free(fd->dev);
		free (fd);
		return (ret);
	}

	ret = sierra_set_int_register (camera, 83, -1);
	if (ret != GP_OK) {
		gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** Could not set "
				 "register 83 to -1: %s", 
				 gp_camera_get_result_as_string (camera, ret));
	}

	ret = gp_port_timeout_set (fd->dev, 50);
	if (ret != GP_OK)
		return (ret);

	/* Folder support? */
	ret = sierra_set_string_register (camera, 84, "\\", 1);
	if (ret != GP_OK) {
		fd->folders = 0;
		gp_debug_printf (GP_DEBUG_LOW, "sierra", 
				 "*** folder support: no");
	} else {
		fd->folders = 1;
		gp_debug_printf (GP_DEBUG_LOW, "sierra",
				 "*** folder support: yes");
	}

	fd->fs = gp_filesystem_new ();
	count = sierra_file_count (camera);
	if (count < 0) 
		return (count);

        /* Populate the filesystem */
	ret = gp_filesystem_populate (fd->fs, "/", "PIC%04i.jpg", count);
	if (ret != GP_OK)
		return (ret);

	strcpy (fd->folder, "/");

	ret = gp_port_timeout_set (fd->dev, TIMEOUT);
	if (ret != GP_OK)
		return (ret);

	camera_stop (camera);
	return (GP_OK);
}

static int sierra_change_folder (Camera *camera, const char *folder)
{	
	SierraData *fd = (SierraData*)camera->camlib_data;
	int st = 0,i = 1, ret;
	char target[128];

	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** sierra_change_folder");
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** folder: %s", folder);

	if (!fd->folders)
		return GP_OK;
	if (!folder || !camera)
		return GP_ERROR_BAD_PARAMETERS;

	if (folder[0]) {
		strncpy(target, folder, sizeof(target)-1);
		target[sizeof(target)-1]='\0';
		if (target[strlen(target)-1] != '/') strcat(target, "/");
	} else
		strcpy(target, "/");

	if (target[0] != '/')
		i = 0;
	else {
		ret = sierra_set_string_register (camera, 84, "\\", 1);
		if (ret != GP_OK)
			return (ret);
	}
	st = i;
	for (; target[i]; i++) {
		if (target[i] == '/') {
			target[i] = '\0';
			if (st == i - 1)
				break;
			ret = sierra_set_string_register (camera, 84, 
				target + st, strlen (target + st));
			if (ret != GP_OK)
				return (ret);
			st = i + 1;
			target[i] = '/';
		}
	}
	strcpy (fd->folder, folder);

	return GP_OK;
}

int camera_start (Camera *camera)
{
	SierraData *fd = (SierraData*)camera->camlib_data;
	int ret;

	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** camera_start");

	if (fd->type != GP_PORT_SERIAL)
		return (GP_OK);
	
	ret = sierra_set_speed (camera, fd->speed);
	if (ret != GP_OK)
		return (ret);

	return (sierra_folder_set (camera, fd->folder));
}

int camera_stop (Camera *camera) 
{
	SierraData *fd = (SierraData*)camera->camlib_data;

	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** camera_stop");

	if (fd->type != GP_PORT_SERIAL)
		return (GP_OK);

	return (sierra_set_speed (camera, -1));
}

int camera_exit (Camera *camera) 
{
//	SierraData *fd = (SierraData*)camera->camlib_data;

	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** camera_exit");

//	gp_port_close (fd->dev);
//	gp_port_free (fd->dev);
//	free (fd);

	return (GP_OK);
}

int camera_folder_list_files (Camera *camera, const char *folder, 
			      CameraList *list) 
{
	SierraData *fd = (SierraData*)camera->camlib_data;
	int x=0, count, ret;

	gp_debug_printf (GP_DEBUG_LOW, "sierra", 
			 "*** camera_folder_list_files");
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** folder: %s", folder);

	if ((!fd->folders) && (strcmp ("/", folder) != 0))
		return (GP_ERROR);

	ret = camera_start (camera);
	if (ret != GP_OK)
		return (ret);

	if (fd->folders) {
		ret = sierra_change_folder (camera, folder);
		if (ret != GP_OK)
			return (ret);
	}

	count = sierra_file_count (camera);
	if (count < 0)
		return (count);

	for (x = 0; x < count; x++) {
		char buf[128];
		int length;
		
		/* are all filenames *.jpg, or are there *.tif files too? */
		/* Set the current picture number */
		ret = sierra_set_int_register (camera, 4, x + 1);
		if (ret != GP_OK) {
			camera_stop (camera);
			return (ret);
		}

		/* Get the picture filename */
		ret = sierra_get_string_register (camera, 79, 0, NULL, buf, 
						  &length);
		if (ret != GP_OK) {
			camera_stop (camera);
			return (ret);
		}

		if (length > 0) {

			/* Filename supported. Use the camera filename */
			gp_list_append (list, buf, GP_LIST_FILE);
			gp_filesystem_append (fd->fs, folder, buf);

		} else {

			/* Filename not supported. Use CameraFileSystem entry */
			gp_list_append (list, gp_filesystem_name (fd->fs, 
					folder, x), GP_LIST_FILE);
		}
	}

	camera_stop (camera);

	return (GP_OK);
}

int camera_folder_list_folders (Camera *camera, const char *folder, 
				CameraList *list) 
{
	SierraData *fd = (SierraData*)camera->camlib_data;
	int count, i, bsize, ret;
	char buf[1024];

	gp_debug_printf (GP_DEBUG_LOW, "sierra", 
			 "*** camera_folder_list_folders");
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** folder: %s", folder);

	if ((!fd->folders) && (strcmp (folder, "/") !=0 ))
		return GP_ERROR;

	if (!fd->folders)
		return GP_OK;

	ret = camera_start (camera);
	if (ret != GP_OK)
		return (ret);

	ret = sierra_change_folder (camera, folder);
	if (ret != GP_OK) {
		camera_stop (camera);
		return (ret);
	}

	ret = sierra_get_int_register (camera, 83, &count);
	if (ret != GP_OK) {
		camera_stop (camera);
		return (ret);
	}

	for (i = 0; i < count; i++) {

		ret = sierra_change_folder (camera, folder);
		if (ret != GP_OK) 
			break;

		ret = sierra_set_int_register (camera, 83, i+1);
		if (ret != GP_OK)
			break;

		bsize = 1024;
		ret = sierra_get_string_register (camera, 84, 0, NULL, buf, 
						  &bsize);
		if (ret != GP_OK)
			break;

		/* remove trailing spaces */
		for (i = strlen (buf)-1; i >= 0 && buf[i] == ' '; i--)
			buf[i]='\0';

		/* append the folder name on to the folder list */
		gp_list_append (list, buf, GP_LIST_FOLDER);
	}

	camera_stop (camera);
	return (GP_OK);
}

int sierra_folder_set (Camera *camera, const char *folder) 
{
	char tf[4096];
	SierraData *fd = (SierraData*)camera->camlib_data;

	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** sierra_folder_set");
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** folder: %s");

	if (!fd->folders) {
		if (strcmp ("/", folder) != 0)
			return (GP_ERROR_DIRECTORY_NOT_FOUND);
		else
			return (GP_OK);
	}

	if (folder[0] != '/')
		strcat (tf, folder);
	else
		strcpy (tf, folder);

	if (tf[strlen (fd->folder) - 1] != '/')
	       strcat (tf, "/");

	/* TODO: check whether the selected folder is valid.
		 It should be done after implementing camera_lock/unlock pair */
	
	return (sierra_change_folder (camera, tf));
}

int camera_file_get_generic (Camera *camera, CameraFile *file, 
			     const char *folder, const char *filename, 
			     int thumbnail) 
{
	int regl, regd, file_number, ret;
	SierraData *fd = (SierraData*)camera->camlib_data;

	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** sierra_file_get_generic");
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** folder: %s", folder);
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** filename: %s", filename);

	ret = camera_start(camera);
	if (ret != GP_OK)
		return (ret);

	/* Get the file number from the CameraFileSystem */
	file_number = gp_filesystem_number(fd->fs, folder, filename);
	if (file_number < 0)
		return (file_number);

	if (thumbnail) {
		regl = 13;
		regd = 15;
	}  else {
		regl = 12;
		regd = 14;
	}

	/* Fill in the file structure */	
	strcpy (file->type, "image/jpeg");
	strcpy (file->name, filename);

	/* Get the picture data */
	ret = sierra_get_string_register (camera, regd, file_number + 1, file, 
					  NULL, NULL);
	if (ret != GP_OK)
		return (ret);

	return (camera_stop (camera));
}

int camera_file_get (Camera *camera, const char *folder, const char *filename,
                     CameraFile *file) 
{
	return (camera_file_get_generic (camera, file, folder, filename, 0));
}

int camera_file_get_preview (Camera *camera, const char *folder, 
			     const char *filename,
                             CameraFile *file) 
{
	return (camera_file_get_generic (camera, file, folder, filename, 1));
}

int camera_file_delete (Camera *camera, const char *folder, 
			const char *filename) 
{
	int ret, file_number;
	SierraData *fd = (SierraData*)camera->camlib_data;

	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** sierra_file_delete");
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** folder: %s", folder);
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** filename: %s", filename);

	ret = camera_start (camera);
	if (ret != GP_OK)
		return (ret);

	file_number = gp_filesystem_number (fd->fs, folder, filename);

	ret = sierra_delete (camera, file_number+1);
	if (ret != GP_OK)
		return (ret);

	ret = gp_filesystem_delete (fd->fs, folder, filename);
	if (ret != GP_OK)
		return (ret);

	return (camera_stop(camera));
}

int camera_capture (Camera *camera, int capture_type, CameraFilePath *path) 
{
	int ret;

	ret = camera_start (camera);
	if (ret != GP_OK)
		return (ret);

	ret = sierra_capture (camera, capture_type, path);
	if (ret != GP_OK) {
		camera_stop (camera);
		return (ret);
	}
	
	return (camera_stop (camera));
}

int camera_capture_preview (Camera *camera, CameraFile *file)
{
	int ret;

	ret = camera_start (camera);
	if (ret != GP_OK)
		return (ret);
	
	ret = sierra_capture_preview (camera, file);
	if (ret != GP_OK) {
		camera_stop (camera);
		return (ret);
	}

	return (camera_stop (camera));
}

int camera_summary (Camera *camera, CameraText *summary) 
{
	char buf[1024*32];
	int value, ret;
	char t[1024];

	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** camera_summary");

	ret = camera_start (camera);
	if (ret != GP_OK)
		return (ret);

	strcpy(buf, "");

	/* Get all the string-related info */
	ret = sierra_get_string_register (camera, 22, 0, NULL, t, &value);
	if (ret == GP_OK)
		sprintf(buf, "%sCamera ID       : %s\n", buf, t);

	ret = sierra_get_string_register (camera, 25, 0, NULL, t, &value);
	if (ret == GP_OK)
		sprintf(buf, "%sSerial Number   : %s\n", buf, t);

	ret = sierra_get_int_register (camera, 1, &value);
	if (ret == GP_OK) {
		switch (value) {
		case 1:	strcpy(t, "Standard");
			break;
		case 2:	strcpy(t, "High");
			break;
		case 3:	strcpy(t, "Best");
			break;
		default:
			sprintf(t, "%i (unknown)", value);
		}
		sprintf (buf, "%sResolution      : %s\n", buf, t);
	}
	
	ret = sierra_get_int_register (camera, 3, &value);
	if (ret == GP_OK) {
		if (value == 0)
			strcpy (t, "Auto");
		else
			sprintf (t, "%i microseconds", value);
		sprintf (buf, "%sShutter Speed   : %s\n", buf, t);
	}

	ret = sierra_get_int_register (camera, 5, &value);
	if (ret == GP_OK) {
		switch (value) {
		case 1:	strcpy (t, "Low");
			break;
		case 2:	strcpy (t, "Medium");
			break;
		case 3:	strcpy (t, "High");
			break;
		default:
			sprintf(t, "%i (unknown)", value);
		}
		sprintf (buf, "%sAperture        : %s\n", buf, t);
	}

	ret = sierra_get_int_register (camera, 6, &value);
	if (ret == GP_OK) {
		switch (value) {
		case 1:	strcpy (t, "Color");
			break;
		case 2:	strcpy (t, "Black/White");
			break;
		default:
			sprintf (t, "%i (unknown)", value);
		}		
		sprintf (buf, "%sColor Mode      : %s\n", buf, t);
	}

	ret = sierra_get_int_register (camera, 7, &value);
	if (ret == GP_OK) {
		switch (value) {
		case 0: strcpy (t, "Auto");
			break;
		case 1:	strcpy (t, "Force");
			break;
		case 2:	strcpy (t, "Off");
			break;
		case 3:	strcpy (t, "Red-eye Reduction");
			break;
		case 4:	strcpy (t, "Slow Synch");
			break;
		default:
			sprintf (t, "%i (unknown)", value);
		}
		sprintf (buf, "%sFlash Mode      : %s\n", buf, t);
	}

	ret = sierra_get_int_register (camera, 19, &value);
	if (ret == GP_OK) {
		switch (value) {
		case 0: strcpy (t, "Normal");
			break;
		case 1:	strcpy (t, "Bright+");
			break;
		case 2:	strcpy (t, "Bright-");
			break;
		case 3:	strcpy (t, "Contrast+");
			break;
		case 4:	strcpy (t, "Contrast-");
			break;
		default:
			sprintf (t, "%i (unknown)", value);
		}
		sprintf (buf, "%sBright/Contrast : %s\n", buf, t);
	}
	
	ret = sierra_get_int_register (camera, 20, &value);
	if (ret == GP_OK) {
		switch (value) {
		case 0: strcpy (t, "Auto");
			break;
		case 1:	strcpy (t, "Skylight");
			break;
		case 2:	strcpy (t, "Flourescent");
			break;
		case 3:	strcpy (t, "Tungsten");
			break;
		case 255:	
			strcpy (t, "Cloudy");
			break;
		default:
			sprintf (t, "%i (unknown)", value);
		}
		sprintf (buf, "%sWhite Balance   : %s\n", buf, t);
	}

	ret = sierra_get_int_register (camera, 33, &value);
	if (ret == GP_OK) {
		switch(value) {
		case 1: strcpy (t, "Macro");
			break;
		case 2:	strcpy (t, "Normal");
			break;
		case 3:	strcpy (t, "Infinity/Fish-eye");
			break;
		default:
			sprintf (t, "%i (unknown)", value);
		}
		sprintf (buf, "%sLens Mode       : %s\n", buf, t);
	}

	/* Get all the integer information */
	if (sierra_get_int_register(camera, 10, &value) == GP_OK)
		sprintf (buf, "%sFrames Taken    : %i\n", buf, value);
	if (sierra_get_int_register(camera, 11, &value) == GP_OK)
		sprintf (buf, "%sFrames Left     : %i\n", buf, value);
	if (sierra_get_int_register(camera, 16, &value) == GP_OK)
		sprintf (buf, "%sBattery Life    : %i\n", buf, value);
	if (sierra_get_int_register(camera, 23, &value) == GP_OK)
		sprintf (buf, "%sAutoOff (host)  : %i seconds\n", buf, value);
	if (sierra_get_int_register(camera, 24, &value) == GP_OK)
		sprintf (buf, "%sAutoOff (field) : %i seconds\n", buf, value);
	if (sierra_get_int_register(camera, 28, &value) == GP_OK)
		sprintf (buf, "%sMemory Left	: %i bytes\n", buf, value);
	if (sierra_get_int_register(camera, 35, &value) == GP_OK)
		sprintf (buf, "%sLCD Brightness  : %i (1-7)\n", buf, value);
	if (sierra_get_int_register(camera, 38, &value) == GP_OK)
		sprintf (buf, "%sLCD AutoOff	: %i seconds\n", buf, value);

	strcpy(summary->text, buf);

	return (camera_stop(camera));
}

int camera_manual (Camera *camera, CameraText *manual) 
{
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** camera_manual");

	strcpy (manual->text, "Manual Not Available");

	return (GP_OK);
}

int camera_about (Camera *camera, CameraText *about) 
{
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** camera_about");
	
	strcpy (about->text, 
		"sierra SPARClite library\n"
		"Scott Fritzinger <scottf@unr.edu>\n"
		"Support for sierra-based digital cameras\n"
		"including Olympus, Nikon, Epson, and others.\n"
		"\n"
		"Thanks to Data Engines (www.dataengines.com)\n"
		"for the use of their Olympus C-3030Z for USB\n"
		"support implementation.");

	return (GP_OK);
}
