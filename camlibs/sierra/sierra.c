#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gphoto2.h>
#include <gpio.h>

#include "library.h"
#include "sierra.h"

#define TIMEOUT	   2000

int camera_start(Camera *camera);
int camera_stop(Camera *camera);

void sierra_debug_print(SierraData *fd, char *message) {

	gp_debug_printf(GP_DEBUG_LOW, "sierra", message);
}

int camera_id (CameraText *id) {

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

int camera_abilities (CameraAbilitiesList *list) {

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
		a->capture  = GP_CAPTURE_IMAGE | GP_CAPTURE_IMAGE;
		a->config   = 0;
		a->file_delete  = 1;
		a->file_preview = 1;
		a->file_put = 0;
		a->usb_vendor  = sierra_cameras[x].usb_vendor;
		a->usb_product = sierra_cameras[x].usb_product;
		gp_abilities_list_append(list, a);

		x++;
	}

	return (GP_OK);
}

int camera_init (Camera *camera) {

	int value=0, count;
#ifdef GPIO_USB
	int x=0;
	int vendor=0, product=0, inep=0, outep=0;
#endif
	gpio_device_settings settings;
	SierraData *fd;

	if (!camera)
		return (GP_ERROR);

	/* First, set up all the function pointers */
	camera->functions->id 		= camera_id;
	camera->functions->abilities 	= camera_abilities;
	camera->functions->init 	= camera_init;
	camera->functions->exit 	= camera_exit;
	camera->functions->folder_list  = camera_folder_list;
	camera->functions->file_list    = camera_file_list;
	camera->functions->file_get 	= camera_file_get;
	camera->functions->file_get_preview =  camera_file_get_preview;
	camera->functions->file_delete 	= camera_file_delete;
	camera->functions->capture 	= camera_capture;
	camera->functions->summary	= camera_summary;
	camera->functions->manual 	= camera_manual;
	camera->functions->about 	= camera_about;

	fd = (SierraData*)malloc(sizeof(SierraData));	
	camera->camlib_data = fd;

	fd->first_packet = 1;

	sierra_debug_print(fd, "Initializing camera");

	switch (camera->port->type) {
		case GP_PORT_SERIAL:
			sierra_debug_print(fd, "Serial Device");
			fd->dev = gpio_new(GPIO_DEVICE_SERIAL);
			if (!fd->dev) {
				gpio_free(fd->dev);
				free(fd);
				return (GP_ERROR);
			}
			strcpy(settings.serial.port, camera->port->path);
			settings.serial.speed 	 = 19200;
			settings.serial.bits 	 = 8;
			settings.serial.parity 	 = 0;
			settings.serial.stopbits = 1;
			break;
#ifdef GPIO_USB
		case GP_PORT_USB:
			/* lookup the USB information */
			while (strlen(sierra_cameras[x].model)>0) {			
				if (strcmp(sierra_cameras[x].model, camera->model)==0) {
					vendor = sierra_cameras[x].usb_vendor;
					product = sierra_cameras[x].usb_product;
					inep = sierra_cameras[x].usb_inep;
					outep = sierra_cameras[x].usb_outep;
				}
				x++;
			}

			/* Couldn't find the usb information */
			if ((vendor == 0) && (product == 0))
				return (GP_ERROR);

			sierra_debug_print(fd, "USB Device");
			fd->dev = gpio_new(GPIO_DEVICE_USB);

			if (!fd->dev) {
				gpio_free(fd->dev);
				free(fd);
				return (GP_ERROR);
			}

		        if (gpio_usb_find_device(fd->dev, vendor, product) == GPIO_ERROR) {
				gpio_free(fd->dev);
				free (fd);
		                return (GP_ERROR);
			}
		        gpio_set_timeout (fd->dev, 5000);
        		settings.usb.inep 	= inep;
        		settings.usb.outep 	= outep;
        		settings.usb.config 	= 1;
        		settings.usb.interface 	= 0;
        		settings.usb.altsetting = 0;
			break;
#endif
		default:
			sierra_debug_print(fd, "Invalid Device");
			free (fd);
	                return (GP_ERROR);
	}

	if (gpio_set_settings(fd->dev, settings) == GPIO_ERROR) {
		gpio_free(fd->dev);
		free (fd);
                return (GP_ERROR);
	}

	gpio_set_timeout(fd->dev, TIMEOUT);
	fd->type = camera->port->type;

	if (gpio_open(fd->dev)==GPIO_ERROR) {
		gpio_free(fd->dev);
		free (fd);
		return (GP_ERROR);
	}

	switch (camera->port->type) {
		case GP_PORT_SERIAL:
			if (sierra_ping(camera)==GP_ERROR) {
				gpio_free(fd->dev);
				free (fd);
				return (GP_ERROR);
			}

			if (sierra_set_speed(camera, camera->port->speed)==GP_ERROR) {
				gpio_free(fd->dev);
				free (fd);
				return (GP_ERROR);
			}
			fd->speed = camera->port->speed;
			break;
#ifdef GPIO_USB
		case GP_PORT_USB:
			gpio_usb_clear_halt(fd->dev, GPIO_USB_IN_ENDPOINT);
			break;
#endif
		default:
			break;
	}

	if (sierra_get_int_register(camera, 1, &value)==GP_ERROR) {
		gpio_free(fd->dev);
		free (fd);
		return (GP_ERROR);
	}

	sierra_set_int_register(camera, 83, -1);

	gpio_set_timeout(fd->dev, 50);
	if (sierra_set_string_register(camera, 84, "\\", 1)==GP_ERROR)
		fd->folders = 0;
	   else
		fd->folders = 1;	

	if (fd->folders)
		sierra_debug_print(fd, "Camera supports folders");
	   else
		sierra_debug_print(fd, "Camera doesn't support folders. Using CameraFilesystem emu.");

	fd->fs = gp_filesystem_new();
	count = sierra_file_count(camera);
        /* Populate the filesystem */
	gp_filesystem_populate(fd->fs, "/", "PIC%04i.jpg", count);

	strcpy(fd->folder, "/");

	gpio_set_timeout(fd->dev, TIMEOUT);

	camera_stop(camera);
	return (GP_OK);
}

