#include <config.h>

#include "sq905.h"

#include <stdlib.h>
#include <string.h>

#include <libgphoto2/gphoto2-library.h>

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
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define _(String) (String)
#  define N_(String) (String)
#endif

struct _CameraPrivateLibrary {
	SQConfig data;
};

static struct {
	char *name;
	CameraDriverStatus status;
	unsigned short vendor;
	unsigned short product;
	char serial;
} models[] = {
        {"SQ chip camera", GP_DRIVER_STATUS_EXPERIMENTAL, 0x2770, 0x9120, 0},
        {"Argus DC-1510", GP_DRIVER_STATUS_PRODUCTION, 0x2770, 0x9120, 0},
        {"Gear to go", GP_DRIVER_STATUS_EXPERIMENTAL, 0x2770, 0x9120, 0},
        {NULL, 0, 0, 0, 0}
};

int
camera_id (CameraText *id)
{
	strcpy (id->text, "SQ chipset camera");

	return GP_OK;
}

int
camera_abilities (CameraAbilitiesList *list)
{
	int i;
	CameraAbilities a;

	for (i = 0; models[i].name; i++) {
		memset (&a, 0, sizeof (a));
		strcpy (a.model, models[i].name);
		a.status = models[i].status;
		a.port = GP_PORT_USB;
		a.speed[0] = 0;
		a.usb_vendor = models[i].vendor;
		a.usb_product= models[i].product;
		if (a.status == GP_DRIVER_STATUS_EXPERIMENTAL)
			a.operations = GP_OPERATION_NONE;
		else
			a.operations = GP_OPERATION_CAPTURE_IMAGE;
		a.folder_operations = GP_FOLDER_OPERATION_NONE;
		a.file_operations = GP_FILE_OPERATION_NONE;
		gp_abilities_list_append (list, a);
	}

	return GP_OK;
}

static int
camera_summary (Camera *camera, CameraText *summary, GPContext *context)
{
	return GP_OK;
}

static int
camera_exit (Camera *camera, GPContext *context)
{
	if (camera) {
		if (camera->pl) {
			free (camera->pl);
			camera->pl = NULL;
		}
	}

	return GP_OK;
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	Camera *camera = data;
	unsigned int n;

	n = sq_config_get_num_pic (camera->pl->data);
	gp_list_populate (list, "pict%02i.ppm", n);

	return GP_OK;
}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *user_data,
	       GPContext *context)
{
	Camera *camera = (Camera *) user_data;
	unsigned int w, h, c;
	int n;

	n = gp_filesystem_number (camera->fs, folder, filename, context);
	w = sq_config_get_width  (camera->pl->data, n);
	h = sq_config_get_height (camera->pl->data, n);
	c = sq_config_get_comp   (camera->pl->data, n);

	return GP_OK;
}

int
camera_init (Camera *camera, GPContext *context)
{
	GPPortSettings settings;
	int ret = GP_OK;

	/* First, set up all the function pointers */
	camera->functions->summary = camera_summary;
	camera->functions->exit    = camera_exit;

	ret = gp_port_get_settings (camera->port, &settings);
	if (ret < 0) return ret;
	ret = gp_port_set_settings (camera->port, settings);
	if (ret < 0) return ret;

	camera->pl = malloc (sizeof (CameraPrivateLibrary));
	if (!camera->pl) return GP_ERROR_NO_MEMORY;
	memset (camera->pl, 0, sizeof (CameraPrivateLibrary));

	/* Initialize the camera */
	ret = sq_init (camera->port, camera->pl->data);
	if (ret < 0) {
		free (camera->pl);
		return ret;
	}

	gp_filesystem_set_list_funcs (camera->fs, file_list_func, NULL, camera);
	gp_filesystem_set_file_funcs (camera->fs, get_file_func, NULL, camera);

	return GP_OK;
}
