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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#ifdef OS2
#include <db.h>
#endif
#include <netinet/in.h>

#include <gphoto2.h>
#include <gphoto2-port.h>

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define _(String) (String)
#  define N_(String) (String)
#endif

#include "digita.h"

static struct {
	char *model;
	int usb_vendor;
	int usb_product;
} models[] = {
	{"Kodak DC220", 0x040A, 0x0100 },
	{"Kodak DC260", 0x040A, 0x0110 },
	{"Kodak DC265", 0x040A, 0x0111 },
	{"Kodak DC290", 0x040A, 0x0112 },
	{"HP PhotoSmart 618", 0x03F0, 0x4102 },
};

int camera_abilities(CameraAbilitiesList *list)
{
	int i;
	CameraAbilities a;

	for (i = 0; i < sizeof(models) / sizeof(models[0]); i++) {
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

static int camera_exit(Camera *camera)
{
	if (camera->pl) {
		free(camera->pl);
		camera->pl = NULL;
	}

	return GP_OK;
}

static int folder_list_func(CameraFilesystem *fs, const char *folder,
			     CameraList *list, void *data)
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
		char tmppath[PATH_MAX + 1];
		char *path;

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

		strncpy(tmppath, path, MIN(strlen(path) - 1, PATH_MAX));
		tmppath[strlen(path) - 1] = 0;

		found = 0;
		for (i1 = 0; i1 < gp_list_count(list); i1++) {
			const char *name;

			gp_list_get_name(list, i1, &name);
			if (!strcmp(name, tmppath)) {
				found = 1;
				break;
			}
		}

		if (!found)
			gp_list_append(list, tmppath, NULL);
	}

	return GP_OK;
}

