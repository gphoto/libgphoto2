/*
 * digita.c
 *
 * Copyright 1999-2001 Johannes Erdfelt
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#define _BSD_SOURCE

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef OS2
#include <db.h>
#endif

#define GP_MODULE "digita"

#include <gphoto2/gphoto2.h>
#include <gphoto2/gphoto2-port.h>

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

#include "gphoto2-endian.h"
#include "digita.h"

#ifndef MAX
# define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
# define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

static const struct {
	char *model;
	int usb_vendor;
	int usb_product;
} models[] = {
	{"Kodak:DC220",		0x040A, 0x0100 },
	{"Kodak:DC260",		0x040A, 0x0110 },
	{"Kodak:DC265",		0x040A, 0x0111 },
	{"Kodak:DC290",		0x040A, 0x0112 },
	{"HP:PhotoSmart C500",	0x03F0, 0x4102 },
	{"HP:PhotoSmart 618",	0x03F0, 0x4102 },
	{"HP:PhotoSmart 912",	0x03F0, 0x4102 },
	{"HP:PhotoSmart C500 2",0xf003, 0x6002 },
};

int camera_abilities(CameraAbilitiesList *list)
{
	int i;
	CameraAbilities a;

	for (i = 0; i < sizeof(models) / sizeof(models[0]); i++) {
		memset(&a, 0, sizeof(a));
		strcpy(a.model, models[i].model);
		a.status	= GP_DRIVER_STATUS_PRODUCTION;
		a.port		= GP_PORT_SERIAL | GP_PORT_USB;
		a.speed[0]	= 9600;
		a.speed[1]	= 19200;
		a.speed[2]	= 38400;
		a.speed[3]	= 57600;
		a.speed[4]	= 115200;
		a.speed[5]	= 0;
		a.operations		= GP_OPERATION_NONE;
		a.folder_operations	= GP_FOLDER_OPERATION_NONE;
		a.file_operations	= GP_FILE_OPERATION_PREVIEW | 
					  GP_FILE_OPERATION_DELETE;
		a.usb_vendor  = models[i].usb_vendor;
		a.usb_product = models[i].usb_product;

		gp_abilities_list_append(list, a);
	}

	return GP_OK;
}

int camera_id(CameraText *id)
{
	strcpy(id->text, "digita");

	return GP_OK;
}

static int camera_exit(Camera *camera, GPContext *context)
{
	if (camera->pl) {
		free(camera->pl);
		camera->pl = NULL;
	}

	return GP_OK;
}

static int folder_list_func(CameraFilesystem *fs, const char *folder,
			    CameraList *list, void *data, GPContext *context)
{
	Camera *camera = data;
	int i, i1;

	if (digita_get_file_list(camera->pl) < 0)
		return GP_ERROR;

	if (folder[0] == '/')
		folder++;

	/* Walk through all of the pictures building a list of folders */
	for (i = 0; i < camera->pl->num_pictures; i++) {
		int found;
		char *path,*newfolder;

		/* Check to make sure the path matches the folder we're */
		/*  looking through */
		if (strlen(folder) && strncmp(camera->pl->file_list[i].fn.path,
		    folder, strlen(folder)))
			continue;

		/* If it's not the root, then we skip the / as well */
		if (strlen(folder))
			path = camera->pl->file_list[i].fn.path + strlen(folder) + 1;
		else
			path = camera->pl->file_list[i].fn.path;

		if (!strlen(path))
			continue;

		if (strchr(path, '/') != path + strlen(path) - 1)
			continue;

		newfolder = strdup(path);
		*strchr(newfolder,'/') = 0;
		found = 0;
		for (i1 = 0; i1 < gp_list_count(list); i1++) {
			const char *name;

			gp_list_get_name(list, i1, &name);
			if (!strcmp(name, newfolder)) {
				found = 1;
				break;
			}
		}

		if (!found)
			gp_list_append(list, newfolder, NULL);
		free (newfolder);
	}

	return GP_OK;
}

static int file_list_func(CameraFilesystem *fs, const char *folder,
			   CameraList *list, void *data, GPContext *context)
{
	Camera *camera = data;
	int i;

	/* We should probably check to see if we have a list already and */
	/*  get the changes since */
	if (digita_get_file_list(camera->pl) < 0)
		return GP_ERROR;

	if (folder[0] == '/')
		folder++;

	/* Walk through all of the pictures building a list of folders */
	for (i = 0; i < camera->pl->num_pictures; i++) {
		if (strncmp(camera->pl->file_list[i].fn.path, folder, strlen(folder)) ||
		    camera->pl->file_list[i].fn.path[strlen(folder)] != '/')
			continue;

		gp_list_append(list, camera->pl->file_list[i].fn.dosname, NULL);
	}

	return GP_OK;
}

