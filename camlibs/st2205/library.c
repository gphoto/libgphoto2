/* Sitronix st2205 picframe access library
 *
 * Copyright (c) 2010 Hans de Goede <hdegoede@redhat.com>
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
#if defined(HAVE_ICONV) && defined(HAVE_LANGINFO_H)
# include <langinfo.h>
#endif
#ifdef HAVE_LIBGD
# include <gd.h>
#endif

#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-result.h>
#include <gphoto2/gphoto2-port.h>
#include <gphoto2/gphoto2-setting.h>
#include "st2205.h"

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

int
camera_id (CameraText *id)
{
	strcpy (id->text, "ST2205 USB picture frame");

	return GP_OK;
}

int
camera_abilities (CameraAbilitiesList *list)
{
	CameraAbilities a;

	memset (&a, 0, sizeof(a));
	strcpy (a.model, "ST2205 USB picture frame");
	a.status = GP_DRIVER_STATUS_TESTING;
	a.port   = GP_PORT_USB_DISK_DIRECT;
	a.speed[0] = 0;
	a.usb_vendor = 0x1403;
	a.usb_product= 0x0001;
	a.operations = GP_OPERATION_NONE;
	a.folder_operations = GP_FOLDER_OPERATION_PUT_FILE | 
			      GP_FOLDER_OPERATION_DELETE_ALL;
	a.file_operations   = GP_FILE_OPERATION_DELETE | GP_FILE_OPERATION_RAW;
	return gp_abilities_list_append (list, a);
}

static int
camera_summary (Camera *camera, CameraText *summary, GPContext *context)
{
	sprintf (summary->text,
		 _("Your USB picture frame has a ST2205 chipset\n"));
	return GP_OK;
}

static int
camera_manual (Camera *camera, CameraText *manual, GPContext *context)
{
	strcpy(manual->text,
	_(
	"ST2205 based picture frames come with a variety of resolutions.\n"
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
	"ST2205 USB picture frame driver\n"
	"Hans de Goede <hdegoede@redhat.com>\n"
	"This driver allows you to download, upload and delete pictures\n"
	"from the picture frame."
	));

	return GP_OK;
}

static int get_file_idx(CameraPrivateLibrary *pl, const char *folder,
	const char *filename)
{
	int i;

	if (strcmp (folder, "/"))
		return GP_ERROR_DIRECTORY_NOT_FOUND;

	for (i = 0; i < ST2205_MAX_NO_FILES; i++) {
		if (!strcmp (filename, pl->filenames[i]))
			break;
	}

	if (i == ST2205_MAX_NO_FILES)
		return GP_ERROR_FILE_NOT_FOUND;

	return i;
}

#ifdef HAVE_LIBGD
static void
rotate90 (gdImagePtr src, gdImagePtr dest)
{
	int x, y;

	for (y = 0; y < dest->sy; y++)
		for (x = 0; x < dest->sx; x++)
			dest->tpixels[y][x] =
				src->tpixels[src->sy - x - 1][y];
}

static void
rotate270 (gdImagePtr src, gdImagePtr dest)
{
	int x, y;

	for (y = 0; y < dest->sy; y++)
		for (x = 0; x < dest->sx; x++)
			dest->tpixels[y][x] =
				src->tpixels[x][src->sx - y - 1];
}

static int
needs_rotation (Camera *camera)
{
	int display_orientation, user_orientation = camera->pl->orientation;

	if (camera->pl->width > camera->pl->height)
		display_orientation = ORIENTATION_LANDSCAPE;
	else
		display_orientation = ORIENTATION_PORTRAIT;

	if (user_orientation == ORIENTATION_AUTO) {
		if (camera->pl->width == 240 && camera->pl->height == 320)
			user_orientation = ORIENTATION_LANDSCAPE;
		else
			user_orientation = display_orientation;
	}

	return display_orientation != user_orientation;
}
#endif

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *data,
	       GPContext *context)
{
	Camera *camera = data;
	int idx, size;
#ifdef HAVE_LIBGD
	int ret;
	gdImagePtr im, rotated;
	void *gdpng;
#endif

	idx = get_file_idx(camera->pl, folder, filename);
	if (idx < 0)
		return idx;

	if (type == GP_FILE_TYPE_RAW) {
		unsigned char *raw;

		size = st2205_read_raw_file (camera, idx, &raw);
		if (size < 0) return size;

		gp_file_set_mime_type (file, GP_MIME_RAW);
		gp_file_set_name (file, filename);
		gp_file_set_data_and_size (file, (char *)raw, size);

		return GP_OK;
	}

#ifdef HAVE_LIBGD
	if (type != GP_FILE_TYPE_NORMAL)
		return GP_ERROR_NOT_SUPPORTED;

	im = gdImageCreateTrueColor(camera->pl->width, camera->pl->height);
	if (im == NULL)
		return GP_ERROR_NO_MEMORY;

	ret = st2205_read_file(camera, idx, im->tpixels);
	if (ret < 0) {
		gdImageDestroy (im);
		return ret;
	}

	if (needs_rotation (camera)) {
		rotated = gdImageCreateTrueColor (im->sy, im->sx);
		if (rotated == NULL) {
			gdImageDestroy (im);
			return GP_ERROR_NO_MEMORY;
		}
		rotate270 (im, rotated);
		gdImageDestroy (im);
		im = rotated;
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
	gp_log(GP_LOG_ERROR,"st2205", "GD decompression not supported - no libGD present during build");
	return GP_ERROR_NOT_SUPPORTED;
#endif
}

static int
put_file_func (CameraFilesystem *fs, const char *folder, const char *name, 
	CameraFileType type, CameraFile *file, void *data, GPContext *context)
{
#ifdef HAVE_LIBGD
	Camera *camera = data;
	char *c, *in_name, *out_name, *filedata = NULL;
	int ret, in_width, in_height, in_x, in_y;
	size_t inc, outc;
	double aspect_in, aspect_out;
#if defined(HAVE_ICONV) && defined(HAVE_LANGINFO_H)
	char *in, *out;
#else
	int i;
#endif
	unsigned long filesize = 0;
	gdImagePtr rotated, im_out, im_in = NULL;

	if (strcmp (folder, "/"))
		return GP_ERROR_DIRECTORY_NOT_FOUND;

	inc = strlen (name);
	in_name = strdup (name);
	outc = inc;
	out_name = malloc (outc + 1);
	if (!in_name || !out_name) {
		free (in_name);
		free (out_name);
		return GP_ERROR_NO_MEMORY;
	}

	/* Convert name to ASCII */
