/* Tenx tp6801 picframe access library
 *
 * Copyright (c) 2011 Hans de Goede <hdegoede@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the 
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#define _BSD_SOURCE
#include "config.h"

#include <string.h>
#include <stdlib.h>
#ifdef HAVE_LIBGD
#include <gd.h>
#endif

#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-result.h>
#include <gphoto2/gphoto2-port.h>
#include <gphoto2/gphoto2-setting.h>
#include "tp6801.h"

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

static const struct tp6801_devinfo tp6801_devinfo[] = {
	{ 0x0168, 0x3011 },
	{}
};

int
camera_id (CameraText *id)
{
	strcpy (id->text, "TP6801 USB picture frame");

	return GP_OK;
}


int
camera_abilities (CameraAbilitiesList *list)
{
	CameraAbilities a;
	int i;

	for (i = 0; tp6801_devinfo[i].vendor_id; i++) {
		memset (&a, 0, sizeof(a));
		snprintf(a.model, sizeof (a.model),
			"TP6801 USB picture frame");
		a.status = GP_DRIVER_STATUS_TESTING;
		a.port   = GP_PORT_USB_SCSI;
		a.speed[0] = 0;
		a.usb_vendor = tp6801_devinfo[i].vendor_id;
		a.usb_product= tp6801_devinfo[i].product_id;
		a.operations = GP_OPERATION_NONE;
		a.folder_operations = GP_FOLDER_OPERATION_PUT_FILE | 
				      GP_FOLDER_OPERATION_DELETE_ALL;
		a.file_operations   = GP_FILE_OPERATION_DELETE |
				      GP_FILE_OPERATION_RAW;
		gp_abilities_list_append (list, a);
	}

	return GP_OK;
}

static int
camera_summary (Camera *camera, CameraText *summary, GPContext *context)
{
	sprintf (summary->text,
		 _("Your USB picture frame has a TP6801 chipset\n"));
	return GP_OK;
}

static int
camera_manual (Camera *camera, CameraText *manual, GPContext *context)
{
	strcpy(manual->text,
	_(
	"TP6801 based picture frames come with a variety of resolutions.\n"
	"The gphoto driver for these devices allows you to download,\n"
	"upload and delete pictures from the picture frame."
	));

	return GP_OK;
}

static int
camera_about (Camera *camera, CameraText *about, GPContext *context)
{
	strcpy (about->text,
	_(
	"TP6801 USB picture frame driver\n"
	"Hans de Goede <hdegoede@redhat.com>\n"
	"This driver allows you to download, upload and delete pictures\n"
	"from the picture frame."
	));

	return GP_OK;
}

static int get_file_idx(Camera *camera, const char *folder,
	const char *filename)
{
	int idx, count, present;
	char *c;

	if (strcmp (folder, "/"))
		return GP_ERROR_DIRECTORY_NOT_FOUND;

	if (strlen (filename) != 12 ||
	    strncmp (filename, "pict", 4) ||
	    strcmp (filename + 8, ".png"))
		return GP_ERROR_FILE_NOT_FOUND;

	idx = strtoul(filename + 4, &c, 10);
	if (*c != '.')
		return GP_ERROR_FILE_NOT_FOUND;
	idx--;

	count = tp6801_max_filecount (camera);
	if (count < 0) return count;

	if (idx < 0 || idx >= count)
		return GP_ERROR_FILE_NOT_FOUND;

	present = tp6801_file_present (camera, idx);
	if (present < 0) return present;
	if (!present)
		return GP_ERROR_FILE_NOT_FOUND;

	return idx;
}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *data,
	       GPContext *context)
{
	Camera *camera = data;
	int idx, size;
#ifdef HAVE_LIBGD
	int ret;
	gdImagePtr im;
	void *gdpng;
#endif

	idx = get_file_idx(camera, folder, filename);
	if (idx < 0)
		return idx;

	if (type == GP_FILE_TYPE_RAW) {
		char *raw;

		CHECK (tp6801_read_raw_file (camera, idx, &raw))
		gp_file_set_mime_type (file, GP_MIME_RAW);
		gp_file_set_name (file, filename);
		gp_file_set_data_and_size (file, raw,
		                           tp6801_filesize (camera));

		return GP_OK;
	}

#ifdef HAVE_LIBGD
	if (type != GP_FILE_TYPE_NORMAL)
		return GP_ERROR_NOT_SUPPORTED;

	im = gdImageCreateTrueColor(camera->pl->width, camera->pl->height);
	if (im == NULL)
		return GP_ERROR_NO_MEMORY;

	ret = tp6801_read_file(camera, idx, im->tpixels);
	if (ret < 0) {
		gdImageDestroy (im);
		return ret;
	}

	gdpng = gdImagePngPtr(im, &size);
	gdImageDestroy (im);
	if (gdpng == NULL)
		return GP_ERROR_NO_MEMORY;

	ret = gp_file_set_mime_type (file, GP_MIME_PNG);
	if (ret < 0) { gdFree (gdpng); return ret; }

	ret = gp_file_set_name (file, filename); 
	if (ret < 0) { gdFree (gdpng); return ret; }

	ret = gp_file_append (file, gdpng, size);
	gdFree (gdpng);
	return ret;
#else
	gp_log(GP_LOG_ERROR,"tp6801", "GD decompression not supported - no libGD present during build");
	return GP_ERROR_NOT_SUPPORTED;
#endif
}

static int
put_file_func (CameraFilesystem *fs, const char *folder, const char *name, 
	CameraFileType type, CameraFile *file, void *data, GPContext *context)
{
#ifdef HAVE_LIBGD
	Camera *camera = data;
	char *filedata = NULL;
	int ret, in_width, in_height, in_x, in_y;
	double aspect_in, aspect_out;
	unsigned long filesize = 0;
	gdImagePtr im_out, im_in = NULL;

	if (strcmp (folder, "/"))
		return GP_ERROR_DIRECTORY_NOT_FOUND;

	CHECK (gp_file_get_data_and_size (file, (const char **)&filedata,
					  &filesize))

	/* Try loading the file using gd, starting with the most often
	   used types first */

	/* gdImageCreateFromJpegPtr is chatty on error, don't call it on
	   non JPEG files */
	if (filesize > 2 &&
	    (uint8_t)filedata[0] == 0xff && (uint8_t)filedata[1] == 0xd8)
		im_in = gdImageCreateFromJpegPtr(filesize, filedata);
	if (im_in == NULL)
		im_in = gdImageCreateFromPngPtr(filesize, filedata);
	if (im_in == NULL)
		im_in = gdImageCreateFromGifPtr(filesize, filedata);
	if (im_in == NULL)
		im_in = gdImageCreateFromWBMPPtr(filesize, filedata);
	if (im_in == NULL) {
		gp_log (GP_LOG_ERROR, "tp6801",
			"Unrecognized file format for file: %s%s",
			folder, name);
		return GP_ERROR_BAD_PARAMETERS;
	}

	im_out = gdImageCreateTrueColor(camera->pl->width, camera->pl->height);
	if (im_out == NULL) {
		gdImageDestroy (im_in);
		return GP_ERROR_NO_MEMORY;
	}

	/* Keep aspect */
	aspect_in  = (double)im_in->sx / im_in->sy;
	aspect_out = (double)im_out->sx / im_out->sy;
	if (aspect_in > aspect_out) {
		/* Reduce in width (crop left and right) */
		in_width = (im_in->sx / aspect_in) * aspect_out;
		in_x = (im_in->sx - in_width) / 2;
		in_height = im_in->sy;
		in_y = 0;
	} else {
		/* Reduce in height (crop top and bottom) */
		in_width = im_in->sx;
		in_x = 0;
		in_height = (im_in->sy * aspect_in) / aspect_out;
		in_y = (im_in->sy - in_height) / 2;
	}

	gdImageCopyResampled (im_out, im_in, 0, 0, in_x, in_y,
			      im_out->sx, im_out->sy,
			      in_width, in_height);

	if (im_in->sx != im_out->sx ||
	    im_in->sy != im_out->sy)
		gdImageSharpen(im_out, 100);

	ret = tp6801_write_file (camera, im_out->tpixels);
	if (ret >= 0) {
		/* Commit the changes to the device */
		ret = tp6801_commit(camera);
	}

	gdImageDestroy (im_in);
	gdImageDestroy (im_out);
	return ret;
