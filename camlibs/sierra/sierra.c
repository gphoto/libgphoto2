#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gphoto2.h>

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

#include "library.h"
#include "sierra.h"

#define TIMEOUT	   2000

#define CHECK_STOP(camera,result) {int res; res = result; if (res != GP_OK) {gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** operation failed!"); camera_stop (camera); return (res);}}

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
	int x;
	CameraAbilities *a;

	for (x = 0; strlen (sierra_cameras[x].model) > 0; x++) {
		a = gp_abilities_new();
		strcpy (a->model, sierra_cameras[x].model);
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
					GP_OPERATION_CAPTURE_PREVIEW |
					GP_OPERATION_CONFIG;
		a->file_operations   = 	GP_FILE_OPERATION_DELETE | 
					GP_FILE_OPERATION_PREVIEW;
		a->folder_operations = 	GP_FOLDER_OPERATION_NONE;
		a->usb_vendor  = sierra_cameras[x].usb_vendor;
		a->usb_product = sierra_cameras[x].usb_product;
		gp_abilities_list_append (list, a);
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
	camera->functions->file_get_info	= camera_file_get_info;
	camera->functions->file_get 		= camera_file_get;
	camera->functions->file_get_preview 	= camera_file_get_preview;
	camera->functions->file_delete 		= camera_file_delete;
	camera->functions->capture_preview	= camera_capture_preview;
	camera->functions->capture 		= camera_capture;
	camera->functions->get_config		= camera_get_config;
	camera->functions->set_config		= camera_set_config;
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
		if ((vendor == 0) && (product == 0)) {
			free (fd);
			return (GP_ERROR_MODEL_NOT_FOUND);
		}

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

	/* If it is a serial camera, check if it's really there. */
	if (camera->port->type == GP_PORT_SERIAL) {

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
	}
	
	if (camera->port->type == GP_PORT_USB)
		gp_port_usb_clear_halt (fd->dev, GP_PORT_USB_ENDPOINT_IN);

	ret = sierra_get_int_register (camera, 1, &value);
	if (ret != GP_OK) {
		gp_port_free(fd->dev);
		free (fd);
		return (ret);
	}

	/* FIXME??? What's that for? */
	ret = sierra_set_int_register (camera, 83, -1);
	if (ret != GP_OK) {
		gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** Could not set "
				 "register 83 to -1: %s", 
				 gp_camera_get_result_as_string (camera, ret));
	}

	CHECK (gp_port_timeout_set (fd->dev, 50));

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
	CHECK (gp_filesystem_populate (fd->fs, "/", "PIC%04i.jpg", count));

	strcpy (fd->folder, "/");

	CHECK (gp_port_timeout_set (fd->dev, TIMEOUT));

	return (camera_stop (camera));
}

