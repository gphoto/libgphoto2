#include <stdio.h>
#include <string.h>
#include <gphoto2.h>
#include <gpio.h>

#include "library.h"
#include "fujitsu.h"

#define TIMEOUT	   2000

void debug_print(FujitsuData *fd, char *message) {

	if (fd->debug) {
		printf("fujitsu: ");
		printf(message);
		printf("\n");
	}
}

int camera_id (CameraText *id) {

	strcpy(id->text, "fujitsu-scottf");

	return (GP_OK);
}


char *models[] = {
	"Agfa ePhoto 307",
	"Agfa ePhoto 780",
	"Agfa ePhoto 780C",
	"Agfa ePhoto 1280",
	"Agfa ePhoto 1680",
	"Apple QuickTake 150",
	"Apple QuickTake 200",
	"Chinon ES-1000",
	"Epson PhotoPC 500",
	"Epson PhotoPC 550",
	"Epson PhotoPC 600",
	"Epson PhotoPC 700",
	"Epson PhotoPC 800",
	"Nikon CoolPix 100",
	"Nikon CoolPix 300",
	"Nikon CoolPix 700",
	"Nikon CoolPix 800",
	"Nikon CoolPix 900",
	"Nikon CoolPix 900S",
	"Nikon CoolPix 950",
	"Nikon CoolPix 950S",
	"Olympus D-100Z",
	"Olympus D-200L",
	"Olympus D-220L",
	"Olympus D-300L",
	"Olympus D-320L",
	"Olympus D-330R",
	"Olympus D-340L",
	"Olympus D-340R",
	"Olympus D-360L",
	"Olympus D-400L Zoom",
	"Olympus D-450Z",
	"Olympus D-460Z",
	"Olympus D-500L",
	"Olympus D-600L",
	"Olympus D-620L",
	"Olympus C-400L",
	"Olympus C-410L",
	"Olympus C-800L",
	"Olympus C-820L",
	"Olympus C-830L",
	"Olympus C-840L",
	"Olympus C-900 Zoom",
	"Olympus C-900L Zoom",
	"Olympus C-1000L",
	"Olympus C-1400L",
	"Olympus C-2000Z",
	"Olympus C-2500Z",
	"Olympus C-3030Z",
	"Panasonic Coolshot KXl-600A",
	"Panasonic Cardshot NV-DCF5E",
	"Polaroid PDC 640",
	"Sanyo DSC-X300",
	"Sanyo DSC-X350",
	"Sanyo VPC-G200",
	"Sanyo VPC-G200EX",
	"Sanyo VPC-G210",
	"Sanyo VPC-G250",
	"Sierra Imaging SD640",
	NULL
};

int camera_abilities (CameraAbilitiesList *list) {

	int x=0;
	CameraAbilities *a;

	while (models[x]) {
		a = gp_abilities_new();
		strcpy(a->model, models[x]);
		a->port     = GP_PORT_SERIAL;
		a->speed[0] = 9600;
		a->speed[1] = 19200;
		a->speed[2] = 38400;
		a->speed[3] = 57600;
		a->speed[4] = 115200;
		a->speed[5] = 0;
		a->capture  = 1;
		a->config   = 0;
		a->file_delete  = 1;
		a->file_preview = 1;
		a->file_put = 0;
		gp_abilities_list_append(list, a);

		x++;
	}

	return (GP_OK);
}

int camera_init (Camera *camera, CameraInit *init) {

	char *p;
	int l, value=0;
	gpio_device_settings settings;
	FujitsuData *fd;

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
	camera->functions->file_put 	= camera_file_put;
	camera->functions->file_delete 	= camera_file_delete;
	camera->functions->config_get   = camera_config_get;
	camera->functions->config_set   = camera_config_set;
	camera->functions->capture 	= camera_capture;
	camera->functions->summary	= camera_summary;
	camera->functions->manual 	= camera_manual;
	camera->functions->about 	= camera_about;

	fd = (FujitsuData*)malloc(sizeof(FujitsuData));	
	camera->camlib_data = fd;

	fd->first_packet = 1;
	fd->debug = camera->debug;

	debug_print(fd, "Initializing camera");

	fd->dev = gpio_new(GPIO_DEVICE_SERIAL);

        strcpy(settings.serial.port, init->port_settings.path);
	settings.serial.speed 	 = 19200;
	settings.serial.bits 	 = 8;
	settings.serial.parity 	 = 0;
	settings.serial.stopbits = 1;
	gpio_set_settings(fd->dev, settings);
	gpio_set_timeout(fd->dev, TIMEOUT);
	strcpy(fd->folder, "/");

	if (gpio_open(fd->dev)==GPIO_ERROR) {
		gp_camera_message(camera, "Can not open the port");
		return (GP_ERROR);
	}

	if (fujitsu_ping(camera)==GP_ERROR) {
		gp_camera_message(camera, "Can not talk to camera");
		return (GP_ERROR);
	}

	if (fujitsu_set_speed(camera, init->port_settings.speed)==GP_ERROR) {
		gp_camera_message(camera, "Can not set the serial port speed");
		return (GP_ERROR);
	}

	if (fujitsu_get_int_register(camera, 1, &value)==GP_ERROR) {
		gp_camera_message(camera, "Could not communicate with camera after speed change");
		return (GP_ERROR);
	}

	fujitsu_set_int_register(camera, 83, -1);

	gpio_set_timeout(fd->dev, 50);
	if (fujitsu_set_string_register(camera, 84, "\\", 1)==GP_ERROR)
		fd->folders = 0;
	   else
		fd->folders = 1;	

	if (fd->folders)
		debug_print(fd, "Camera supports folders");
	   else
		debug_print(fd, "Camera doesn't support folders. Using CameraFilesystem emu.");
	fd->fs = gp_filesystem_new();

	gpio_set_timeout(fd->dev, TIMEOUT);
	fd->speed = init->port_settings.speed;

	camera_stop(camera);
	return (GP_OK);
}

