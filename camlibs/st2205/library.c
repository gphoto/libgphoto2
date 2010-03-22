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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "config.h"

#include <string.h>
#include <stdlib.h>
#ifdef HAVE_ICONV
# include <langinfo.h>
#endif
#ifdef HAVE_GD
# include <gd.h>
#endif

#include <gphoto2/gphoto2-library.h>
#include <gphoto2/gphoto2-result.h>
#include <gphoto2/gphoto2-port.h>
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
	a.folder_operations = GP_FOLDER_OPERATION_PUT_FILE;
	/* FIXME add support for downloading RAW images */
	/* FIXME add support for delete all */
	a.file_operations   = GP_FILE_OPERATION_DELETE;
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

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *data,
	       GPContext *context)
{
#ifdef HAVE_GD
	Camera *camera = data;
	gdImagePtr im;
	int ret, idx, size;
	void *gdpng;

	idx = get_file_idx(camera->pl, folder, filename);
	if (idx < 0)
		return idx;

	im = gdImageCreateTrueColor(camera->pl->width, camera->pl->height);
	if (im == NULL)
		return GP_ERROR_NO_MEMORY;

	ret = st2205_read_file(camera, idx, im->tpixels);
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
	return GP_ERROR_NOT_SUPPORTED;
#endif
}

static int
put_file_func (CameraFilesystem *fs, const char *folder, const char *name, 
	CameraFileType type, CameraFile *file, void *data, GPContext *context)
{
#ifdef HAVE_GD
	Camera *camera = data;
	char *c, *in_name, *out_name, *filedata = NULL;
	int ret, in_width, in_height, in_x, in_y;
	size_t inc, outc;
	double aspect_in, aspect_out;
#ifdef HAVE_ICONV
	char *in, *out;
#endif
	unsigned long filesize = 0;
	gdImagePtr im_out, im_in = NULL;

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
#ifdef HAVE_ICONV
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

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.get_info_func = get_info_func,
	.get_file_func = get_file_func,
	.del_file_func = delete_file_func,
	.put_file_func = put_file_func,
};

static int
camera_exit (Camera *camera, GPContext *context) 
{
	if (camera->pl != NULL) {
#ifdef HAVE_ICONV
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
#ifdef HAVE_ICONV
	char *curloc;
#endif
	st2205_filename clean_name;

	/* First, set up all the function pointers */
	camera->functions->exit    = camera_exit;
	camera->functions->summary = camera_summary;
	camera->functions->manual  = camera_manual;
	camera->functions->about   = camera_about;
	/* FIXME add gp_camera_get_storageinfo support */

	/* Tell the CameraFilesystem where to get lists from */
	gp_filesystem_set_funcs (camera->fs, &fsfuncs, camera);

	camera->pl = calloc (1, sizeof(CameraPrivateLibrary));
	if (!camera->pl) return GP_ERROR_NO_MEMORY;

#ifdef HAVE_ICONV
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

	return GP_OK;
}
