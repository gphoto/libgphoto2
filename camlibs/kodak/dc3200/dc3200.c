/****************************************************
 * kodak dc3200 digital camera driver library       *
 * for gphoto2                                      *
 *                                                  *
 * author: donn morrison - dmorriso@gulf.uvic.ca    *
 * date: dec 2000 - jan 2002                        *
 * license: gpl                                     *
 * version: 1.6                                     *
 *                                                  *
 ****************************************************/

#include "config.h"
#include "dc3200.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <gphoto2/gphoto2-library.h>

#include "library.h"

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (GETTEXT_PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define _(String) (String)
#  define N_(String) (String)
#endif

/*
 * FIXME: Use properly sized integer types. The dc3200 code used lots
 *        of u_char, u_long, u_int, which may be wrong on non-32bit systems.
 */

#define CONTEXT_EXISTS _("There is currently an operation in progress. \
This camera only supports one operation \
at a time. Please wait until the current \
operation has finished.")

int camera_id (CameraText *id) 
{
	strcpy(id->text, "kodak-dc3200");

	return (GP_OK);
}

int camera_abilities (CameraAbilitiesList *list) 
{
	CameraAbilities a;

	memset (&a, 0, sizeof(a));
	strcpy(a.model, "Kodak:DC3200");
	a.port     = GP_PORT_SERIAL;
	a.speed[0] = 9600;
	a.speed[1] = 19200;
	a.speed[2] = 38400;
	a.speed[3] = 57600;
	a.speed[4] = 115200;
	a.speed[5] = 0;
	a.operations        = GP_OPERATION_NONE;
	a.file_operations   = GP_FILE_OPERATION_PREVIEW;
	a.folder_operations = GP_FOLDER_OPERATION_NONE;

	gp_abilities_list_append(list, a);

	return (GP_OK);
}

static
int init(Camera *camera)
{
	GPPortSettings settings;
	int ret, selected_speed;

	ret = gp_port_get_settings (camera->port, &settings);
	if (ret < 0)
		return (ret);

	/* Remember the selected speed 0 == fastest */
	selected_speed = settings.serial.speed == 0 ? 115200 : settings.serial.speed;

	settings.serial.speed    = 9600;
	settings.serial.bits     = 8;
	settings.serial.parity   = 0;
	settings.serial.stopbits = 1;

	ret = gp_port_set_settings (camera->port, settings);
	if (ret < 0)
		return (ret);

	gp_port_set_timeout (camera->port, TIMEOUT);

	if (dc3200_set_speed (camera, selected_speed) == GP_ERROR)
		return GP_ERROR;

	/* Set the new speed */
	settings.serial.speed = selected_speed;
	ret = gp_port_set_settings (camera->port, settings);
	if (ret < 0)
		return (ret);

	/* Wait for it to update */
	sleep(1);

	/* Try to talk after speed change */
	if (dc3200_keep_alive(camera) == GP_ERROR)
		return GP_ERROR;

	/* setup the camera */
	if (dc3200_setup(camera) == GP_ERROR)
		return GP_ERROR;		

	return GP_OK;
}

static int camera_exit (Camera *camera, GPContext *context)
{
	if (camera->pl) {
		if(camera->pl->context) {
			gp_context_error(context, CONTEXT_EXISTS);
			return GP_ERROR;
		}
		free (camera->pl);
		camera->pl = NULL;
	}

	return GP_OK;
}

int check_last_use(Camera *camera)
{
	time_t t;

	time(&t);
	
	if(t - camera->pl->last > 9) {
		/* we have to re-init the camera */
		printf(_("camera inactive for > 9 seconds, re-initing.\n"));
		return init(camera);
	}

	return GP_OK;
}

static int folder_list_func (CameraFilesystem *fs, const char *folder,
			     CameraList *list, void *user_data,
			     GPContext *context)
{
	Camera 		*camera = user_data;
	unsigned char	*data = NULL;
	long unsigned 	data_len = 0;
	unsigned char	*ptr_data_buff;
	char		filename[13], *ptr;
	int		res, i;

	if(camera->pl->context)
	{
		gp_context_error(context, CONTEXT_EXISTS);
		return GP_ERROR;
	}

	if(check_last_use(camera) == GP_ERROR)
	{
		return GP_ERROR;
	}

	/* get file list data */
	res = dc3200_get_data (camera, &data, &data_len, CMD_LIST_FILES, folder,
			       NULL);
	if (res == GP_ERROR)
	{
		return GP_ERROR;
	}

	/* check the data length, each record is 20 bytes */
	if(data_len%20 != 0 || data_len < 1)
	{
		return GP_ERROR;
	}
	
	if (data == NULL)
	{
		return GP_ERROR;
	}

	/* add directories to the list */
	ptr_data_buff = data;
	i = 0;

	while(i < data_len) {
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
		strncpy(filename, (char *)ptr_data_buff, sizeof(filename));

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
	return (dc3200_keep_alive (camera));
}

static int file_list_func (CameraFilesystem *fs, const char *folder,
			   CameraList *list, void *user_data,
			   GPContext *context)
{
	Camera		*camera = user_data;
	unsigned char	*data = NULL;
	long unsigned	data_len = 0;
	unsigned char	*ptr_data_buff;
	char		filename[13];
	int		res, i;

	if(camera->pl->context)
	{
		gp_context_error(context, CONTEXT_EXISTS);
		return GP_ERROR;
	}

	if(check_last_use(camera) == GP_ERROR)
	{
		return GP_ERROR;
	}
	
	/* get file list data */
	res = dc3200_get_data (camera, &data, &data_len, CMD_LIST_FILES, folder,
			       NULL);
	if (res == GP_ERROR)
	{
		return GP_ERROR;
	}
	
	/* check the data length */
	if(data_len%20 != 0 || data_len < 1)
	{
		return GP_ERROR;
	}

	if(data == NULL)
	{
		return GP_ERROR;
	}

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
		strncpy(filename, (char *)ptr_data_buff, 8);
		filename[8] = 0;
		/* add dot */
		strcat(filename, ".");
		/* copy extension, last 3 bytes*/
		strncat(filename, (char *)ptr_data_buff+8, 3);
		
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
	return (dc3200_keep_alive(camera));
}

static int get_file_func (CameraFilesystem *fs, const char *folder,
			  const char *filename, CameraFileType type,
			  CameraFile *file, void *user_data, 
			  GPContext *context)
{
	Camera		*camera = user_data;
	unsigned char	*data = NULL;
	long unsigned	data_len = 0;
	int		res;

	if(camera->pl->context)
	{
		gp_context_error(context, CONTEXT_EXISTS);
		return GP_ERROR;
	}

	camera->pl->context = context;

	if(check_last_use(camera) == GP_ERROR)
	{
		camera->pl->context = NULL;
		return GP_ERROR;
	}

	switch (type) {
	case GP_FILE_TYPE_PREVIEW:
		res = dc3200_get_data (camera, &data, &data_len,
				       CMD_GET_PREVIEW, folder, filename);
		break;
	case GP_FILE_TYPE_NORMAL:
		res = dc3200_get_data (camera, &data, &data_len, 
				       CMD_GET_FILE, folder, filename);
		break;
	default:
		camera->pl->context = NULL;
		return (GP_ERROR_NOT_SUPPORTED);
	}
	if (res < 0)
	{
		camera->pl->context = NULL;
		return (res);
	}

	if (data == NULL || data_len < 1)
	{
		camera->pl->context = NULL;
		return GP_ERROR;
	}

	gp_file_append (file, (char *)data, data_len);

	free(data);
	camera->pl->context = NULL;
	return (dc3200_keep_alive(camera));
}

static int
get_info_func (CameraFilesystem *fs, const char *folder,
	       const char *filename, CameraFileInfo *info, void *user_data,
	       GPContext *context)
{
	Camera		*camera = user_data;
	unsigned char	*data = NULL;
	long unsigned	data_len = 0;
	int		res;
	char		file[1024];

	if(camera->pl->context)
	{
		gp_context_error(context, CONTEXT_EXISTS);
		return GP_ERROR;
	}

	if(check_last_use(camera) == GP_ERROR)
	{
		return GP_ERROR;
	}

	if(!folder)
	{
		return GP_ERROR;
	}
	
	strcpy(file, folder);
	if(folder[strlen(folder)-1] != '\\' || folder[strlen(folder)-1] != '/')
		strcat(file, "\\");
	strcat(file, filename);	

	/* get file list data */
	res = dc3200_get_data (camera, &data, &data_len, CMD_LIST_FILES, file,
			       NULL);
	if (res == GP_ERROR)
	{
		return GP_ERROR;
	}

	/* check the data length */
	if(data_len%20 != 0 || data_len < 1)
	{
		/* there is a problem */
		return GP_ERROR;
	}

	if(data == NULL)
	{
		return GP_ERROR;
	}

	/* get the file length && type and stuff */
	info->file.fields = GP_FILE_INFO_SIZE | GP_FILE_INFO_TYPE;
	info->file.size = bytes_to_l(data[19], data[18], data[17], data[16]);
	strcpy (info->file.type, GP_MIME_JPEG);
	
	info->preview.fields = GP_FILE_INFO_TYPE;
	strcpy (info->preview.type, GP_MIME_JPEG);

	free(data);
	return (dc3200_keep_alive(camera));
}

static int camera_manual (Camera *camera, CameraText *manual, GPContext *context)
{
	strcpy (manual->text, 
		_("Known problems:\n"
		"\n"
		"1. If the Kodak DC3200 does not receive a command at least "
		"every 10 seconds, it will time out, and will have to be "
		"re-initialized. If you notice the camera does not respond, "
		"simply re-select the camera. This will cause it to "
		"reinitialize."));
	return (GP_OK);
}

static int camera_about (Camera *camera, CameraText *about, GPContext *context)
{
	strcpy	(about->text, 
		_("Kodak DC3200 Driver\n"
		"Donn Morrison <dmorriso@gulf.uvic.ca>\n"
		"\n"
		"Questions and comments appreciated."));
	return (GP_OK);
}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.get_file_func = get_file_func,
	.get_info_func = get_info_func,
	.folder_list_func = folder_list_func
};

int camera_init (Camera *camera, GPContext *context) 
{
	int ret;

	camera->pl = malloc (sizeof (CameraPrivateLibrary));
	if (!camera->pl)
		return (GP_ERROR_NO_MEMORY);

        /* First, set up all the function pointers */
        camera->functions->exit                 = camera_exit;
        camera->functions->manual               = camera_manual;
        camera->functions->about                = camera_about;

	/* Set up the CameraFilesystem */
	gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera);

        /* initialize the camera */
	ret = init (camera);
	if (ret < 0) {
		free (camera->pl);
		camera->pl = NULL;
                return (ret);
        }

        ret = dc3200_keep_alive (camera);
	if (ret < 0) {
		free (camera->pl);
		camera->pl = NULL;
		return (ret);
	}
	camera->pl->context = NULL;
	return (GP_OK);
}

