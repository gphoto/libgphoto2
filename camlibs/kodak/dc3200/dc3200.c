/****************************************************
 * kodak dc3200 digital camera driver library       *
 * for gphoto2                                      *
 *                                                  *
 * author: donn morrison - dmorriso@gulf.uvic.ca    *
 * date: dec 2000 - feb 2001                        *
 * license: gpl                                     *
 * version: 1.5                                     *
 *                                                  *
 ****************************************************/

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

#include "dc3200.h"
#include "library.h"

int camera_id (CameraText *id) 
{
	strcpy(id->text, "kodak-dc3200");

	return (GP_OK);
}

int camera_abilities (CameraAbilitiesList *list) 
{
	CameraAbilities *a;

	gp_abilities_new(&a);

	strcpy(a->model, "Kodak DC3200");
	a->port     = GP_PORT_SERIAL;
	a->speed[0] = 9600;
	a->speed[1] = 19200;
	a->speed[2] = 38400;
	a->speed[3] = 57600;
	a->speed[4] = 115200;
	a->speed[5] = 0;
	a->operations        = GP_OPERATION_NONE;
	a->file_operations   = GP_FILE_OPERATION_PREVIEW;
	a->folder_operations = GP_FOLDER_OPERATION_NONE;

	gp_abilities_list_append(list, a);

	return (GP_OK);
}

int camera_init (Camera *camera) 
{
	DC3200Data *dd;
        int ret;

	if (!camera)
		return (GP_ERROR);

	dd = (DC3200Data*)malloc(sizeof(DC3200Data));
	if (!dd)
		return (GP_ERROR);

	/* First, set up all the function pointers */
	camera->functions->id 			= camera_id;
	camera->functions->abilities 		= camera_abilities;
	camera->functions->init 		= camera_init;
	camera->functions->exit 		= camera_exit;
	camera->functions->folder_list_folders  = camera_folder_list_folders;
	camera->functions->folder_list_files	= camera_folder_list_files;
	camera->functions->file_get 		= camera_file_get;
	camera->functions->file_get_preview 	= camera_file_get_preview;
	camera->functions->summary		= camera_summary;
	camera->functions->manual 		= camera_manual;
	camera->functions->about 		= camera_about;
	camera->functions->result_as_string 	= camera_result_as_string;
	
        if ((ret = gp_port_new(&(dd->dev), GP_PORT_SERIAL)) < 0) {
		free(dd);
		return (ret);
	}

	camera->camlib_data = dd;

	/* initialize the camera */
	if (init(camera) == GP_ERROR) {
		gp_port_close(dd->dev);
		gp_port_free(dd->dev);
		free(dd);
		return (GP_ERROR);
	}

	/* Everything went OK. Save the data*/
	gp_filesystem_new(&dd->fs);

	return (dc3200_keep_alive(dd));
}

int init(Camera *camera)
{
	gp_port_settings settings;
	DC3200Data *dd = camera->camlib_data;

	strcpy(settings.serial.port, camera->port->path);
	settings.serial.speed    = 9600;
	settings.serial.bits     = 8;
	settings.serial.parity   = 0;
	settings.serial.stopbits = 1;

	/* set up the port */
	if (gp_port_settings_set(dd->dev, settings) == GP_ERROR)
		return GP_ERROR;

	/* open the port */
	if (gp_port_open(dd->dev) == GP_ERROR)
		return GP_ERROR;

	gp_port_timeout_set(dd->dev, TIMEOUT);

	if (dc3200_set_speed(dd, camera->port->speed) == GP_ERROR)
		return GP_ERROR;

	settings.serial.speed = camera->port->speed;

	/* set the new speed */
	if (gp_port_settings_set(dd->dev, settings) == GP_ERROR)
		return GP_ERROR;

	/* Wait for it to update */
	sleep(1);

	/* Try to talk after speed change */
	if (dc3200_keep_alive(dd) == GP_ERROR)
		return GP_ERROR;

	/* setup the camera */
	if (dc3200_setup(dd) == GP_ERROR)
		return GP_ERROR;		

	return GP_OK;
}

int camera_exit (Camera *camera)
{
	DC3200Data	*dd = camera->camlib_data;
	if (!dd)
		return (GP_OK);

	gp_filesystem_free(dd->fs);

	if (dd->dev) {
		if (gp_port_close(dd->dev) == GP_ERROR)
			{ /* camera did a bad, bad thing */ }
		gp_port_free(dd->dev);
	}

	return (GP_OK);
}

int check_last_use(Camera *camera)
{
	time_t t;
	DC3200Data *dd = camera->camlib_data;
	
	time(&t);
	
	if(t - dd->last > 9) {
		/* we have to re-init the camera */
		printf(_("camera inactive for > 9 seconds, re-initing.\n"));
		return init(camera);
	}

	return GP_OK;
}