static int sierra_change_folder (Camera *camera, const char *folder)
{	
	SierraData *fd = (SierraData*)camera->camlib_data;
	int st = 0,i = 1;
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
		CHECK (sierra_set_string_register (camera, 84, "\\", 1));
	}
	st = i;
	for (; target[i]; i++) {
		if (target[i] == '/') {
			target[i] = '\0';
			if (st == i - 1)
				break;
			CHECK (sierra_set_string_register (camera, 84, 
				target + st, strlen (target + st)));

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

	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** camera_start");

	if (fd->type != GP_PORT_SERIAL)
		return (GP_OK);
	
	CHECK (sierra_set_speed (camera, fd->speed));

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
	int x=0, count;

	gp_debug_printf (GP_DEBUG_LOW, "sierra", 
			 "*** camera_folder_list_files");
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** folder: %s", folder);

	if ((!fd->folders) && (strcmp ("/", folder) != 0))
		return (GP_ERROR);

	CHECK (camera_start (camera));

	if (fd->folders)
		CHECK (sierra_change_folder (camera, folder));

	count = sierra_file_count (camera);
	if (count < 0)
		return (count);

	for (x = 0; x < count; x++) {
		char buf[128];
		int length;
		
		/* Set the current picture number */
		CHECK_STOP (camera, sierra_set_int_register (camera, 4, x + 1));

		/* Get the picture filename */
		gp_debug_printf (GP_DEBUG_LOW, "sierra", 
				 "*** getting filename of picture %i...", x);
		CHECK_STOP (camera, sierra_get_string_register (camera, 79, 0, 
						NULL, buf, &length));

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

	return (camera_stop (camera));
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

	CHECK (camera_start (camera));

	CHECK_STOP (camera, sierra_change_folder (camera, folder));

	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** getting number of "
			 "folders...");
	CHECK_STOP (camera, sierra_get_int_register (camera, 83, &count));

	for (i = 0; i < count; i++) {

		ret = sierra_change_folder (camera, folder);
		if (ret != GP_OK) 
			break;

		ret = sierra_set_int_register (camera, 83, i + 1);
		if (ret != GP_OK)
			break;

		bsize = 1024;
		gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** getting name of "
				 "folder %i...", i + 1);
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

	return (camera_stop (camera));
}

int sierra_folder_set (Camera *camera, const char *folder) 
{
	char tf[4096];
	SierraData *fd = (SierraData*)camera->camlib_data;

	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** sierra_folder_set");
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** folder: %s", folder);

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
	int regl, regd, file_number;
	SierraData *fd = (SierraData*)camera->camlib_data;

	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** sierra_file_get_generic");
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** folder: %s", folder);
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** filename: %s", filename);

	CHECK (camera_start (camera));

	/* Get the file number from the CameraFileSystem */
	file_number = gp_filesystem_number (fd->fs, folder, filename);
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
	if (thumbnail) 
		strcpy (file->type, "image/jpeg");
	else {
		//FIXME: Do it better...
		if (strcmp (filename + 8, ".MOV") == 0)
			strcpy (file->type, "video/quicktime");
		else
			strcpy (file->type, "image/jpeg");
	}
	strcpy (file->name, filename);

	/* Get the picture data */
	CHECK (sierra_get_string_register (camera, regd, file_number + 1, file,
					  NULL, NULL));
	return (camera_stop (camera));
}

int camera_file_get_info (Camera *camera, const char *folder, 
			  const char *filename, CameraFileInfo *info)
{
	int file_number, l;
	SierraData *fd = (SierraData*)camera->camlib_data;

	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** camera_file_get_info");
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** folder: %s", folder);
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** filename: %s", filename);

	CHECK (camera_start (camera));

	/* Get the file number from the CameraFileSystem */
	file_number = gp_filesystem_number (fd->fs, folder, filename);
	if (file_number < 0) 
		return (file_number);
	
	/* Set the current picture number */
	CHECK_STOP (camera, sierra_set_int_register (camera, 4, file_number));

	/* Get the size of the current image */
	CHECK_STOP (camera, sierra_get_int_register (camera, 12, &l));
	info->file.fields = GP_FILE_INFO_SIZE | GP_FILE_INFO_TYPE;
	strcpy (info->file.type, "image/jpeg");
	info->file.size = l;

	/* Get the size of the current thumbnail */
	CHECK_STOP (camera, sierra_get_int_register (camera, 13, &l));
	info->preview.fields = GP_FILE_INFO_SIZE | GP_FILE_INFO_TYPE;
	//FIXME: Do it better...
	if (strcmp (filename + 8, ".MOV") == 0)
		strcpy (info->preview.type, "video/quicktime");
	else
		strcpy (info->preview.type, "image/jpeg");
	info->preview.size = l;

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
	int file_number;
	SierraData *fd = (SierraData*)camera->camlib_data;

	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** sierra_file_delete");
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** folder: %s", folder);
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** filename: %s", filename);

	CHECK (camera_start (camera));

	file_number = gp_filesystem_number (fd->fs, folder, filename);

	CHECK (sierra_delete (camera, file_number + 1));

	CHECK (gp_filesystem_delete (fd->fs, folder, filename));

	return (camera_stop(camera));
}

int camera_capture (Camera *camera, int capture_type, CameraFilePath *path) 
{
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** camera_capture");

	CHECK (camera_start (camera));

	CHECK_STOP (camera, sierra_capture (camera, capture_type, path));
	
	return (camera_stop (camera));
}

int camera_capture_preview (Camera *camera, CameraFile *file)
{
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** camera_capture_preview");

	CHECK (camera_start (camera));
	
	CHECK_STOP (camera, sierra_capture_preview (camera, file));

	return (camera_stop (camera));
}

static void dump_register (Camera *camera)
{
	int ret, value, i;
	const char *description[] = {
		"?",				//   0
		"resolution",
		"?",
		"shutter speed",
		"current image number",
		"aperture",
		"color mode",
		"flash mode",
		"?",
		"?",
		"frames taken",			//  10
		"frames left",
		"size of current image",
		"size of thumbnail of current image",
		"current preview (captured)",
		"?",
		"battery life",
		"?",
		"?",
		"brightness/contrast",
		"white balance",		//  20
		"?",
		"camera id",
		"auto off (host)",
		"auto off (field)",
		"serial number",
		"?",
		"?",
		"memory left",
		"?",
		"?",				//  30
		"?",
		"?",
		"lens mode",
		"?",
		"lcd brightness",
		"?",
		"?",
		"lcd auto off",
		"?",
		"?",				//  40
		"?", "?", "?", "?", "?", "?", "?", "?", "?",
		"?",				//  50
		"?", "?", "?", "?", "?", "?", "?", "?", "?",
		"?",				//  60
		"?", "?", "?", "?", "?", "?", "?", "?", "?",
		"spot metering mode",		//  70
		"?",
		"zoom",
		"?", "?", "?", "?", "?", "?", 
		"current filename",
		"?",				//  80
		"?", "?",
		"number of folders in current folder/folder number",
		"current folder name",
		"?", "?", "?", "?", "?",
		"?",				//  90
		"?", "?", "?", "?", "?", "?", "?", "?", "?",
		"?", 				// 100
		"?", "?", "?", "?", "?", "?", "?", "?", "?",
		"?", 				// 110
		"?", "?", "?", "?", "?", "?", "?", "?", "?",
		"?",				// 120
		"?", "?", "?", "?", "?", "?", "?", "?", "?",
		"?"				// 130
	};
		

	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** Register:");
	for (i = 0; i < 128; i++) {
		ret = sierra_get_int_register (camera, i, &value);
		if (ret == GP_OK)
			gp_debug_printf (GP_DEBUG_LOW, "sierra", 
					 "***  %3i: %12i (%s)", i, value, 
					 description [i]);
	}
}

int camera_get_config (Camera *camera, CameraWidget **window)
{
	CameraWidget *child;
	CameraWidget *section;
	char t[1024];
	int ret, value;

	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** camera_get_config");

	CHECK (camera_start (camera));
	dump_register (camera);

	gp_widget_new (GP_WIDGET_WINDOW, _("Camera Configuration"), window);
	gp_widget_new (GP_WIDGET_SECTION, _("Picture Settings"), &section);
	gp_widget_append (*window, section);

	/* Resolution */
	ret = sierra_get_int_register (camera, 1, &value);
        if (ret == GP_OK) {

		gp_widget_new (GP_WIDGET_RADIO, _("Resolution"), &child);
		gp_widget_add_choice (child, _("Auto"));
		gp_widget_add_choice (child, _("Standard"));
		gp_widget_add_choice (child, _("High"));
		gp_widget_add_choice (child, _("Best"));
		
                switch (value) {
		case 0: strcpy (t, _("Auto"));
			break;
                case 1: strcpy (t, _("Standard"));
                        break;
                case 2: strcpy (t, _("High"));
                        break;
                case 3: strcpy (t, _("Best"));
                        break;
                default:
                	sprintf (t, _("%i (unknown)"), value);
			gp_widget_add_choice (child, t);
                }
		gp_widget_set_value (child, t);
		gp_widget_append (section, child);
        }

	/* Shutter Speed */
        ret = sierra_get_int_register (camera, 3, &value);
        if (ret == GP_OK) {
		
		gp_widget_new (GP_WIDGET_RANGE, 
			       _("Shutter Speed (microseconds)"), 
			       &child);
		gp_widget_set_range (child, 0, 2000, 1);
		gp_widget_set_value (child, &value);
		gp_widget_append (section, child);
        }

	/* Aperture */
        ret = sierra_get_int_register (camera, 5, &value);
        if (ret == GP_OK) {

		gp_widget_new (GP_WIDGET_RADIO, _("Aperture"), &child);
		gp_widget_add_choice (child, _("Auto"));
		gp_widget_add_choice (child, _("Low"));
		gp_widget_add_choice (child, _("Medium"));
		gp_widget_add_choice (child, _("High"));

                switch (value) {
		case 0: strcpy (t, _("Auto"));
			break;
                case 1: strcpy (t, _("Low"));
                        break;
                case 2: strcpy (t, _("Medium"));
                        break;
                case 3: strcpy (t, _("High"));
                        break;
                default:
                        sprintf(t, _("%i (unknown)"), value);
			gp_widget_add_choice (child, t);
                }
		gp_widget_set_value (child, t);
		gp_widget_append (section, child);
        }

	/* Color Mode */
        ret = sierra_get_int_register (camera, 6, &value);
        if (ret == GP_OK) {

		// Those values are for a C-2020 Z. If your model differs, we
		// have to distinguish models here...
		gp_widget_new (GP_WIDGET_RADIO, _("Color Mode"), &child);
		gp_widget_add_choice (child, _("Normal"));
		gp_widget_add_choice (child, _("Black/White"));
		gp_widget_add_choice (child, _("Sepia"));
		gp_widget_add_choice (child, _("White Board"));
		gp_widget_add_choice (child, _("Black Board"));

                switch (value) {
		case 0: strcpy (t, _("Normal"));
			break;
                case 1: strcpy (t, _("Black/White"));
                        break;
                case 2: strcpy (t, _("Sepia"));
                        break;
		case 3: strcpy (t, _("White Board"));
			break;
		case 4: strcpy (t, _("Black Board"));
			break;
                default:
                        sprintf (t, _("%i (unknown)"), value);
			gp_widget_add_choice (child, t);
                }
		gp_widget_set_value (child, t);
		gp_widget_append (section, child);
        }

	/* Flash Mode */
	ret = sierra_get_int_register (camera, 7, &value);
        if (ret == GP_OK) {

		gp_widget_new (GP_WIDGET_RADIO, _("Flash Mode"), &child);
		gp_widget_add_choice (child, _("Auto"));
		gp_widget_add_choice (child, _("Force"));
		gp_widget_add_choice (child, _("Off"));
		gp_widget_add_choice (child, _("Red-eye Reduction"));
		gp_widget_add_choice (child, _("Slow Sync"));

                switch (value) {
                case 0: strcpy (t, _("Auto"));
                        break;
                case 1: strcpy (t, _("Force"));
                        break;
                case 2: strcpy (t, _("Off"));
                        break;
                case 3: strcpy (t, _("Red-eye Reduction"));
                        break;
                case 4: strcpy (t, _("Slow Sync"));
                        break;
                default:
                        sprintf (t, _("%i (unknown)"), value);
			gp_widget_add_choice (child, t);
                }
		gp_widget_set_value (child, t);
		gp_widget_append (section, child);
        }

	/* Brightness/Contrast */
        ret = sierra_get_int_register (camera, 19, &value);
        if (ret == GP_OK) {

		gp_widget_new (GP_WIDGET_RADIO, _("Brightness/Contrast"), 
			       &child);
		gp_widget_add_choice (child, _("Normal"));
		gp_widget_add_choice (child, _("Bright+"));
		gp_widget_add_choice (child, _("Bright-"));
		gp_widget_add_choice (child, _("Contrast+"));
		gp_widget_add_choice (child, _("Contrast-"));

                switch (value) {
                case 0: strcpy (t, _("Normal"));
                        break;
                case 1: strcpy (t, _("Bright+"));
                        break;
                case 2: strcpy (t, _("Bright-"));
                        break;
                case 3: strcpy (t, _("Contrast+"));
                        break;
                case 4: strcpy (t, _("Contrast-"));
                        break;
                default:
                        sprintf (t, _("%i (unknown)"), value);
			gp_widget_add_choice (child, t);
                }
		gp_widget_set_value (child, t);
		gp_widget_append (section, child);
        }

	/* White Balance */
        ret = sierra_get_int_register (camera, 20, &value);
        if (ret == GP_OK) {
		
		gp_widget_new (GP_WIDGET_RADIO, _("White Balance"), &child);
		gp_widget_add_choice (child, _("Auto"));
		gp_widget_add_choice (child, _("Skylight"));
		gp_widget_add_choice (child, _("Fluorescent"));
		gp_widget_add_choice (child, _("Tungsten"));
		gp_widget_add_choice (child, _("Cloudy"));

                switch (value) {
                case 0: strcpy (t, _("Auto"));
                        break;
                case 1: strcpy (t, _("Skylight"));
                        break;
                case 2: strcpy (t, _("Fluorescent"));
                        break;
                case 3: strcpy (t, _("Tungsten"));
                        break;
                case 255:
                        strcpy (t, _("Cloudy"));
                        break;
                default:
                        sprintf (t, _("%i (unknown)"), value);
			gp_widget_add_choice (child, t);
                }
		gp_widget_set_value (child, t);
		gp_widget_append (section, child);
        }

	/* Lens Mode */
        ret = sierra_get_int_register (camera, 33, &value);
        if (ret == GP_OK) {

		gp_widget_new (GP_WIDGET_RADIO, _("Lens Mode"), &child);
		gp_widget_add_choice (child, _("Macro"));
		gp_widget_add_choice (child, _("Normal"));
		gp_widget_add_choice (child, _("Infinity/Fish-eye"));

                switch (value) {
                case 1: strcpy (t, _("Macro"));
                        break;
                case 2: strcpy (t, _("Normal"));
                        break;
                case 3: strcpy (t, _("Infinity/Fish-eye"));
                        break;
                default:
                        sprintf (t, _("%i (unknown)"), value);
			gp_widget_add_choice (child, t);
                }
		gp_widget_set_value (child, t);
		gp_widget_append (section, child);
        }

	/* Spot Metering Mode */
	ret = sierra_get_int_register (camera, 70, &value);
	if (ret == GP_OK) {
		
		gp_widget_new (GP_WIDGET_RADIO, _("Spot Metering Mode"), 
			       &child);
		gp_widget_add_choice (child, _("On"));
		gp_widget_add_choice (child, _("Off"));

		switch (value) {
		case 5:	strcpy (t, _("Off"));
			break;
		case 3: strcpy (t, _("On"));
			break;
		default:
			sprintf (t, _("%i (unknown)"), value);
			gp_widget_add_choice (child, t);
		}
		gp_widget_set_value (child, t);
		gp_widget_append (section, child);
	}

	/* Zoom */
	ret = sierra_get_int_register (camera, 72, &value);
	if (ret == GP_OK) {

		gp_widget_new (GP_WIDGET_RADIO, _("Zoom"), &child);
		gp_widget_add_choice (child, _("1x"));
		gp_widget_add_choice (child, _("1.6x"));
		gp_widget_add_choice (child, _("2x"));
		gp_widget_add_choice (child, _("2.5x"));

		switch (value) {
		case 0: strcpy (t, _("1x"));
			break;
		case 8: strcpy (t, _("2x"));
			break;
		case 520:
			strcpy (t, _("1.6x"));
			break;
		case 1032:
			strcpy (t, _("2.5x"));
			break;
		default:
			sprintf (t, _("%i (unknown)"), value);
			gp_widget_add_choice (child, t);
		}
		gp_widget_set_value (child, t);
		gp_widget_append (section, child);
	}

	gp_widget_new (GP_WIDGET_SECTION, _("Camera Settings"), &section);
	gp_widget_append (*window, section);

	/* Auto Off (host) */
	ret = sierra_get_int_register (camera, 23, &value);
	if (ret == GP_OK) {
		
		gp_widget_new (GP_WIDGET_RANGE, _("Auto Off (host) "
				       "(in seconds)"), &child);
		gp_widget_set_range (child, 0, 255, 1);
		gp_widget_set_value (child, &value);
		gp_widget_append (section, child);
	}

	/* Auto Off (field) */
	ret = sierra_get_int_register (camera, 24, &value);
	if (ret == GP_OK) {

		gp_widget_new (GP_WIDGET_RANGE, _("Auto Off (field) "
				       "(in seconds)"), &child);
		gp_widget_set_range (child, 0, 255, 1);
		gp_widget_set_value (child, &value);
		gp_widget_append (section, child);
	}

	/* LCD Brightness */
	ret = sierra_get_int_register (camera, 35, &value);
	if (ret == GP_OK) {
		
		gp_widget_new (GP_WIDGET_RANGE, _("LCD Brightness"), &child);
		gp_widget_set_range (child, 1, 7, 1);
		gp_widget_set_value (child, &value);
		gp_widget_append (section, child);
	}

	/* LCD Auto Off */
	ret = sierra_get_int_register (camera, 38, &value);
	if (ret == GP_OK) {

		gp_widget_new (GP_WIDGET_RANGE, _("LCD Auto Off (in "
				       "seconds)"), &child);
		gp_widget_set_range (child, 0, 255, 1);
		gp_widget_set_value (child, &value);
		gp_widget_append (section, child);
	}

	return (camera_stop (camera));
}

int camera_set_config (Camera *camera, CameraWidget *window)
{
	CameraWidget *child;
	char *value;
	int i = 0;

	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** camera_set_config");

	CHECK (camera_start (camera));

	/* Resolution */
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** setting resolution");
	if ((gp_widget_get_child_by_label (window, _("Resolution"), &child)
		== GP_OK) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &value);
		if (strcmp (value, _("Auto")) == 0) {
			i = 0;
		} else if (strcmp (value, _("Standard")) == 0) {
			i = 1;
		} else if (strcmp (value, _("High")) == 0) {
			i = 2;
		} else if (strcmp (value, _("Best")) == 0) {
			i = 3;
		} else
			return (GP_ERROR_NOT_SUPPORTED);
		CHECK_STOP (camera, sierra_set_int_register (camera, 1, i));
	}
	
	/* Shutter Speed */
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** setting shutter speed");
	if ((gp_widget_get_child_by_label (window, 
		_("Shutter Speed (microseconds)"), &child) == GP_OK) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &i);
		CHECK_STOP (camera, sierra_set_int_register (camera, 3, i));
	}

	/* Aperture */
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** setting aperture");
	if ((gp_widget_get_child_by_label (window, _("Aperture"), &child) 
		== GP_OK) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &value);
		if (strcmp (value, _("Auto")) == 0) {
			i = 0;
		} else if (strcmp (value, _("Low")) == 0) {
			i = 1;
		} else if (strcmp (value, _("Medium")) == 0) {
			i = 2;
		} else if (strcmp (value, _("High")) == 0) {
			i = 3;
		} else
			return (GP_ERROR_NOT_SUPPORTED);
		CHECK_STOP (camera, sierra_set_int_register (camera, 5, i));
	}

	/* Color Mode */
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** setting color mode");
	if ((gp_widget_get_child_by_label (window, _("Color Mode"), &child)
		== GP_OK) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &value);
		if (strcmp (value, _("Normal")) == 0) {
			i = 0;
		} else if (strcmp (value, _("Black/White")) == 0) {
			i = 1;
		} else if (strcmp (value, _("Sepia")) == 0) {
			i = 2;
		} else if (strcmp (value, _("White Board")) == 0) {
			i = 3;
		} else if (strcmp (value, _("Black Board")) == 0) {
			i = 4;
		} else
			return (GP_ERROR_NOT_SUPPORTED);
		CHECK_STOP (camera, sierra_set_int_register (camera, 6, i));
	}

	/* Flash Mode */
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** setting flash mode");
	if ((gp_widget_get_child_by_label (window, _("Flash Mode"), &child)
		== GP_OK) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &value);
		if (strcmp (value, _("Auto")) == 0) {
			i = 0;
		} else if (strcmp (value, _("Force")) == 0) {
			i = 1;
		} else if (strcmp (value, _("Off")) == 0) {
			i = 2;
		} else if (strcmp (value, _("Red-eye Reduction")) == 0) {
			i = 3;
		} else if (strcmp (value, _("Slow Sync")) == 0) {
			i = 4;
		} else
			return (GP_ERROR_NOT_SUPPORTED);
		CHECK_STOP (camera, sierra_set_int_register (camera, 7, i));
	}

	/* Brightness/Contrast */
	gp_debug_printf (GP_DEBUG_LOW, "sierra", 
			 "*** setting brightness/contrast");
	if ((gp_widget_get_child_by_label (window, _("Brightness/Contrast"), 
		&child) == GP_OK) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &value);
		if (strcmp (value, _("Normal")) == 0) {
			i = 0;
		} else if (strcmp (value, _("Bright+")) == 0) {
			i = 1;
		} else if (strcmp (value, _("Bright-")) == 0) {
			i = 2;
		} else if (strcmp (value, _("Contrast+")) == 0) {
			i = 3;
		} else if (strcmp (value, _("Contrast-")) == 0) {
			i = 4;
		} else
			return (GP_ERROR_NOT_SUPPORTED);
		CHECK_STOP (camera, sierra_set_int_register (camera, 19, i));
	}

	/* White Balance */
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** setting white balance");
	if ((gp_widget_get_child_by_label (window, _("White Balance"), &child)
		== GP_OK) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &value);
		if (strcmp (value, _("Auto")) == 0) {
			i = 0;
		} else if (strcmp (value, _("Skylight")) == 0) {
			i = 1;
		} else if (strcmp (value, _("Fluorescent")) == 0) {
			i = 2;
		} else if (strcmp (value, _("Tungsten")) == 0) {
			i = 3;
		} else if (strcmp (value, _("Cloudy")) == 0) {
			i = 4;
		} else
			return (GP_ERROR_NOT_SUPPORTED);
		CHECK_STOP (camera, sierra_set_int_register (camera, 20, i));
	}

	/* Lens Mode */
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** setting lens mode");
	if ((gp_widget_get_child_by_label (window, _("Lens Mode"), &child)
		== GP_OK) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &value);
                if (strcmp (value, _("Macro")) == 0) {
			i = 1;
		} else if (strcmp (value, _("Normal")) == 0) {
			i = 2;
		} else if (strcmp (value, _("Infinity/Fish-eye")) == 0) {
			i = 3;
		} else
			return (GP_ERROR_NOT_SUPPORTED);
		CHECK_STOP (camera, sierra_set_int_register (camera, 33, i));
        }

	/* Spot Metering Mode */
	gp_debug_printf (GP_DEBUG_LOW, "sierra", 
			 "*** setting spot metering mode");
	if ((gp_widget_get_child_by_label (window, _("Spot Metering Mode"), 
		&child) == GP_OK) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &value);
		if (strcmp (value, _("On")) == 0) {
			i = 3;
		} else if (strcmp (value, _("Off")) == 0) {
			i = 5;
		} else
			return (GP_ERROR_NOT_SUPPORTED);
		CHECK_STOP (camera, sierra_set_int_register (camera, 33, i));
	}

	/* Zoom */
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** setting zoom");
	if ((gp_widget_get_child_by_label (window, _("Zoom"), &child) 
		== GP_OK) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &value);
		if (strcmp (value, "1x") == 0) {
			i = 0;
		} else if (strcmp (value, "2x") == 0) {
			i = 8;
		} else if (strcmp (value, "1.6x") == 0) {
			i = 520;
		} else if (strcmp (value, "2.5x") == 0) {
			i = 1032;
		} else 
			return (GP_ERROR_NOT_SUPPORTED);
		CHECK_STOP (camera, sierra_set_int_register (camera, 72, i));
	}

        /* Auto Off (host) */
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** setting auto off (host)");
	if ((gp_widget_get_child_by_label (window, _("Auto Off (host) "
					  "(in seconds)"), &child) == GP_OK) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &i);
                CHECK_STOP (camera, sierra_set_int_register (camera, 23, i));
        }

        /* Auto Off (field) */
	gp_debug_printf (GP_DEBUG_LOW, "sierra", 
			 "*** setting auto off (field)");
	if ((gp_widget_get_child_by_label (window, _("Auto Off (field) "
		"(in seconds)"), &child) == GP_OK) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &i);
		CHECK_STOP (camera, sierra_set_int_register (camera, 24, i));
	}

        /* LCD Brightness */
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** setting lcd brightness");
	if ((gp_widget_get_child_by_label (window, 
		_("LCD Brightness"), &child) == GP_OK) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &i);
		CHECK_STOP (camera, sierra_set_int_register (camera, 35, i));
	}

        /* LCD Auto Off */
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** setting lcd auto off");
	if ((gp_widget_get_child_by_label (window, 
		_("LCD Auto Off (in seconds)"), &child) == GP_OK) &&
	    gp_widget_changed (child)) {
		gp_widget_get_value (child, &i);
		CHECK_STOP (camera, sierra_set_int_register (camera, 38, i));
        }

	return (camera_stop (camera));
}