#if defined(HAVE_ICONV) && defined(HAVE_LANGINFO_H)
	in = in_name;
	out = out_name;
	if (iconv (camera->pl->cd, &in, &inc, &out, &outc) == -1) {
		free (in_name);
		free (out_name);
		gp_log (GP_LOG_ERROR, "iconv",
			"Failed to convert filename to ASCII");
		return GP_ERROR_OS_FAILURE;
	}
	outc = out - out_name;
	out_name[outc] = 0;
#else
	for (i = 0; i < inc; i++) {
		if ((uint8_t)in_name[i] < 0x20 || (uint8_t)in_name[i] > 0x7E)
			out_name[i] = '?';
		else
			out_name[i] = in_name[i];
	}
	out_name[i] = 0;
#endif
	free (in_name);

	/* Remove file extension, and if necessary truncate the name */
	c = strrchr (out_name, '.');
	if (c)
		*c = 0;
	if (outc > ST2205_FILENAME_LENGTH)
		out_name[ST2205_FILENAME_LENGTH] = 0;

	ret = gp_file_get_data_and_size (file, (const char **)&filedata,
					 &filesize);
	if (ret < 0) { free (out_name); return ret; }

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
		gp_log (GP_LOG_ERROR, "st2205",
			"Unrecognized file format for file: %s%s",
			folder, name);
		free (out_name);
		return GP_ERROR_BAD_PARAMETERS;
	}

	if (needs_rotation (camera)) {
		rotated = gdImageCreateTrueColor (im_in->sy, im_in->sx);
		if (rotated == NULL) {
			gdImageDestroy (im_in);
			free (out_name);
			return GP_ERROR_NO_MEMORY;
		}
		rotate90 (im_in, rotated);
		gdImageDestroy (im_in);
		im_in = rotated;
	}

	im_out = gdImageCreateTrueColor(camera->pl->width, camera->pl->height);
	if (im_out == NULL) {
		gdImageDestroy (im_in);
		free (out_name);
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

	ret = st2205_write_file (camera, out_name, im_out->tpixels);
	if (ret >= 0) {
		/* Add to our filenames list */
		ST2205_SET_FILENAME(camera->pl->filenames[ret], out_name, ret);
		/* And commit the changes to the device */
		ret = st2205_commit(camera);
	}

	gdImageDestroy (im_in);
	gdImageDestroy (im_out);
	free (out_name);
	return ret;
#else
	gp_log(GP_LOG_ERROR,"st2205", "GD compression not supported - no libGD present during build");
	return GP_ERROR_NOT_SUPPORTED;
#endif
}