#define GFD_BUFSIZE 19432
static unsigned char *digita_file_get(Camera *camera, const char *folder, 
			     const char *filename, int thumbnail, int *size,
			     GPContext *context)
{
	struct filename fn;
	struct partial_tag tag;
	unsigned char *data;
	int pos, len, buflen;
	unsigned int id;

	GP_DEBUG ("Getting %s from folder %s...", filename, folder);

	/* Setup the filename */
	/* FIXME: This is kinda lame, but it's a quick hack */
	fn.driveno = camera->pl->file_list[0].fn.driveno;
	strcpy(fn.path, folder);
	strcat(fn.path, "/");
	strcpy(fn.dosname, filename);

	/* How much data we're willing to accept */
	tag.offset = htobe32(0);
	tag.length = htobe32(GFD_BUFSIZE);
	tag.filesize = htobe32(0);

	buflen = GFD_BUFSIZE;
	data = malloc(buflen);
	if (!data) {
		GP_DEBUG( "digita_file_get: allocating memory");
		return NULL;
	}
	memset(data, 0, buflen);

	if (digita_get_file_data(camera->pl, thumbnail, &fn, &tag, data) < 0) {
		GP_DEBUG( "digita_get_picture: digita_get_file_data failed");
		free (data);
		return NULL;
	}

	buflen = be32toh(tag.filesize);
	if (thumbnail)
		buflen += 16;

	data = realloc(data, buflen);
	if (!data) {
		GP_DEBUG( "digita_file_get: couldn't reallocate memory");
		return NULL;
	}

	len = be32toh(tag.filesize);
	pos = be32toh(tag.length);
	id = gp_context_progress_start (context, len, _("Getting file..."));
	while (pos < len) {
		gp_context_progress_update (context, id, pos);

		tag.offset = htobe32(pos);
		if ((len - pos) > GFD_BUFSIZE)
			tag.length = htobe32(GFD_BUFSIZE);
		else
			tag.length = htobe32(len - pos);

		if (digita_get_file_data(camera->pl, thumbnail, &fn, &tag, data + pos) < 0) {
			GP_DEBUG ("digita_get_file_data failed.");
			free (data);
			return NULL;
		}
		pos += be32toh(tag.length);
	}
	gp_context_progress_stop (context, id);

	*size = buflen;

	return data;
}

/* Colorspace conversion is voodoo-magic to me --jerdfelt */
static void decode_ycc422(unsigned char *input, int width, int height, unsigned char *output)
{
	int x, y;

	for (y = 0; y < height; y++) {
		for (x = 0; x < width / 2; x++) {
			int _y, u, y1, v, r, g, b;

#define LIMIT(_x) ((((_x)>0xffffff)?0xff0000:(((_x)<=0xffff)?0:(_x)&0xff0000))>>16)
			u =  *input++ - 128;
			_y =  *input++ - 16;
			v =  *input++ - 128;
			y1 = *input++ - 16;
			r = 104635 * v;
			g = -25690 * u + -53294 * v;
			b = 132278 * u;
			_y  *= 76310;
			y1 *= 76310;
			*output++ = LIMIT(r + _y);
			*output++ = LIMIT(g + _y);
			*output++ = LIMIT(b + _y);
			*output++ = LIMIT(r + y1);
			*output++ = LIMIT(g + y1);
			*output++ = LIMIT(b + y1);
		}
	}
}