int camera_summary (Camera *camera, CameraText *summary) 
{
	char buf[1024*32];
	int value, ret;
	char t[1024];

	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** camera_summary");

	CHECK (camera_start (camera));

	strcpy(buf, "");

	/* Get all the string-related info */
	ret = sierra_get_string_register (camera, 22, 0, NULL, t, &value);
	if (ret == GP_OK)
		sprintf(buf, _("%sCamera ID       : %s\n"), buf, t);

	ret = sierra_get_string_register (camera, 25, 0, NULL, t, &value);
	if (ret == GP_OK)
		sprintf(buf, _("%sSerial Number   : %s\n"), buf, t);

	/* Get all the integer information */
	if (sierra_get_int_register(camera, 10, &value) == GP_OK)
		sprintf (buf, _("%sFrames Taken    : %i\n"), buf, value);
	if (sierra_get_int_register(camera, 11, &value) == GP_OK)
		sprintf (buf, _("%sFrames Left     : %i\n"), buf, value);
	if (sierra_get_int_register(camera, 16, &value) == GP_OK)
		sprintf (buf, _("%sBattery Life    : %i\n"), buf, value);
	if (sierra_get_int_register(camera, 28, &value) == GP_OK)
		sprintf (buf, _("%sMemory Left	: %i bytes\n"), buf, value);

	strcpy(summary->text, buf);

	return (camera_stop(camera));
}

int camera_manual (Camera *camera, CameraText *manual) 
{
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** camera_manual");

	strcpy (manual->text, _("Some notes:\n"
		"(1) Camera Configuration:\n"
		"    A value of 0 will take the default one (auto).\n"));
	return (GP_OK);
}

int camera_about (Camera *camera, CameraText *about) 
{
	gp_debug_printf (GP_DEBUG_LOW, "sierra", "*** camera_about");
	
	strcpy (about->text, 
		_("sierra SPARClite library\n"
		"Scott Fritzinger <scottf@unr.edu>\n"
		"Support for sierra-based digital cameras\n"
		"including Olympus, Nikon, Epson, and others.\n"
		"\n"
		"Thanks to Data Engines (www.dataengines.com)\n"
		"for the use of their Olympus C-3030Z for USB\n"
		"support implementation."));

	return (GP_OK);
}