int camera_folder_list_folders (Camera *camera, const char *folder, 
				CameraList *list)
{
	DC3200Data	*dd = camera->camlib_data;
	u_char		*data = NULL;
	long		data_len = 0;
	u_char		*ptr_data_buff;
	char		filename[13], *ptr;
	int		res, i;

	if(check_last_use(camera) == GP_ERROR)
		return GP_ERROR;

	/* get file list data */
	res = dc3200_get_data (dd, &data, &data_len, CMD_LIST_FILES, folder, 
			       NULL);
	if (res == GP_ERROR)
		return GP_ERROR;

	/* check the data length, each record is 20 bytes */
	if(data_len%20 != 0 || data_len < 1) {
		/* there is a problem */
		return GP_ERROR;
	}
	
	if (data == NULL)
		return GP_ERROR;

	/* add directories to the list */
	ptr_data_buff = data;
	i = 0;

	while(i < data_len) {
		//dump_buffer(ptr_data_buff, 20, "list", 20);
	
		/* directories have 0x10 in their attribute */
		if(!(ptr_data_buff[11] & 0x10)) {
			ptr_data_buff += 20;
			i += 20;
			continue;
		}
		
		/* skip directories starting with . */
		if(ptr_data_buff[0] == '.') {
			ptr_data_buff += 20;
			i += 20;
			continue;
		}
		
		/* copy the filename */
		strncpy(filename, ptr_data_buff, sizeof(filename));

		/* chop off the trailing spaces and null terminate */
		ptr = strchr(filename, 0x20);
		if(ptr) ptr[0] = 0;

		/* in case of long directory */
		filename[12] = 0;
		
		/* append dir to the list */
		gp_list_append(list, filename, NULL);
		
		ptr_data_buff += 20;
		i += 20;
	}

	free (data);
	return (dc3200_keep_alive (dd));
}

int camera_folder_list_files (Camera *camera, const char *folder, 
			      CameraList *list)
{
	DC3200Data	*dd = camera->camlib_data;
	u_char		*data = NULL;
	long		data_len = 0;
	u_char		*ptr_data_buff;
	char		filename[13];
	int		res, i;

	if(check_last_use(camera) == GP_ERROR)
		return GP_ERROR;

	/* get file list data */
	res = dc3200_get_data (dd, &data, &data_len, CMD_LIST_FILES, folder, 
			       NULL);
	if (res == GP_ERROR)
		return GP_ERROR;

	/* check the data length */
	if(data_len%20 != 0 || data_len < 1) {
		/* there is a problem */
		return GP_ERROR;
	}

	if(data == NULL)
		return GP_ERROR;

	/* add files to the list */
	ptr_data_buff = data;
	i = 0;
	
	while(i < data_len) {
		/* files don't have 0x10 in their attribute */
		if(ptr_data_buff[11] & 0x10) {
			ptr_data_buff += 20;
			i += 20;
			continue;
		}
		
		/* copy the first 8 bytes of filename */
		strncpy(filename, ptr_data_buff, 8);
		filename[8] = 0;
		/* add dot */
		strcat(filename, ".");
		/* copy extension, last 3 bytes*/
		strncat(filename, ptr_data_buff+8, 3);
		
		if(!strstr(filename, ".JPG") && !strstr(filename, ".jpg")) {
			ptr_data_buff += 20;
			i += 20;
			continue;
		}

		/* append file to the list */
		gp_list_append(list, filename, NULL);
		
		ptr_data_buff += 20;
		i += 20;
	}

	free(data);
	return (dc3200_keep_alive(dd));
}

int camera_file_get (Camera *camera, const char *folder, const char *filename, 
		     CameraFile *file)
{
	DC3200Data	*dd = camera->camlib_data;
	u_char		*data = NULL;
	long		data_len = 0;
	int		res;

	if(check_last_use(camera) == GP_ERROR)
		return GP_ERROR;

	res = dc3200_get_data (dd, &data, &data_len, CMD_GET_FILE, folder, 
			       filename);
	if (res == GP_ERROR)
		return (GP_ERROR);

	if (data == NULL || data_len < 1)
		return (GP_ERROR);

	gp_file_append (file, data, data_len);

	free(data);
	return (dc3200_keep_alive(dd));
}

int camera_file_get_preview (Camera *camera, const char *folder, 
			     const char *filename, CameraFile *file)
{
	DC3200Data	*dd = camera->camlib_data;
	u_char		*data = NULL;
	long		data_len = 0;
	int		res;

	if(check_last_use(camera) == GP_ERROR)
		return GP_ERROR;

	res = dc3200_get_data (dd, &data, &data_len, CMD_GET_PREVIEW, folder, 
			       filename);

	if (res == GP_ERROR)
		return (GP_ERROR);
		
	if(data == NULL || data_len < 1)
		return GP_ERROR;

	gp_file_append(file, data, data_len);

	free(data);
	return (dc3200_keep_alive(dd));
}

int camera_summary (Camera *camera, CameraText *summary)
{
	strcpy(summary->text, _("No summary information yet"));
	return (GP_OK);
}

int camera_manual (Camera *camera, CameraText *manual)
{
	strcpy (manual->text, 
		_("Known problems:\n"
		"\n"
		"1. If the Kodak DC3200 does not receive a command at least "
		"every 10 seconds, it will time out, and will have to be "
		"re-initialized. If you notice the camera does not respond, "
		"simply re-select the camera. This will cause it to "
		"reinitialize.\n"
		"\n"
		"2. If you cancel a picture transfer, the driver will be left "
		"in an unknown state, and will most likely need to be "
		"reinitialized."));
	return (GP_OK);
}

int camera_about (Camera *camera, CameraText *about)
{
	strcpy	(about->text, 
		_("Kodak DC3200 Driver\n"
		"Donn Morrison <dmorriso@gulf.uvic.ca>\n"
		"\n"
		"Questions and comments appreciated."));
	return (GP_OK);
}

char* camera_result_as_string (Camera *camera, int result)
{
	if (result >= 0) return (_("This is not an error..."));
	if (-result < 100) return gp_result_as_string (result);
	return (_("This is a dc3200 specific error."));
}
