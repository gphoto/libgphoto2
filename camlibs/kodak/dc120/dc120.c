#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gphoto2.h>
#include <gphoto2-port.h>

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

#include "dc120.h"
#include "library.h"

char *dc120_folder_memory = _("Internal Memory");
char *dc120_folder_card   = _("CompactFlash Card");

int camera_id (CameraText *id) {

	strcpy(id->text, "kodak-dc120");

	return (GP_OK);
}

int camera_abilities (CameraAbilitiesList *list) {

	CameraAbilities *a;

	gp_abilities_new(&a);

	strcpy(a->model, "Kodak DC120");
	a->port     = GP_PORT_SERIAL;
	a->speed[0] = 9600;
	a->speed[1] = 19200;
	a->speed[2] = 38400;
	a->speed[3] = 57600;
	a->speed[4] = 115200;
	a->speed[5] = 0;
	a->operations        = 	GP_OPERATION_CAPTURE_IMAGE;
	a->file_operations   = 	GP_FILE_OPERATION_DELETE | 
				GP_FILE_OPERATION_PREVIEW;
	a->folder_operations = 	GP_FOLDER_OPERATION_NONE;

	gp_abilities_list_append(list, a);

	return (GP_OK);
}

int camera_init (Camera *camera) {

	gp_port_settings settings;
	DC120Data *dd;
        int ret;

	if (!camera)
		return (GP_ERROR);

	dd = (DC120Data*)malloc(sizeof(DC120Data));
	if (!dd)
		return (GP_ERROR);

	/* First, set up all the function pointers */
	camera->functions->id 		= camera_id;
	camera->functions->abilities 	= camera_abilities;
	camera->functions->init 	= camera_init;
	camera->functions->exit 	= camera_exit;
	camera->functions->folder_list_folders  = camera_folder_list_folders;
	camera->functions->folder_list_files	= camera_folder_list_files;
	camera->functions->file_get 	= camera_file_get;
	camera->functions->file_delete 	= camera_file_delete;
	camera->functions->summary	= camera_summary;
	camera->functions->manual 	= camera_manual;
	camera->functions->about 	= camera_about;

        if ((ret= gp_port_new(&(dd->dev), GP_PORT_SERIAL)) < 0) {
		free(dd);
		return (ret);
	}

	strcpy(settings.serial.port, camera->port_info->path);
	settings.serial.speed    = 9600;
	settings.serial.bits     = 8;
	settings.serial.parity   = 0;
	settings.serial.stopbits = 1;

	if (gp_port_settings_set(dd->dev, settings) == GP_ERROR) {
		gp_port_free(dd->dev);
		free(dd);
		return (GP_ERROR);
	}

	if (gp_port_open(dd->dev) == GP_ERROR) {
		gp_port_free(dd->dev);
		free(dd);
		return (GP_ERROR);
	}

	gp_port_timeout_set(dd->dev, TIMEOUT);

	/* Reset the camera to 9600 */
	gp_port_send_break(dd->dev, 2);

	/* Wait for it to update */
	GP_SYSTEM_SLEEP(1500);

	if (dc120_set_speed(dd, camera->port_info->speed) == GP_ERROR) {
		gp_port_close(dd->dev);
		gp_port_free(dd->dev);
		free(dd);
		return (GP_ERROR);
	}

	/* Try to talk after speed change */
	if (dc120_get_status(dd) == GP_ERROR) {
		gp_port_close(dd->dev);
		gp_port_free(dd->dev);
		free(dd);
		return (GP_ERROR);
	}

	/* Everything went OK. Save the data*/
	gp_filesystem_new(&dd->fs);
	camera->camlib_data = dd;

	return (GP_OK);
}

int camera_exit (Camera *camera) {

	DC120Data *dd = camera->camlib_data;

	if (!dd)
		return (GP_OK);

	gp_filesystem_free(dd->fs);

	if (dd->dev) {
		if (gp_port_close(dd->dev) == GP_ERROR)
			{ /* camera did a bad, bad thing */ }
		gp_port_free(dd->dev);
	}
	free(dd);

	return (GP_OK);
}

int camera_folder_list_folders (Camera *camera, const char *folder, 
				CameraList *list) 
{
	DC120Data *dd = camera->camlib_data;
	char buf[32];

	if (strcmp(folder, "/")==0) {
		gp_list_append(list, dc120_folder_memory, NULL);
		gp_list_append(list, dc120_folder_card, NULL);
		return (GP_OK);
	}

	/* Chop trailing slash */
	if (folder[strlen(folder)-1] == '/')
		((char*) folder) [strlen(folder)-1] = 0;

	sprintf(buf, "/%s", dc120_folder_memory);
	if (strcmp(folder, buf)==0)
		/* From memory */
		return (dc120_get_albums(dd, 0, list));

	sprintf(buf, "/%s", dc120_folder_card);
	if (strcmp(folder, buf)==0)
		/* From cf card */
		return (dc120_get_albums(dd, 1, list));
	return (GP_ERROR);
}