static int sierra_change_folder(Camera *camera, const char *folder)
{	
	SierraData *fd = (SierraData*)camera->camlib_data;

	int st=0,i = 1;
	char target[128];

	if (!fd->folders)
		return GP_OK;
	if (!folder)
		return GP_ERROR;

	if (folder[0]) {
		strncpy(target, folder, sizeof(target)-1);
		target[sizeof(target)-1]='\0';
		if (target[strlen(target)-1] != '/') strcat(target, "/");
	} else
		strcpy(target, "/");

	if (target[0] != '/') {
		sierra_debug_print(fd, "Change dir called with relative path?");
		i = 0;
	} else {
		if (sierra_set_string_register(camera, 84, "\\", 1)==GP_ERROR)
			return GP_ERROR;
	}
	st = i;
	for (; target[i]; i++) {
		if (target[i] == '/') {
			target[i] = '\0';
			if (st == i-1)
				break;
			if (sierra_set_string_register(camera, 84, target+st, strlen(target+st))==GP_ERROR)
				return GP_ERROR; 
			st = i+1;
			target[i] = '/';
		}
	}
	strcpy(fd->folder, folder);

	return GP_OK;
}

int camera_start(Camera *camera) {

	SierraData *fd = (SierraData*)camera->camlib_data;
	
	if (fd->type == GP_PORT_SERIAL) {
		if (sierra_set_speed(camera, fd->speed)==GP_ERROR)
			return (GP_ERROR);
		sierra_folder_set(camera, fd->folder);
	}
	return (GP_OK);
}

int camera_stop(Camera *camera) {

	SierraData *fd = (SierraData*)camera->camlib_data;

	if (fd->type == GP_PORT_SERIAL) {
		if (sierra_set_speed(camera, -1)==GP_ERROR)
			return (GP_ERROR);
	}

	return (GP_OK);
}

int camera_exit (Camera *camera) {

	SierraData *fd = (SierraData*)camera->camlib_data;

	sierra_debug_print(fd, "Exiting camera");

	gpio_free(fd->dev);
	free(fd);

	return (GP_OK);
}