static int fujitsu_change_folder(Camera *camera, const char *folder)
{	
	FujitsuData *fd = (FujitsuData*)camera->camlib_data;

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
		debug_print(fd, "Change dir called with relative path?");
		i = 0;
	} else {
		if (fujitsu_set_string_register(camera, 84, "\\", 1)==GP_ERROR)
			return GP_ERROR;
	}
	st = i;
	for (; target[i]; i++) {
		if (target[i] == '/') {
			target[i] = '\0';
			if (st == i-1)
				break;
			if (fujitsu_set_string_register(camera, 84, target+st, strlen(target+st))==GP_ERROR)
				return GP_ERROR; 
			st = i+1;
			target[i] = '/';
		}
	}
	return GP_OK;
}

int camera_start(Camera *camera) {

	FujitsuData *fd = (FujitsuData*)camera->camlib_data;

	if (fujitsu_set_speed(camera, fd->speed)==GP_ERROR) {
		gp_camera_message(camera, "Can not set the serial port speed");
		return (GP_ERROR);
	}
	fujitsu_folder_set(camera, fd->folder);
	return (GP_OK);
}

int camera_stop(Camera *camera) {

	FujitsuData *fd = (FujitsuData*)camera->camlib_data;

	if (fujitsu_set_speed(camera, -1)==GP_ERROR) {
		gp_camera_message(camera, "Can not set the serial port speed");
		return (GP_ERROR);
	}

	return (GP_OK);
}

int camera_exit (Camera *camera) {

	FujitsuData *fd = (FujitsuData*)camera->camlib_data;

	debug_print(fd, "Exiting camera");

/* obsoleted by start/stop
	if (fujitsu_end_session(camera)==GP_ERROR) {
		gp_camera_message(camera, "Can not end camera session");
		return (GP_ERROR);		
	}
*/

	free(fd);

	return (GP_OK);
}

