#include <stdio.h>
#include <string.h>
#include <gphoto2.h>
#include <gpio/gpio.h>

#include "library.h"
#include "fujitsu.h"

int 		glob_debug	=0;
gpio_device*	glob_dev;
char		glob_folder[128];

void debug_print(char *message) {

	if (glob_debug) {
		printf("fujitsu: ");
		printf(message);
		printf("\n");
	}
}

int camera_id (char *id) {

	strcpy(id, "fujitsu-scottf");

	return (GP_OK);
}

int camera_debug_set (int value) {

	glob_debug=value;

        return (GP_OK);
}

char *models[] = {
	"Agfa ePhoto 307",
	"Agfa ePhoto 780",
	"Agfa ePhoto 780C",
	"Agfa ePhoto 1280",
	"Agfa ePhoto 1680",
	"Epson PhotoPC 500",
	"Epson PhotoPC 550",
	"Epson PhotoPC 600",
	"Epson PhotoPC 700",
	"Epson PhotoPC 800",
	"Nikon CoolPix 100",
	"Nikon CoolPix 300",
	"Nikon CoolPix 600",
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
	"Olympus C-3030Z",
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

int camera_abilities (CameraAbilities *abilities, int *count) {

	int x=0;
	CameraAbilities a;

	debug_print("getting camera abilities");

	/* Fill in a template */
	strcpy(a.model, "");
	a.serial   = 1;
	a.parallel = 0;
	a.usb      = 0;
	a.ieee1394 = 0;
	a.speed[0] = 9600;
	a.speed[1] = 19200;
	a.speed[2] = 38400;
	a.speed[3] = 57600;
	a.speed[4] = 115200;
	a.speed[5] = 0;
	a.capture  = 1;
	a.config   = 0;
	a.file_delete  = 1;
	a.file_preview = 1;
	a.file_put = 0;

	while (models[x]) {
		memcpy(&abilities[x], &a, sizeof(a));
		strcpy(abilities[x].model, models[x]);
		x++;
	}

	*count = x;
	return (GP_OK);
}

int camera_init (CameraInit *init) {

	char *p;

	int l, value=0;
	gpio_device_settings settings;

	debug_print("Initializing camera");

	glob_dev = NULL;

	glob_dev = gpio_new(GPIO_DEVICE_SERIAL);
	if (!glob_dev)
		return (GP_ERROR);

        strcpy(settings.serial.port, init->port_settings.path);
	settings.serial.speed 	 = 19200;
	settings.serial.bits 	 = 8;
	settings.serial.parity 	 = 0;
	settings.serial.stopbits = 1;
	gpio_set_settings(glob_dev, settings);

	if (gpio_open(glob_dev)==GPIO_ERROR) {
		gp_message("Can not open the port");
		return (GP_ERROR);
	}

	if (fujitsu_ping(glob_dev)==GP_ERROR) {
		gp_message("Can not talk to camera");
		return (GP_ERROR);
	}

	if (fujitsu_set_speed(glob_dev, init->port_settings.speed)==GP_ERROR) {
		gp_message("Can not set the serial port speed");
		return (GP_ERROR);
	}

	if (fujitsu_get_int_register(glob_dev, 1, &value)==GP_ERROR) {
		gp_message("Can not talk to camera after speed change");
		return (GP_ERROR);
	}

	return (GP_OK);
}

int camera_exit () {

	debug_print("Exiting camera");

	if (fujitsu_end_session(glob_dev)==GP_ERROR) {
		gp_message("Can not end camera session");
		return (GP_ERROR);		
	}

	return (GP_OK);
}

int camera_folder_list(char *folder_name, CameraFolderInfo *list) {

	debug_print("Listing folders");

	strcpy(list[0].name, "<photos>");

	/* return only 1 folder (root) on camera */
	return (1);
}

int camera_folder_set(char *folder_name) {

	char buf[4096];

	sprintf(buf, "Setting folder to %s", folder_name);
	debug_print(buf);

	strcpy(glob_folder, folder_name);

	return (GP_OK);
}

int camera_file_count () {

	int value;

	debug_print("Counting files");

	if (fujitsu_get_int_register(glob_dev, 10, &value)==GP_ERROR) {
                gp_message("Could not get number of files on camera.");
                return (GP_ERROR);
        }

	return (value);
}

int camera_file_get_generic (int file_number, CameraFile *file, int thumbnail) {

	char buf[4096];
	int length, regl, regd;

	if (thumbnail) {
		sprintf(buf, "Getting file preview #%i", file_number);
		regl = 13;
		regd = 15;
	}  else {
		sprintf(buf, "Getting file #%i", file_number);
		regl = 12;
		regd = 14;
	}
	debug_print(buf);

	/* Set the current picture number */
	if (fujitsu_set_int_register(glob_dev, 4, file_number+1)==GP_ERROR) {
		gp_message("Can not set current image");
		return (GP_ERROR);
	}

	/* Get the size of the current picture number */
	if (fujitsu_get_int_register(glob_dev, regl, &length)==GP_ERROR) {
		gp_message("Can not get current image length");
		return (GP_ERROR);
	}

	/* Fill in the file structure */
	file->type = GP_FILE_JPEG;
	file->size = length;
	strcpy(file->name, buf);

	/* Allocate the picture */
	file->data = (char *)malloc(sizeof(char)*(length+16));
	if (!file->data) {
		debug_print("Could not allocate picture space");
		return (GP_ERROR);
	}

	/* Get the picture filename */
	if (fujitsu_get_string_register(glob_dev, 79, file->name, &length)==GP_ERROR) {
		gp_message("Can not get picture filename");
		return (GP_ERROR);
	}

	/* In case the register doesn't have the filename, use this one */
	if (length == 0)
		sprintf(file->name, "fujitsu-%03i.jpg", file_number);

	/* Prepend the "thumbnail-" prefix */
	if (thumbnail) {
		sprintf(buf, "thumbnail-%s", file->name);
		strcpy(file->name, buf);
	}

	/* Get the picture data */
	if (fujitsu_get_string_register(glob_dev, regd, file->data, &length)==GP_ERROR) {
		gp_message("Can not get picture");
		return (GP_ERROR);
	}

	return (GP_OK);
}

int camera_file_get (int file_number, CameraFile *file) {

	return (camera_file_get_generic(file_number, file, 0));
}

int camera_file_get_preview (int file_number, CameraFile *preview) {

	return (camera_file_get_generic(file_number, preview, 1));
}

int camera_file_put (CameraFile *file) {

	debug_print("Putting file on camera");

	return (GP_ERROR);
}

int camera_file_delete (int file_number) {

	char buf[4096];

	sprintf(buf, "Deleting file #%i", file_number);
	debug_print(buf);

	return (fujitsu_delete(glob_dev, file_number+1));
}

int camera_config_get (CameraWidget *window) {

	debug_print("Building configuration window");

        return (GP_ERROR);
}

int camera_config_set (CameraSetting *setting, int count) {

	debug_print("Setting configuration values");

	return (GP_ERROR);
}

int camera_capture (CameraFileType type) {

	debug_print("Capturing image");

	return (GP_ERROR);
}

int camera_summary (char *summary) {

	char buf[1024*32];
	int value;
	char t[1024];

	debug_print("Getting camera summary");
	strcpy(buf, "");

	/* Get all the string-related info */
	if (fujitsu_get_string_register(glob_dev, 22, t, &value)!=GP_ERROR)
		sprintf(buf, "%sCamera ID       : %s\n", buf, t);
	if (fujitsu_get_string_register(glob_dev, 25, t, &value)!=GP_ERROR)
		sprintf(buf, "%sSerial Number   : %s\n", buf, t);
	if (fujitsu_get_int_register(glob_dev, 1, &value)!=GP_ERROR) {
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
	if (fujitsu_get_int_register(glob_dev, 3, &value)!=GP_ERROR) {
		if (value == 0)
			strcpy(t, "Auto");
		   else
			sprintf(t, "%i microseconds", value);
		sprintf(buf, "%sShutter Speed   : %s\n", buf, t);
	}
	if (fujitsu_get_int_register(glob_dev, 5, &value)!=GP_ERROR) {
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
	if (fujitsu_get_int_register(glob_dev, 6, &value)!=GP_ERROR) {	
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

	if (fujitsu_get_int_register(glob_dev, 7, &value)!=GP_ERROR) {	
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
	if (fujitsu_get_int_register(glob_dev, 19, &value)!=GP_ERROR) {	
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
	if (fujitsu_get_int_register(glob_dev, 20, &value)!=GP_ERROR) {	
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
	if (fujitsu_get_int_register(glob_dev, 33, &value)!=GP_ERROR) {	
		switch(value) {
			case 1: strcpy(t, "Macro");
				break;
			case 2:	strcpy(t, "Normal");
				break;
			case 3:	strcpy(t, "Inifity/Fish-eye");
				break;
			default:
				sprintf(t, "%i (unknown)", value);
		}
		sprintf(buf, "%sLens Mode       : %s\n", buf, t);
	}

	/* Get all the integer information */
	if (fujitsu_get_int_register(glob_dev, 10, &value)!=GP_ERROR)
		sprintf(buf, "%sFrames Taken    : %i\n", buf, value);
	if (fujitsu_get_int_register(glob_dev, 11, &value)!=GP_ERROR)
		sprintf(buf, "%sFrames Left     : %i\n", buf, value);
	if (fujitsu_get_int_register(glob_dev, 16, &value)!=GP_ERROR)
		sprintf(buf, "%sBattery Life    : %i\n", buf, value);
	if (fujitsu_get_int_register(glob_dev, 23, &value)!=GP_ERROR)
		sprintf(buf, "%sAutoOff (host)  : %i seconds\n", buf, value);
	if (fujitsu_get_int_register(glob_dev, 24, &value)!=GP_ERROR)
		sprintf(buf, "%sAutoOff (field) : %i seconds\n", buf, value);
	if (fujitsu_get_int_register(glob_dev, 28, &value)!=GP_ERROR)
		sprintf(buf, "%sMemory Left	: %i bytes\n", buf, value);
	if (fujitsu_get_int_register(glob_dev, 35, &value)!=GP_ERROR)
		sprintf(buf, "%sLCD Brightness  : %i (1-7)\n", buf, value);
	if (fujitsu_get_int_register(glob_dev, 38, &value)!=GP_ERROR)
		sprintf(buf, "%sLCD AutoOff	: %i seconds\n", buf, value);

	strcpy(summary, buf);

	return (GP_OK);
}

int camera_manual (char *manual) {

	debug_print("Getting camera manual");

	strcpy(manual, "Manual Not Available");

	return (GP_OK);
}

int camera_about (char *about) {

	debug_print("Getting 'about' information");

	strcpy(about, 
"Fujitsu SPARClite library
Scott Fritzinger <scottf@unr.edu>
Support for Fujitsu-based digital cameras
including Olympus, Nikon, Epson, and others.");

	return (GP_OK);
}