int camera_file_list(Camera *camera, CameraList *list, char *folder) {

	SierraData *fd = (SierraData*)camera->camlib_data;
	int x=0, error=0, count;

	if ((!fd->folders)&&(strcmp("/", folder)!=0))
		return (GP_ERROR);

	if (camera_start(camera)==GP_ERROR)
		return (GP_ERROR);

	if (fd->folders) {
		if (sierra_change_folder(camera, folder))
			return (GP_ERROR);
	}

	count = sierra_file_count(camera);

	while ((x<count)&&(!error)) {
#if 0
	/* are all filenames *.jpg, or are there *.tif files too? */
		/* Set the current picture number */
		if (sierra_set_int_register(camera, 4, x+1)==GP_ERROR) {
			gp_message("Could not set the picture number");
			camera_stop(camera);
			return (GP_ERROR);
		}
		/* Get the picture filename */
		if (sierra_get_string_register(camera, 79, 0, NULL, buf, &length)==GP_ERROR)
			gp_message("Could not get filename");
			camera_stop(camera);
			return (GP_ERROR);
		}
		if (length > 0)
			/* Filename supported. Use the camera filename */
			gp_list_append(list, buf, GP_LIST_FILE);
		   else
#endif
			/* Filename not supported. Use CameraFileSystem entry */
			gp_list_append(list, gp_filesystem_name(fd->fs, folder, x), 
				GP_LIST_FILE);
		x++;
	}

	camera_stop(camera);
	return GP_OK;
}

int camera_folder_list(Camera *camera, CameraList *list, char *folder) {

	SierraData *fd = (SierraData*)camera->camlib_data;
	int count, i, bsize;
	char buf[1024];

	sierra_debug_print(fd, "Listing folders");

	if ((!fd->folders)&&(strcmp(folder, "/")!=0))
		return GP_ERROR;

	if (!fd->folders)
		return GP_OK;

	if (camera_start(camera) != GP_OK) /* XXX */
		return GP_ERROR;

	if ((sierra_change_folder(camera, folder) != GP_OK) ||
	    (sierra_get_int_register(camera, 83, &count) != GP_OK)) {
		camera_stop(camera);
		return GP_ERROR;
	}
	if (count)
	for (i = 0; i < count; i++) {
		if (sierra_change_folder(camera, folder) != GP_OK) {
			break;
		}
		if (sierra_set_int_register(camera, 83, i+1) != GP_OK) {
			break;
		}
		bsize = 1024;
		if (sierra_get_string_register(camera, 84, 0, NULL, buf, &bsize) != GP_OK) {
			break;
		} else {
			/* append the folder name on to the folder list */
			gp_list_append(list, buf, GP_LIST_FOLDER);
		}

	}
	camera_stop(camera);
	return (GP_OK);
}

int sierra_folder_set(Camera *camera, char *folder) {

	char buf[4096];
	char tf[4096];
	SierraData *fd = (SierraData*)camera->camlib_data;

	/* If the camera doesn't support folders and it was passed something
	   other than "/", return an error! */
	if ((!fd->folders)&&(strcmp("/", folder)!=0))
		return (GP_ERROR);

	/* If the camera doesn't support folders and it was passed "/",
	   then that's OK */
	if ((!fd->folders)&&(strcmp("/", folder)==0))
		return (GP_OK);

	if (folder[0] != '/') {
		strcat(tf, folder);
	} else {
		strcpy(tf, folder);
	}

	if (tf[strlen(fd->folder)-1] != '/')
	       strcat(tf, "/");

	printf(buf, "Folder set to %s", tf);

	/* TODO: check whether the selected folder is valid.
		 It should be done after implementing camera_lock/unlock pair */
	
	if (fd->folders) {
		if (sierra_change_folder(camera, tf))
			return (GP_ERROR);
	}

	return (GP_OK);
}

int camera_file_get_generic (Camera *camera, CameraFile *file, 
	char *folder, char *filename, int thumbnail) {

	char buf[4096];
	int regl, regd, file_number;
	SierraData *fd = (SierraData*)camera->camlib_data;

if (camera_start(camera)==GP_ERROR)
return (GP_ERROR);

	/* Get the file number from the CameraFileSystem */
	file_number = gp_filesystem_number(fd->fs, folder, filename);

	if (file_number == GP_ERROR)
		return GP_ERROR;

	if (thumbnail) {
		sprintf(buf, "Getting thumbnail \"%s\" (#%i)", filename, file_number+1);
		regl = 13;
		regd = 15;
	}  else {
		sprintf(buf, "Getting photo \"%s\" (#%i)", filename, file_number+1);
		regl = 12;
		regd = 14;
	}

	sierra_debug_print(fd, buf);

	/* Fill in the file structure */	
	strcpy(file->type, "image/jpg");
	strcpy(file->name, filename);

	/* Get the picture data */
	if (sierra_get_string_register(camera, regd, file_number+1, file, NULL, NULL)==GP_ERROR)
		return (GP_ERROR);

if (camera_stop(camera)==GP_ERROR)
return (GP_ERROR);

	return (GP_OK);
}