int camera_file_list(Camera *camera, CameraList *list, char *folder) {

	FujitsuData *fd = (FujitsuData*)camera->camlib_data;
	int x=0, error=0, count;
	char buf[1024];

	if ((!fd->folders)&&(strcmp("/", folder)!=0))
		return (GP_ERROR);
	if (camera_start(camera)==GP_ERROR)
		return (GP_ERROR);

	if (fd->folders) {
		if (fujitsu_change_folder(camera, folder)) {
			gp_camera_message(camera, "Can't change folder");
			return (GP_ERROR);
		}
	}

        count = fujitsu_file_count(camera);

        /* Populate the filesystem */
	gp_filesystem_populate(fd->fs, folder, "PIC%04i.jpg", count);

	while ((x<count)&&(!error)) {
#if 0
	/* are all filenames *.jpg, or are there *.tif files too? */
		/* Set the current picture number */
		if (fujitsu_set_int_register(camera, 4, x+1)==GP_ERROR) {
			gp_message("Could not set the picture number");
			camera_stop(camera);
			return (GP_ERROR);
		}
		/* Get the picture filename */
		if (fujitsu_get_string_register(camera, 79, NULL, buf, &length)==GP_ERROR)
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

	FujitsuData *fd = (FujitsuData*)camera->camlib_data;
	int count, i, bsize;
	char buf[1024];

	debug_print(fd, "Listing folders");

	if (!fd->folders) /* camera doesn't support folders */
		return GP_OK;

	if (camera_start(camera) != GP_OK) /* XXX */
		return GP_ERROR;

	if ((fujitsu_change_folder(camera, folder) != GP_OK) ||
	    (fujitsu_get_int_register(camera, 83, &count) != GP_OK)) {
		camera_stop(camera);
		return GP_ERROR;
	}
	if (count)
	for (i = 0; i < count; i++) {
		if (fujitsu_change_folder(camera, folder) != GP_OK) {
			break;
		}
		if (fujitsu_set_int_register(camera, 83, i+1) != GP_OK) {
			break;
		}
		bsize = 1024;
		if (fujitsu_get_string_register(camera, 84, NULL, buf, &bsize) != GP_OK) {
			break;
		} else {
			/* append the folder name on to the folder list */
			gp_list_append(list, buf, GP_LIST_FOLDER);
		}

	}
	camera_stop(camera);
	return (GP_OK);
}

int fujitsu_folder_set(Camera *camera, char *folder) {

/* DOUBLE CHECK THIS CODE!!! (after filename reworking!) */
	char buf[4096];
	FujitsuData *fd = (FujitsuData*)camera->camlib_data;

	/* If the camera doesn't support folders and it was passed something
	   other than "/", return an error! */
	if ((!fd->folders)&&(strcmp("/", folder)!=0))
		return (GP_ERROR);

	/* If the camera doesn't support folders and it was passed "/",
	   then that's OK */
	if ((!fd->folders)&&(strcmp("/", folder)==0))
		return (GP_OK);

	if (folder[0] != '/') {
		strcat(fd->folder, folder);
	} else {
		strcpy(fd->folder, folder);
	}

	if (fd->folder[strlen(fd->folder)-1] != '/')
	       strcat(fd->folder, "/");

	sprintf(buf, "Folder set to %s", fd->folder);

	/* TODO: check whether the selected folder is valid.
		 It should be done after implementing camera_lock/unlock pair */
	
	if (fd->folders) {
		if (fujitsu_change_folder(camera, fd->folder)) {
			gp_camera_message(camera, "Can't change folder");
			return (GP_ERROR);
		}
	}

	return (GP_OK);
}

int fujitsu_file_count (Camera *camera) {

	int value;
	FujitsuData *fd = (FujitsuData*)camera->camlib_data;

	debug_print(fd, "Counting files");

	if (fujitsu_get_int_register(camera, 10, &value)==GP_ERROR) {
                gp_camera_message(camera, "Could not get number of files on camera->");
                return (GP_ERROR);
        }

	return (value);
}

int camera_file_get_generic (Camera *camera, CameraFile *file, 
	char *folder, char *filename, int thumbnail) {

	char buf[4096];
	int length, regl, regd, file_number;
	FujitsuData *fd = (FujitsuData*)camera->camlib_data;

if (camera_start(camera)==GP_ERROR)
return (GP_ERROR);

	/* Get the file number from the CameraFileSystem */
	file_number = gp_filesystem_number(fd->fs, fd->folder, filename);

	if (thumbnail) {
		sprintf(buf, "Getting file preview #%i", file_number);
		regl = 13;
		regd = 15;
	}  else {
		sprintf(buf, "Getting file #%i", file_number);
		regl = 12;
		regd = 14;
	}
	debug_print(fd, buf);

	/* Set the current picture number */
	if (fujitsu_set_int_register(camera, 4, file_number+1)==GP_ERROR) {
		gp_camera_message(camera, "Can not set current image");
		return (GP_ERROR);
	}

	/* Get the size of the current picture number */
	if (fujitsu_get_int_register(camera, regl, &length)==GP_ERROR) {
		gp_camera_message(camera, "Can not get current image length");
		return (GP_ERROR);
	}

	/* Fill in the file structure */
	
	strcpy(file->type, "image/jpg");

	strcpy(file->name, filename);

	/* Get the picture data */
	if (fujitsu_get_string_register(camera, regd, file, NULL, NULL)==GP_ERROR) {
		gp_camera_message(camera, "Can not get picture");
		return (GP_ERROR);
	}

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

int camera_file_put (Camera *camera, CameraFile *file, char *folder) {

	FujitsuData *fd = (FujitsuData*)camera->camlib_data;

	debug_print(fd, "Putting file on camera");

	return (GP_ERROR);
}

int camera_file_delete (Camera *camera, char *folder, char *filename) {

	int ret, file_number;
	char buf[4096];
	FujitsuData *fd = (FujitsuData*)camera->camlib_data;

if (camera_start(camera)==GP_ERROR)
return (GP_ERROR);

	file_number = gp_filesystem_number(fd->fs, fd->folder, filename);

	sprintf(buf, "Deleting photo #%i", file_number+1);
	debug_print(fd, buf);

	ret = fujitsu_delete(camera, file_number+1);

if (camera_stop(camera)==GP_ERROR)
return (GP_ERROR);

	return (ret);
}

int camera_config_get (Camera *camera, CameraWidget *window) {

	FujitsuData *fd = (FujitsuData*)camera->camlib_data;

	debug_print(fd, "Building configuration window");

	return (GP_ERROR);
}

int camera_config_set (Camera *camera, CameraSetting *setting, int count) {

	FujitsuData *fd = (FujitsuData*)camera->camlib_data;

if (camera_start(camera)==GP_ERROR)
return (GP_ERROR);

	debug_print(fd, "Setting configuration values");

if (camera_stop(camera)==GP_ERROR)
return (GP_ERROR);

	return (GP_OK);
}

int camera_capture (Camera *camera, CameraFile *file, CameraCaptureInfo *info) {

	FujitsuData *fd = (FujitsuData*)camera->camlib_data;

	debug_print(fd, "Capturing image");

	return (GP_ERROR);
}

int camera_summary (Camera *camera, CameraText *summary) {

	char buf[1024*32];
	int value;
	char t[1024];
	FujitsuData *fd = (FujitsuData*)camera->camlib_data;

if (camera_start(camera)==GP_ERROR)
return (GP_ERROR);

	debug_print(fd, "Getting camera summary");
	strcpy(buf, "");

	/* Get all the string-related info */
	if (fujitsu_get_string_register(camera, 22, NULL, t, &value)!=GP_ERROR)
		sprintf(buf, "%sCamera ID       : %s\n", buf, t);
	if (fujitsu_get_string_register(camera, 25, NULL, t, &value)!=GP_ERROR)
		sprintf(buf, "%sSerial Number   : %s\n", buf, t);
	if (fujitsu_get_int_register(camera, 1, &value)!=GP_ERROR) {
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
	if (fujitsu_get_int_register(camera, 3, &value)!=GP_ERROR) {
		if (value == 0)
			strcpy(t, "Auto");
		   else
			sprintf(t, "%i microseconds", value);
		sprintf(buf, "%sShutter Speed   : %s\n", buf, t);
	}
	if (fujitsu_get_int_register(camera, 5, &value)!=GP_ERROR) {
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
	if (fujitsu_get_int_register(camera, 6, &value)!=GP_ERROR) {	
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

	if (fujitsu_get_int_register(camera, 7, &value)!=GP_ERROR) {	
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
	if (fujitsu_get_int_register(camera, 19, &value)!=GP_ERROR) {	
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
	if (fujitsu_get_int_register(camera, 20, &value)!=GP_ERROR) {	
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
	if (fujitsu_get_int_register(camera, 33, &value)!=GP_ERROR) {	
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
	if (fujitsu_get_int_register(camera, 10, &value)!=GP_ERROR)
		sprintf(buf, "%sFrames Taken    : %i\n", buf, value);
	if (fujitsu_get_int_register(camera, 11, &value)!=GP_ERROR)
		sprintf(buf, "%sFrames Left     : %i\n", buf, value);
	if (fujitsu_get_int_register(camera, 16, &value)!=GP_ERROR)
		sprintf(buf, "%sBattery Life    : %i\n", buf, value);
	if (fujitsu_get_int_register(camera, 23, &value)!=GP_ERROR)
		sprintf(buf, "%sAutoOff (host)  : %i seconds\n", buf, value);
	if (fujitsu_get_int_register(camera, 24, &value)!=GP_ERROR)
		sprintf(buf, "%sAutoOff (field) : %i seconds\n", buf, value);
	if (fujitsu_get_int_register(camera, 28, &value)!=GP_ERROR)
		sprintf(buf, "%sMemory Left	: %i bytes\n", buf, value);
	if (fujitsu_get_int_register(camera, 35, &value)!=GP_ERROR)
		sprintf(buf, "%sLCD Brightness  : %i (1-7)\n", buf, value);
	if (fujitsu_get_int_register(camera, 38, &value)!=GP_ERROR)
		sprintf(buf, "%sLCD AutoOff	: %i seconds\n", buf, value);

	strcpy(summary->text, buf);

if (camera_stop(camera)==GP_ERROR)
return (GP_ERROR);

	return (GP_OK);
}

int camera_manual (Camera *camera, CameraText *manual) {

	FujitsuData *fd = (FujitsuData*)camera->camlib_data;

	debug_print(fd, "Getting camera manual");

	strcpy(manual->text, "Manual Not Available");

	return (GP_OK);
}

int camera_about (Camera *camera, CameraText *about) {

	FujitsuData *fd = (FujitsuData*)camera->camlib_data;

	debug_print(fd, "Getting 'about' information");

	strcpy(about->text, 
"Fujitsu SPARClite library
Scott Fritzinger <scottf@unr.edu>
Support for Fujitsu-based digital cameras
including Olympus, Nikon, Epson, and others.");

	return (GP_OK);
}
