#include <config.h>

#include "sq905.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <libgphoto2/gphoto2-library.h>
#include <libgphoto2/bayer.h>
#include <libgphoto2/gamma.h>

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

#undef  MAX
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))

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
		sq_reset (camera->port);
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
	unsigned int i, j, n;
	unsigned char name[1024];
	CameraFile *file;
	unsigned int w, h, c;
	unsigned char *buf, b;
	unsigned char header[1024]; /* Should be enough */
	unsigned int buf_len, read, id;
	unsigned char *buf_out, t[256];

	/* There is only a root folder. */
	if (strcmp (folder, "/")) return GP_OK;

	/*
	 * The protocol is super stupid. You need to download all
	 * pictures at once. Therefore, we add the pictures here and
	 * don't return a list. The filesystem is clever enough to 
	 * figure out what happened.
	 */
	n = sq_config_get_num_pic (camera->pl->data);
	id = gp_context_progress_start (context, n, _("Downloading..."));
	for (i = 0; i < n; i++) {
		snprintf (name, sizeof (name), "pict%02i.ppm", i + 1);
		gp_filesystem_append (fs, folder, name, context);
		
		w = sq_config_get_width  (camera->pl->data, n);
		h = sq_config_get_height (camera->pl->data, n);
		c = sq_config_get_comp   (camera->pl->data, n);

		buf_len = w * h / c;
		buf = malloc (buf_len);
		if (!buf) return GP_ERROR_NO_MEMORY;

		/* Download the data from the camera */
		read = 0;
		id = gp_context_progress_start (context, buf_len,
						_("Downloading..."));
		while (read < buf_len) {
			sq_read (camera->port, buf + read, MAX (0x8000,
								buf_len-read));
			read += MAX (0x8000, buf_len - read);
		}

		/* Store the file with raw data */
		gp_file_new (&file);
		gp_file_set_name (file, name);
		gp_file_set_type (file, GP_FILE_TYPE_RAW);
		gp_file_set_data_and_size (file, buf, buf_len);
		gp_filesystem_set_file_noop (fs, folder, file, context);
		gp_file_unref (file);

		/* Reverse the data, else the pic is upside down. */
		for (j = 0; j < buf_len / 2; j++) {
			b = buf[j];
			buf[j] = buf[buf_len - 1 - j];
			buf[buf_len - 1 - j] = b;
		}

		/* Decode the data and postprocess. */
		buf_out = malloc (w * h * 3);
		if (!buf_out) return GP_ERROR_NO_MEMORY;
		gp_bayer_decode (buf, w, h, buf_out, BAYER_TILE_BGGR);
		gp_gamma_fill_table (t, .65);
		gp_gamma_correct_single (t, buf_out, w * h);

		snprintf (header, sizeof (header), 
			"P6\n"
			"# CREATOR: gphoto2, SQ905 library\n"
			"%d %d\n"
			"255\n", w, h);
		gp_file_new (&file);
		gp_file_set_name (file, name);
		gp_file_set_type (file, GP_FILE_TYPE_NORMAL);
		gp_file_set_mime_type (file, GP_MIME_PPM);
		gp_file_append (file, header, strlen (header));
		gp_file_append (file, buf_out, w * h * 3);
		free (buf_out);
		gp_filesystem_set_file_noop (fs, folder, file, context);
		gp_file_unref (file);

		gp_context_progress_update (context, id, i + 1);
	}
	gp_context_progress_stop (context, id);
	sq_reset (camera->port);

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

	return GP_OK;
}