static int
delete_file_func (CameraFilesystem *fs, const char *folder,
		  const char *filename, void *data, GPContext *context)
{
	Camera *camera = data;
	int ret, idx;

	idx = get_file_idx(camera->pl, folder, filename);
	if (idx < 0)
		return idx;

	ret = st2205_delete_file(camera, idx);
	if (ret < 0) return ret;

	/* Also remove the file from our cached filelist */
	camera->pl->filenames[idx][0] = 0;

	return st2205_commit(camera);
}

static int
delete_all_func (CameraFilesystem *fs, const char *folder, void *data,
		 GPContext *context)
{
	Camera *camera = data;

	CHECK (st2205_delete_all (camera))

	return st2205_commit (camera);
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
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	Camera *camera = data; 
	int i, ret;

	for (i = 0; i < ST2205_MAX_NO_FILES; i++) {
		if (camera->pl->filenames[i][0]) {
			ret = gp_list_append (list, camera->pl->filenames[i],
					      NULL);
			if (ret < 0) return ret;
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
	int free;

	free = st2205_get_free_mem_size (camera);
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
	sinfo->capacitykbytes = st2205_get_mem_size (camera) / 1024;
	sinfo->fields |= GP_STORAGEINFO_FREESPACEKBYTES;
	sinfo->freekbytes = free / 1024;

	return GP_OK;
}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.get_info_func = get_info_func,
	.get_file_func = get_file_func,
	.del_file_func = delete_file_func,
	.put_file_func = put_file_func,
	.delete_all_func = delete_all_func,
	.storage_info_func = storage_info_func
};

static char *
orientation_to_string (int orientation)
{
	switch (orientation) {
	case ORIENTATION_AUTO:
		return  _("Auto");
	case ORIENTATION_LANDSCAPE:
		return  _("Landscape");
	case ORIENTATION_PORTRAIT:
		return  _("Portrait");
	}
	/* Never reached */
	return NULL;
}

static int
string_to_orientation (const char *str)
{
	if (strcmp (str, _("Auto")) == 0)
		return ORIENTATION_AUTO;
	else if (strcmp (str, _("Landscape")) == 0)
		return ORIENTATION_LANDSCAPE;
	else if (strcmp (str, _("Portrait")) == 0)
		return ORIENTATION_PORTRAIT;
	else
		return GP_ERROR_NOT_SUPPORTED;
}

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

	gp_widget_new (GP_WIDGET_RADIO, _("Orientation"), &child);
	gp_widget_add_choice (child, orientation_to_string (0));
	gp_widget_add_choice (child, orientation_to_string (1));
	gp_widget_add_choice (child, orientation_to_string (2));
       	gp_widget_set_value (child,
			     orientation_to_string (camera->pl->orientation));
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

	ret = gp_widget_get_child_by_label (window, _("Orientation"), &child);
	if (ret == GP_OK) {
		char *value;
		int orientation;
		gp_widget_get_value (child, &value);
		orientation = string_to_orientation (value);
		if (orientation < 0) return orientation;
		camera->pl->orientation = orientation;
	}

	return GP_OK;
}