static int get_file_func(CameraFilesystem *fs, const char *folder,
			 const char *filename, CameraFileType type,
			 CameraFile *file, void *user_data, GPContext *context)
{
	Camera *camera = user_data;
	unsigned char *data;
	unsigned int width, height;
	char ppmhead[64]; 
	int buflen;

	if (folder[0] == '/')
		folder++;

	switch (type) {
	case GP_FILE_TYPE_NORMAL:
		GP_DEBUG("digita/get_file_func: Getting picture");
	        data = digita_file_get(camera, folder, filename, 0, &buflen, context);
		break;
	case GP_FILE_TYPE_PREVIEW:
		GP_DEBUG("digita/get_file_func: Getting thumbnail");
		data = digita_file_get(camera, folder, filename, 1, &buflen, context);
		break;
	default:
		gp_context_error(context, _("Image type is not supported"));
		return GP_ERROR_NOT_SUPPORTED;
	}

	if (!data)
		return GP_ERROR;

	switch (type) {
	case GP_FILE_TYPE_NORMAL:
		gp_file_set_data_and_size(file, (char*)data, buflen);
		gp_file_set_mime_type(file, GP_MIME_JPEG);
		break;
	case GP_FILE_TYPE_PREVIEW:
	{
		char *ppm;

		memcpy((void *)&height, data + 4, 4);
		height = be32toh(height);
		memcpy((void *)&width, data + 8, 4);
		width = be32toh(width);

		GP_DEBUG( "picture size %dx%d", width, height);
		GP_DEBUG( "data size %d", buflen - 16);

		sprintf(ppmhead,
			"P6\n"
			"# CREATOR: gphoto2, digita library\n"
			"%i %i\n"
			"255\n", width, height);
		ppm = malloc(strlen(ppmhead) + (width * height * 3));
		if (!ppm)
			return -1;

		strcpy(ppm, ppmhead);

		decode_ycc422(data + 16, width, height, (unsigned char*)(ppm + strlen(ppmhead)));
		free(data);

		gp_file_set_mime_type(file, GP_MIME_PPM);
		gp_file_set_data_and_size(file, ppm, strlen(ppmhead) + (width * height * 3));
		break;
	}
	default:
		return GP_ERROR_NOT_SUPPORTED;
	}

	return GP_OK;
}

static int camera_summary(Camera *camera, CameraText *summary, GPContext *context)
{
	int taken;

	if (digita_get_storage_status(camera->pl, &taken, NULL, NULL) < 0)
		return 0;

	sprintf(summary->text, _("Number of pictures: %d"), taken);

	return GP_OK;
}

static int camera_about(Camera *camera, CameraText *about, GPContext *context)
{
	strcpy(about->text, _("Digita\n" \
		"Johannes Erdfelt <johannes@erdfelt.com>"));

	return GP_OK;
}

static int digita_file_delete(Camera *camera, const char *folder, 
			     const char *filename, GPContext *context)
{
	struct filename fn;

	/* Setup the filename */
	/* FIXME: This is kinda lame, but it's a quick hack */
	fn.driveno = camera->pl->file_list[0].fn.driveno;
	strcpy(fn.path, folder);
	strcat(fn.path, "/");
	strcpy(fn.dosname, filename);

	if (digita_delete_picture(camera->pl, &fn) < 0)
		return 0;

	if (digita_get_file_list(camera->pl) < 0)
		return 0;

        return GP_OK;
}

static int delete_file_func(CameraFilesystem *fs, const char *folder,
		const char *filename, void *user_data, GPContext *context)
{
        Camera *camera = user_data;

	if (folder[0] == '/')
		folder++;

        return digita_file_delete(camera, folder, filename, context);
}

static CameraFilesystemFuncs fsfuncs = {
	.file_list_func = file_list_func,
	.folder_list_func = folder_list_func,
	.get_file_func = get_file_func,
	.del_file_func = delete_file_func
};

int camera_init(Camera *camera, GPContext *context)
{
	int ret = 0;

	if (!camera)
		return GP_ERROR;

	/* First, set up all the function pointers */
	camera->functions->exit		= camera_exit;
	camera->functions->summary	= camera_summary;
	camera->functions->about	= camera_about;

	/* Set up the CameraFilesystem */
	gp_filesystem_set_funcs(camera->fs, &fsfuncs, camera);

	GP_DEBUG( "Initializing the camera");

	camera->pl = malloc(sizeof(CameraPrivateLibrary));
	if (!camera->pl)
		return GP_ERROR_NO_MEMORY;
	memset(camera->pl, 0, sizeof(CameraPrivateLibrary));
	camera->pl->gpdev = camera->port;

	switch (camera->port->type) {
	case GP_PORT_USB:
		ret = digita_usb_open(camera->pl, camera);
		break;
	case GP_PORT_SERIAL:
		ret = digita_serial_open(camera->pl, camera);
		break;
	default:
		free(camera->pl);
		camera->pl = NULL;
		return GP_ERROR_UNKNOWN_PORT;
	}

	if (ret < 0) {
		GP_DEBUG( "camera_init: couldn't open digita device");
		free(camera->pl);
		camera->pl = NULL;
		return ret;
	}

	return GP_OK;
}

