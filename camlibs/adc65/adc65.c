/* ADC-65(s) camera driver
 * Released under the GPL version 2   
 * 
 * Copyright 2001
 * Benjamin Moos
 * <benjamin@psnw.com>
 * http://www.psnw.com/~smokeserpent/code/
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gphoto2.h>
#include <gphoto2-abilities-list.h>

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

#include "adc65.h"

int camera_id (CameraText *id) {
	strcpy(id->text, "adc65");
	return (GP_OK);
}

int camera_abilities (CameraAbilitiesList *list) {
	CameraAbilities a;

	memset(&a, 0, sizeof(a));
	/* Fill in the appropriate flags/data */
	strcpy(a.model, "Achiever Digital:Adc65");
	a.port      = GP_PORT_SERIAL;
	a.speed[0]  = 115200;
	a.speed[1]  = 230400;
	a.speed[2]  = 0;
	a.operations        = GP_OPERATION_NONE;
	a.file_operations   = GP_FILE_OPERATION_NONE;
	a.folder_operations = GP_FOLDER_OPERATION_NONE;
	return gp_abilities_list_append(list, a);
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	return gp_list_populate (list, "adc65%02i.ppm", adc65_file_count((Camera*)data));
}

static int
get_file_func (	CameraFilesystem *fs, const char *folder,
		const char *filename, CameraFileType type,
		CameraFile *file, void *user_data, GPContext *context)
{
	Camera *camera = user_data;
        int size, num;
	char *data;

        gp_file_set_name (file, filename);
        gp_file_set_mime_type (file, GP_MIME_PPM);
        num = gp_filesystem_number (fs, folder, filename, context);
	if (num < 0)
		return num;
        data = adc65_read_picture (camera, num, &size);
        if (!data)
		return GP_ERROR;
	return gp_file_append (file,data,size);
}

static int
camera_about (Camera *camera, CameraText *about, GPContext *context) {
        strcpy (about->text, _("Adc65\nBenjamin Moos <benjamin@psnw.com>"));
        return GP_OK;
}

int
camera_init(Camera *camera, GPContext *context) {
	int ret;
	gp_port_settings settings;

	camera->functions->about        = camera_about;
	gp_filesystem_set_list_funcs (camera->fs, file_list_func, NULL, camera);
	gp_filesystem_set_file_funcs (camera->fs, get_file_func, NULL, camera);

	ret = gp_port_set_timeout (camera->port, 5000);
	if (ret < GP_OK)
		return ret;

	ret = gp_port_get_settings (camera->port, &settings);
	if (ret < GP_OK)
		return ret;
	settings.serial.bits    = 8;
	settings.serial.parity  = 0;
	settings.serial.stopbits= 1;
	ret = gp_port_set_settings (camera->port, settings);
	if (ret < GP_OK)
		return ret;

	return adc65_ping(camera);
}