int camera_file_get (Camera *camera, CameraFile *file, char *folder, char *filename) {

	return (camera_file_get_generic(camera, file, folder, filename, 0));
}

int camera_file_get_preview (Camera *camera, CameraFile *file, char *folder, char *filename) {

	return (camera_file_get_generic(camera, file, folder, filename, 1));
}

int camera_file_delete (Camera *camera, char *folder, char *filename) {

	int ret, file_number;
	char buf[4096];
	SierraData *fd = (SierraData*)camera->camlib_data;

if (camera_start(camera)==GP_ERROR)
return (GP_ERROR);

	file_number = gp_filesystem_number(fd->fs, folder, filename);

	sprintf(buf, "Deleting photo #%i", file_number+1);
	sierra_debug_print(fd, buf);

	ret = sierra_delete(camera, file_number+1);
	if (ret == GPIO_OK)
		gp_filesystem_delete(fd->fs, folder, filename);

if (camera_stop(camera)==GP_ERROR)
return (GP_ERROR);

	return (ret);
}

int camera_capture (Camera *camera, CameraFile *file, CameraCaptureInfo *info) {

	SierraData *fd = (SierraData*)camera->camlib_data;
	int retval;

if (camera_start(camera)==GP_ERROR)
return (GP_ERROR);

	sierra_debug_print(fd, "Capturing image");
	retval = sierra_capture(camera, file, info);

if (camera_stop(camera)==GP_ERROR)
return (GP_ERROR);

	return (retval);
}