#else
	gp_log(GP_LOG_ERROR,"tp6801", "GD compression not supported - no libGD present during build");
	return GP_ERROR_NOT_SUPPORTED;
#endif
}

static int
delete_file_func (CameraFilesystem *fs, const char *folder,
		  const char *filename, void *data, GPContext *context)
{
	Camera *camera = data;
	int idx;

	idx = get_file_idx(camera, folder, filename);
	if (idx < 0)
		return idx;

	CHECK (tp6801_delete_file(camera, idx))

	return tp6801_commit(camera);
}

static int
delete_all_func (CameraFilesystem *fs, const char *folder, void *data,
		 GPContext *context)
{
	Camera *camera = data;

	CHECK (tp6801_delete_all (camera))

	return tp6801_commit(camera);
}

static int
get_info_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileInfo *info, void *data, GPContext *context)
{
	memset (info, 0, sizeof(CameraFileInfo));
	/* FIXME: fill in some stuff? */
	return GP_OK;
}

static int
folder_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		  void *data, GPContext *context)
{
	if (strcmp(folder, "/"))
		return GP_ERROR_DIRECTORY_NOT_FOUND;

	/* Subfolders not supported */
	return GP_OK;
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	Camera *camera = data; 
	int i, count, present;
	char buf[16];

	count = tp6801_max_filecount (camera);
	if (count < 0) return count;

	for (i = 0; i < count; i++) {
		present = tp6801_file_present (camera, i);
		if (present < 0) return present;
		if (present) {
			snprintf(buf, sizeof(buf), "pict%04d.png", i + 1);
			CHECK (gp_list_append (list, buf, NULL))
		}
	}

	return GP_OK;
}