int camera_folder_list_files (Camera *camera, const char *folder, 
			      CameraList *list) 
{
	DC120Data *dd = camera->camlib_data;
	char buf[32];
	int retval = GP_OK;
	int x;
	const char *name;

	/* Chop trailing slash */
	if (folder[strlen(folder)-1] == '/')
		((char*) folder) [strlen(folder)-1] = 0;

	sprintf(buf, "/%s", dc120_folder_memory);
	if (strcmp(folder, buf)==0)
		/* From memory */
		retval = dc120_get_filenames(dd, 0, 0, list);

	sprintf(buf, "/%s", dc120_folder_card);
	if (strcmp(folder, buf)==0)
		/* From cf card */
		retval = dc120_get_filenames(dd, 1, 0, list);

	sprintf(buf, "/%s/ALBUM", dc120_folder_card);
	if (strncmp(folder, buf, strlen(dc120_folder_card)+7)==0)
		retval = dc120_get_filenames (dd, 1, folder[strlen (dc120_folder_card)+8] - '0', list);

	/* Save the order of the pics (wtf: no filename access on dc120???) */
	/* Use the filesystem helpers to maintain picture list */
	if (retval == GP_OK) {
		for (x=0; x<gp_list_count(list); x++) {
			gp_list_get_name (list, x, &name);
			gp_filesystem_append(dd->fs, folder, name);
		}
	}

	return (retval);
}

int camera_file_action (Camera *camera, int action, CameraFile *file, 
			const char *folder, const char *filename) 
{
	DC120Data *dd = camera->camlib_data;
	int picnum=0, album_num=-1, from_card=0;
	char buf[32];
	char *dot;

	picnum = gp_filesystem_number(dd->fs, folder, filename);

	if (picnum == GP_ERROR)
		return (GP_ERROR);
	
	sprintf(buf, "/%s", dc120_folder_memory);
	if (strcmp(folder, buf)==0) {
		from_card = 0;
		album_num = 0;
	}

	sprintf(buf, "/%s", dc120_folder_card);
	if (strcmp(folder, buf)==0) {
		from_card = 1;
		album_num = 0;
	}

	sprintf(buf, "/%s/ALBUM", dc120_folder_card);
	if (strncmp(folder, buf, strlen(dc120_folder_card)+7)==0) {
		from_card = 1; 
		album_num = folder[12] - '0';
	}

	if (album_num == -1)
		return (GP_ERROR);

	if (action == DC120_ACTION_PREVIEW) {
		dot = strrchr(filename, '.');
		if (dot)
			strcpy(&dot[1], "ppm");
	}

	if (file)
		gp_file_set_name (file, filename);

	return (dc120_file_action(dd, action, from_card, album_num, picnum+1, file));
}

int camera_file_get (Camera *camera, const char *folder, const char *filename, 
		     CameraFileType type, CameraFile *file) 
{
	switch (type) {
	case GP_FILE_TYPE_NORMAL:
		return (camera_file_action (camera, DC120_ACTION_IMAGE, file,
					    folder, filename));
	case GP_FILE_TYPE_PREVIEW:
		return (camera_file_action (camera, DC120_ACTION_PREVIEW, file,
					    folder, filename));
	default:
		return (GP_ERROR_NOT_SUPPORTED);
	}
}

int camera_file_delete (Camera *camera, const char *folder, 
			const char *filename) 
{
	DC120Data *dd = camera->camlib_data;
	int retval;

	retval = camera_file_action (camera, DC120_ACTION_DELETE, NULL, folder, 
				     filename);

	if (retval == GP_OK)
		gp_filesystem_delete(dd->fs, folder, filename);

	return (retval);
}

#if 0
int camera_capture (Camera *camera, int capture_type, CameraFilePath *path) {

	DC120Data *dd = camera->camlib_data;

	return (dc120_capture(dd, file));
}
#endif

int camera_summary (Camera *camera, CameraText *summary) 
{
/*	DC120Data *dd = camera->camlib_data; */

	strcpy(summary->text, _("No summary information yet"));

	return (GP_OK);
}

int camera_manual (Camera *camera, CameraText *manual) 
{
	strcpy (manual->text, 
		_("The Kodak DC120 camera uses the KDC file format "
		"for storing images. If you want to view the images you "
		"download from your camera, you will need to download "
		"the \"kdc2tiff\" program. "
		"It is available from http://kdc2tiff.sourceforge.net"));

	return (GP_OK);
}

int camera_about (Camera *camera, CameraText *about) 
{
	strcpy(about->text, 
		_("Kodak DC120 Camera Library\n"
		"Scott Fritzinger <scottf@gphoto.net>\n"
		"Camera Library for the Kodak DC120 camera.\n"
		"(by popular demand)."));

	return (GP_OK);
}