static int
camera_exit (Camera *camera, GPContext *context) 
{
	char buf[2];

	if (camera->pl != NULL) {
		buf[0] = '0' + camera->pl->syncdatetime;
		buf[1] = 0;
		gp_setting_set ("st2205", "syncdatetime", buf);
		gp_setting_set ("st2205", "orientation", orientation_to_string
						(camera->pl->orientation));
#if defined(HAVE_ICONV) && defined(HAVE_LANGINFO_H)
		if (camera->pl->cd != (iconv_t) -1)
			iconv_close (camera->pl->cd);
#endif
		st2205_close (camera);
		free (camera->pl);
		camera->pl = NULL;
	}
	return GP_OK;
}

int
camera_init (Camera *camera, GPContext *context) 
{
	int i, j, ret;
#if defined(HAVE_ICONV) && defined(HAVE_LANGINFO_H)
	char *curloc;
#endif
	char buf[256];
	st2205_filename clean_name;

	/* First, set up all the function pointers */
	camera->functions->exit    = camera_exit;
	camera->functions->summary = camera_summary;
	camera->functions->manual  = camera_manual;
	camera->functions->about   = camera_about;
	camera->functions->get_config = camera_get_config;
	camera->functions->set_config = camera_set_config;
	/* FIXME add gp_camera_get_storageinfo support */

	/* Tell the CameraFilesystem where to get lists from */
	gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera);

	camera->pl = calloc (1, sizeof(CameraPrivateLibrary));
	if (!camera->pl) return GP_ERROR_NO_MEMORY;

	ret = gp_setting_get("st2205", "syncdatetime", buf);
	if (ret == GP_OK)
		camera->pl->syncdatetime = buf[0] == '1';
	else
		camera->pl->syncdatetime = 1;

	ret = gp_setting_get("st2205", "orientation", buf);
	if (ret == GP_OK) {
		ret = string_to_orientation (buf);
		if (ret >= 0)
			camera->pl->orientation = ret;
	}

#if defined(HAVE_ICONV) && defined(HAVE_LANGINFO_H)
	curloc = nl_langinfo (CODESET);
	if (!curloc)
		curloc="UTF-8";
	camera->pl->cd = iconv_open("ASCII", curloc);
	if (camera->pl->cd == (iconv_t) -1) {
		gp_log (GP_LOG_ERROR, "iconv",
			"Failed to create iconv converter");
		camera_exit (camera, context);
		return GP_ERROR_OS_FAILURE;
	}
#endif

#if 1
	ret = st2205_open_device (camera);
#else
	ret = st2205_open_dump (camera,
				"/home/hans/st2205tool/memdump.bin", 128, 128);
#endif
	if (ret != GP_OK) {
		camera_exit (camera, context);
		return ret;
	}

	GP_DEBUG ("st2205 memory size: %d, free: %d",
		  st2205_get_mem_size (camera),
		  st2205_get_free_mem_size (camera));

	/* Get the filenames from the picframe */
	ret = st2205_get_filenames (camera, camera->pl->filenames);
	if (ret != GP_OK) {
		camera_exit (camera, context);
		return ret;
	}

	/* And clean them up and make them unique */
	for (i = 0; i < ST2205_MAX_NO_FILES; i++) {
		if (!camera->pl->filenames[i][0])
			continue;

		/* Filter out non ASCII chars (some frames ship
		   with sample photo's with garbage in the names) */
		for (j = 0; camera->pl->filenames[i][j]; j++) {
			if ((uint8_t)camera->pl->filenames[i][j] < 0x20 ||
			    (uint8_t)camera->pl->filenames[i][j] > 0x7E)
				clean_name[j] = '?';
			else
				clean_name[j] = camera->pl->filenames[i][j];
		}
		clean_name[j] = 0;

		ST2205_SET_FILENAME(camera->pl->filenames[i], clean_name, i);
	}

	/* Sync time if requested */
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
			ret = st2205_set_time_and_date (camera, xtm);
			if (ret != GP_OK) {
				camera_exit (camera, context);
				return ret;
			}
		}
	}

	return GP_OK;
}