static int
storage_info_func (CameraFilesystem *fs,
                CameraStorageInformation **sinfos,
                int *nrofsinfos,
                void *data, GPContext *context)
{
	Camera *camera = (Camera*)data;
	CameraStorageInformation *sinfo;
	int free, imagesize;

	free = tp6801_get_free_mem_size (camera);
	if (free < 0) return free;

	sinfo = malloc(sizeof(CameraStorageInformation));
	if (!sinfo) return GP_ERROR_NO_MEMORY;

	*sinfos = sinfo;
	*nrofsinfos = 1;

	sinfo->fields  = GP_STORAGEINFO_BASE;
	strcpy(sinfo->basedir, "/");

	sinfo->fields |= GP_STORAGEINFO_ACCESS;
	sinfo->access  = GP_STORAGEINFO_AC_READWRITE;
	sinfo->fields |= GP_STORAGEINFO_STORAGETYPE;
	sinfo->type    = GP_STORAGEINFO_ST_FIXED_RAM;
	sinfo->fields |= GP_STORAGEINFO_FILESYSTEMTYPE;
	sinfo->fstype  = GP_STORAGEINFO_FST_GENERICFLAT;
	sinfo->fields |= GP_STORAGEINFO_MAXCAPACITY;
	sinfo->capacitykbytes = tp6801_get_mem_size (camera) / 1024;
	sinfo->fields |= GP_STORAGEINFO_FREESPACEKBYTES;
	sinfo->freekbytes = free / 1024;

	imagesize = tp6801_filesize (camera);
	if (imagesize) {
        	sinfo->fields |= GP_STORAGEINFO_FREESPACEIMAGES;
        	sinfo->freeimages = free / imagesize;
	}

	return GP_OK;
}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.folder_list_func = folder_list_func,
	.get_info_func = get_info_func,
	.get_file_func = get_file_func,
	.del_file_func = delete_file_func,
	.put_file_func = put_file_func,
	.delete_all_func = delete_all_func,
	.storage_info_func = storage_info_func
};

static int
camera_get_config (Camera *camera, CameraWidget **window, GPContext *context)
{
	CameraWidget *child;

	GP_DEBUG ("*** camera_get_config");

	gp_widget_new (GP_WIDGET_WINDOW,
			_("Picture Frame Configuration"), window);

	gp_widget_new (GP_WIDGET_TOGGLE,
			_("Synchronize frame data and time with PC"), &child);
	gp_widget_set_value (child, &camera->pl->syncdatetime);
	gp_widget_append (*window, child);

	return GP_OK;
}

static int
camera_set_config (Camera *camera, CameraWidget *window, GPContext *context)
{
	CameraWidget *child;
	int ret;

	GP_DEBUG ("*** camera_set_config");

	ret = gp_widget_get_child_by_label (window,
			_("Synchronize frame data and time with PC"), &child);
	if (ret == GP_OK)
		gp_widget_get_value (child, &camera->pl->syncdatetime);

	return GP_OK;
}

static int
camera_exit (Camera *camera, GPContext *context) 
{
	char buf[2];

	if (camera->pl != NULL) {
		buf[0] = '0' + camera->pl->syncdatetime;
		buf[1] = 0;
		gp_setting_set("tp6801", "syncdatetime", buf);
		tp6801_close (camera);
		free (camera->pl);
		camera->pl = NULL;
	}
	return GP_OK;
}

int
camera_init (Camera *camera, GPContext *context) 
{
    	CameraAbilities a;
    	int ret;
    	char *dump, buf[256];

	/* First, set up all the function pointers */
	camera->functions->exit    = camera_exit;
	camera->functions->summary = camera_summary;
	camera->functions->manual  = camera_manual;
	camera->functions->about   = camera_about;
	camera->functions->get_config = camera_get_config;
	camera->functions->set_config = camera_set_config;

	/* Tell the CameraFilesystem where to get lists from */
	gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera);

	camera->pl = calloc (1, sizeof(CameraPrivateLibrary));
	if (!camera->pl) return GP_ERROR_NO_MEMORY;

	ret = gp_setting_get("tp6801", "syncdatetime", buf);
	if (ret == GP_OK)
		camera->pl->syncdatetime = buf[0] == '1';
	else
		camera->pl->syncdatetime = 1;

	CHECK (gp_camera_get_abilities(camera, &a))

	dump = getenv("GP_TP6801_DUMP");
	if (dump)
		ret = tp6801_open_dump (camera, dump);
	else
		ret = tp6801_open_device (camera);

	if (ret != GP_OK) {
		camera_exit (camera, context);
		return ret;
	}

	if (camera->pl->syncdatetime) {
#ifdef HAVE_LOCALTIME_R
		struct tm tm;
#endif
		struct tm *xtm;
		time_t t;

		t = time (NULL);
#ifdef HAVE_LOCALTIME_R
		if ((xtm = localtime_r (&t , &tm))) {
#else
		if ((xtm = localtime (&t))) {
#endif
			ret = tp6801_set_time_and_date (camera, xtm);
			if (ret != GP_OK) {
				camera_exit (camera, context);
				return ret;
			}
		}
	}

	return GP_OK;
}