static int file_list_func(CameraFilesystem *fs, const char *folder,
			   CameraList *list, void *data)
{
	Camera *camera = data;
	int i;

	gp_debug_printf(GP_DEBUG_HIGH, "digita", "camera_file_list %s", 
			 folder);

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
static char *digita_file_get(Camera *camera, const char *folder, 
			      const char *filename, int thumbnail, int *size)
{
	struct filename fn;
	struct partial_tag tag;
	unsigned char *data;
	int pos, len, buflen;

	fprintf(stderr, "digita_file_get\n");

	fprintf(stderr, "digita: getting %s/%s\n", folder, filename);

	/* Setup the filename */
	/* FIXME: This is kinda lame, but it's a quick hack */
	fn.driveno = camera->pl->file_list[0].fn.driveno;
	strcpy(fn.path, folder);
	strcat(fn.path, "/");
	strcpy(fn.dosname, filename);

	/* How much data we're willing to accept */
	tag.offset = htonl(0);
	tag.length = htonl(GFD_BUFSIZE);
	tag.filesize = htonl(0);

	buflen = GFD_BUFSIZE;
	data = malloc(buflen);
	if (!data) {
		gp_debug_printf(GP_DEBUG_HIGH, "digita", "digita_file_get: allocating memory");
		return NULL;
	}
	memset(data, 0, buflen);

	gp_camera_progress(camera, 0.00);

	if (digita_get_file_data(camera->pl, thumbnail, &fn, &tag, data) < 0) {
		gp_debug_printf(GP_DEBUG_HIGH, "digita", "digita_get_picture: digita_get_file_data failed");
		return NULL;
	}

	buflen = ntohl(tag.filesize);
	if (thumbnail)
		buflen += 16;

	data = realloc(data, buflen);
	if (!data) {
		gp_debug_printf(GP_DEBUG_HIGH, "digita", "digita_file_get: couldn't reallocate memory");
		return NULL;
	}

	len = ntohl(tag.filesize);
	pos = ntohl(tag.length);
	while (pos < len) {
		gp_camera_progress(camera, (float)pos / (float)len);

		tag.offset = htonl(pos);
		if ((len - pos) > GFD_BUFSIZE)
			tag.length = htonl(GFD_BUFSIZE);
		else
			tag.length = htonl(len - pos);

		if (digita_get_file_data(camera->pl, thumbnail, &fn, &tag, data + pos) < 0) {
			gp_debug_printf(GP_DEBUG_HIGH, "digita", "digita_get_picture: digita_get_file_data failed");
			return NULL;
		}
		pos += ntohl(tag.length);
	}

	gp_camera_progress(camera, 1.00);

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
			  CameraFile *file, void *user_data)
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
		fprintf(stderr, "Getting picture\n");
	        data = digita_file_get(camera, folder, filename, 0, &buflen);
		break;
	case GP_FILE_TYPE_PREVIEW:
		fprintf(stderr, "Getting thumbnail\n");
		data = digita_file_get(camera, folder, filename, 1, &buflen);
		break;
	default:
		fprintf(stderr, "Unsupported image type\n");
		return GP_ERROR_NOT_SUPPORTED;
	}

	if (!data)
		return GP_ERROR;

	gp_file_set_name(file, filename);

	switch (type) {
	case GP_FILE_TYPE_NORMAL:
		gp_file_set_data_and_size(file, data, buflen);
		gp_file_set_mime_type(file, GP_MIME_JPEG);
		break;
	case GP_FILE_TYPE_PREVIEW:
	{
		char *ppm;

		memcpy((void *)&height, data + 4, 4);
		height = ntohl(height);
		memcpy((void *)&width, data + 8, 4);
		width = ntohl(width);

		gp_debug_printf(GP_DEBUG_LOW, "digita", "picture size %dx%d", width, height);
		gp_debug_printf(GP_DEBUG_LOW, "digita", "data size %d", buflen - 16);

		sprintf(ppmhead,
			"P6\n"
			"# CREATOR: gphoto2, digita library\n"
			"%i %i\n"
			"255\n", width, height);
		ppm = malloc(strlen(ppmhead) + (width * height * 3));
		if (!ppm)
			return -1;

		strcpy(ppm, ppmhead);

		decode_ycc422(data + 16, width, height, ppm + strlen(ppmhead));
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

static int camera_summary(Camera *camera, CameraText *summary)
{
	int taken;

	if (digita_get_storage_status(camera->pl, &taken, NULL, NULL) < 0)
		return 0;

	sprintf(summary->text, _("Number of pictures: %d"), taken);

	return GP_OK;
}

static int camera_manual(Camera *camera, CameraText *manual)
{
	strcpy(manual->text, _("Manual Not Available"));

	return GP_ERROR;
}

static int camera_about(Camera *camera, CameraText *about)
{
	strcpy(about->text, _("Digita\n" \
		"Johannes Erdfelt <johannes@erdfelt.com>"));

	return GP_OK;
}

#if 0
static int delete_picture(int index)
{
	struct filename fn;

	fprintf(stderr, "digita_delete_picture\n");

	if (index > digita_num_pictures)
		return 0;

	index--;

	fprintf(stderr, "deleting %d, %s%s\n", index, digita_file_list[index].fn.path, digita_file_list[index].fn.dosname);

	/* Setup the filename */
	fn.driveno = digita_file_list[index].fn.driveno;
	strcpy(fn.path, digita_file_list[index].fn.path);
	strcpy(fn.dosname, digita_file_list[index].fn.dosname);

	if (digita_delete_picture(camera->pl, &fn) < 0)
		return 0;

	if (digita_get_file_list(camera->pl) < 0)
		return 0;

	return 1;
}
#endif

int camera_init(Camera *camera)
{
	int ret = 0;

	if (!camera)
		return GP_ERROR;

	/* First, set up all the function pointers */
	camera->functions->exit		= camera_exit;
	camera->functions->summary	= camera_summary;
	camera->functions->manual	= camera_manual;
	camera->functions->about	= camera_about;

	/* Set up the CameraFilesystem */
	gp_filesystem_set_list_funcs(camera->fs, file_list_func,
				      folder_list_func, camera);
	gp_filesystem_set_file_funcs(camera->fs, get_file_func, NULL, camera);

	gp_debug_printf(GP_DEBUG_LOW, "digita", "Initializing the camera");

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
		gp_debug_printf(GP_DEBUG_HIGH, "digita", "camera_init: couldn't open digita device");
		free(camera->pl);
		camera->pl = NULL;
		return ret;
	}

	return GP_OK;
}