int camera_summary (Camera *camera, CameraText *summary) {

	char buf[1024*32];
	int value;
	char t[1024];
	SierraData *fd = (SierraData*)camera->camlib_data;

if (camera_start(camera)==GP_ERROR)
return (GP_ERROR);

	sierra_debug_print(fd, "Getting camera summary");
	strcpy(buf, "");

	/* Get all the string-related info */
	if (sierra_get_string_register(camera, 22, 0, NULL, t, &value)!=GP_ERROR)
		sprintf(buf, "%sCamera ID       : %s\n", buf, t);
	if (sierra_get_string_register(camera, 25, 0, NULL, t, &value)!=GP_ERROR)
		sprintf(buf, "%sSerial Number   : %s\n", buf, t);
	if (sierra_get_int_register(camera, 1, &value)!=GP_ERROR) {
		switch(value) {
			case 1:	strcpy(t, "Standard");
				break;
			case 2:	strcpy(t, "High");
				break;
			case 3:	strcpy(t, "Best");
				break;
			default:
				sprintf(t, "%i (unknown)", value);
		}
		sprintf(buf, "%sResolution      : %s\n", buf, t);
	}
	if (sierra_get_int_register(camera, 3, &value)!=GP_ERROR) {
		if (value == 0)
			strcpy(t, "Auto");
		   else
			sprintf(t, "%i microseconds", value);
		sprintf(buf, "%sShutter Speed   : %s\n", buf, t);
	}
	if (sierra_get_int_register(camera, 5, &value)!=GP_ERROR) {
		switch(value) {
			case 1:	strcpy(t, "Low");
				break;
			case 2:	strcpy(t, "Medium");
				break;
			case 3:	strcpy(t, "High");
				break;
			default:
				sprintf(t, "%i (unknown)", value);
		}
		sprintf(buf, "%sAperture        : %s\n", buf, t);
	}
	if (sierra_get_int_register(camera, 6, &value)!=GP_ERROR) {	
		switch(value) {
			case 1:	strcpy(t, "Color");
				break;
			case 2:	strcpy(t, "Black/White");
				break;
			default:
				sprintf(t, "%i (unknown)", value);
		}		
		sprintf(buf, "%sColor Mode      : %s\n", buf, t);
	}

	if (sierra_get_int_register(camera, 7, &value)!=GP_ERROR) {	
		switch(value) {
			case 0: strcpy(t, "Auto");
				break;
			case 1:	strcpy(t, "Force");
				break;
			case 2:	strcpy(t, "Off");
				break;
			case 3:	strcpy(t, "Red-eye Reduction");
				break;
			case 4:	strcpy(t, "Slow Synch");
				break;
			default:
				sprintf(t, "%i (unknown)", value);
		}
		sprintf(buf, "%sFlash Mode      : %s\n", buf, t);
	}
	if (sierra_get_int_register(camera, 19, &value)!=GP_ERROR) {	
		switch(value) {
			case 0: strcpy(t, "Normal");
				break;
			case 1:	strcpy(t, "Bright+");
				break;
			case 2:	strcpy(t, "Bright-");
				break;
			case 3:	strcpy(t, "Contrast+");
				break;
			case 4:	strcpy(t, "Contrast-");
				break;
			default:
				sprintf(t, "%i (unknown)", value);
		}
		sprintf(buf, "%sBright/Contrast : %s\n", buf, t);
	}
	if (sierra_get_int_register(camera, 20, &value)!=GP_ERROR) {	
		switch(value) {
			case 0: strcpy(t, "Auto");
				break;
			case 1:	strcpy(t, "Skylight");
				break;
			case 2:	strcpy(t, "Flourescent");
				break;
			case 3:	strcpy(t, "Tungsten");
				break;
			case 255:	strcpy(t, "Cloudy");
				break;
			default:
				sprintf(t, "%i (unknown)", value);
		}
		sprintf(buf, "%sWhite Balance   : %s\n", buf, t);
	}
	if (sierra_get_int_register(camera, 33, &value)!=GP_ERROR) {	
		switch(value) {
			case 1: strcpy(t, "Macro");
				break;
			case 2:	strcpy(t, "Normal");
				break;
			case 3:	strcpy(t, "Infinity/Fish-eye");
				break;
			default:
				sprintf(t, "%i (unknown)", value);
		}
		sprintf(buf, "%sLens Mode       : %s\n", buf, t);
	}

	/* Get all the integer information */
	if (sierra_get_int_register(camera, 10, &value)!=GP_ERROR)
		sprintf(buf, "%sFrames Taken    : %i\n", buf, value);
	if (sierra_get_int_register(camera, 11, &value)!=GP_ERROR)
		sprintf(buf, "%sFrames Left     : %i\n", buf, value);
	if (sierra_get_int_register(camera, 16, &value)!=GP_ERROR)
		sprintf(buf, "%sBattery Life    : %i\n", buf, value);
	if (sierra_get_int_register(camera, 23, &value)!=GP_ERROR)
		sprintf(buf, "%sAutoOff (host)  : %i seconds\n", buf, value);
	if (sierra_get_int_register(camera, 24, &value)!=GP_ERROR)
		sprintf(buf, "%sAutoOff (field) : %i seconds\n", buf, value);
	if (sierra_get_int_register(camera, 28, &value)!=GP_ERROR)
		sprintf(buf, "%sMemory Left	: %i bytes\n", buf, value);
	if (sierra_get_int_register(camera, 35, &value)!=GP_ERROR)
		sprintf(buf, "%sLCD Brightness  : %i (1-7)\n", buf, value);
	if (sierra_get_int_register(camera, 38, &value)!=GP_ERROR)
		sprintf(buf, "%sLCD AutoOff	: %i seconds\n", buf, value);

	strcpy(summary->text, buf);

if (camera_stop(camera)==GP_ERROR)
return (GP_ERROR);

	return (GP_OK);
}

int camera_manual (Camera *camera, CameraText *manual) {

	SierraData *fd = (SierraData*)camera->camlib_data;

	sierra_debug_print(fd, "Getting camera manual");

	strcpy(manual->text, "Manual Not Available");

	return (GP_OK);
}

int camera_about (Camera *camera, CameraText *about) {

	SierraData *fd = (SierraData*)camera->camlib_data;

	sierra_debug_print(fd, "Getting 'about' information");

	strcpy(about->text, "sierra SPARClite library\nScott Fritzinger <scottf@unr.edu>\nSupport for sierra-based digital cameras\nincluding Olympus, Nikon, Epson, and others.\n\nThanks to Data Engines (www.dataengines.com)\nfor the use of their Olympus C-3030Z for USB\nsupport implementation.");

	return (GP_OK);
}
