/*
 * digita.c
 *
 * Copyright (c) 1999-2000 Johannes Erdfelt
 * Copyright (c) 1999-2000 VA Linux Systems
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <netinet/in.h>

#include <gphoto2.h>

#include "digita.h"

#include <gpio.h>

int (*digita_send)(struct digita_device *dev, void *buffer, int buflen) = NULL;
int (*digita_read)(struct digita_device *dev, void *buffer, int buflen) = NULL;

int camera_id(CameraText *id)
{
	strcpy(id->text, "digita");

	return GP_OK;
}

char *models[] = {  
	"Kodak DC220",
	"Kodak DC260",
	"Kodak DC265",
	"Kodak DC290",
	NULL
};

int camera_abilities(CameraAbilitiesList *list)
{
	int i;
	CameraAbilities *a;

	for (i = 0; models[i]; i++) {
		a = gp_abilities_new();
		strcpy(a->model, models[i]);
		a->port       = GP_PORT_SERIAL | GP_PORT_USB;
		a->speed[0]   = 57600;
		a->speed[1]   = 0;
		a->capture    = 0;
		a->config     = 0;
		a->file_delete = 0;
		a->file_preview = 1;
		a->file_put = 0;

		gp_abilities_list_append(list, a);
	}

	return GP_OK;
}

int camera_init(Camera *camera, CameraInit *init)
{
	struct digita_device *dev;
	int ret = 0;

	if (!camera)
		return GP_ERROR;

	/* First, set up all the function pointers */
	camera->functions->id           = camera_id;
	camera->functions->abilities    = camera_abilities;
	camera->functions->init         = camera_init;
	camera->functions->exit         = camera_exit;
	camera->functions->folder_list  = camera_folder_list;
	camera->functions->file_list	= camera_file_list;
	camera->functions->file_get     = camera_file_get;
	camera->functions->file_get_preview =  camera_file_get_preview;
	camera->functions->file_put     = camera_file_put;
	camera->functions->file_delete  = camera_file_delete;
	camera->functions->config       = camera_config;
	camera->functions->capture      = camera_capture;
	camera->functions->summary      = camera_summary;
	camera->functions->manual       = camera_manual;
	camera->functions->about        = camera_about;

	if (camera->debug)
		fprintf(stderr, "digita: Initializing the camera\n");

	dev = malloc(sizeof(*dev));
	if (!dev) {
		fprintf(stderr, "Couldn't allocate digita device\n");
		return GP_ERROR;
	}
	memset((void *)dev, 0, sizeof(*dev));

	switch (init->port.type) {
	case GP_PORT_USB:
		ret = digita_usb_open(dev, camera, init);
		break;
	case GP_PORT_SERIAL:
		ret = digita_serial_open(dev, camera, init);
		break;
	default:
		fprintf(stderr, "Unknown port type %d\n", init->port.type);
		return GP_ERROR;
	}

	if (ret < 0) {
		fprintf(stderr, "Couldn't open digita device\n");
		return GP_ERROR;
	}

	dev->debug = camera->debug;

	camera->camlib_data = dev;

	return GP_OK;
}

int camera_exit(Camera *camera)
{
	return GP_OK;
}

int camera_folder_list(Camera *camera, CameraList *list, char *folder)
{
	struct digita_device *dev = camera->camlib_data;
	int i, i1;

printf("[digita]camera_folder_list %s\n", folder);

	if (!dev)
		return GP_ERROR;

	if (digita_get_file_list(dev) < 0)
		return GP_ERROR;

	if (folder[0] == '/')
		folder++;

	/* Walk through all of the pictures building a list of folders */
	for (i = 0; i < dev->num_pictures; i++) {
		int found;
		char *path;

		/* Check to make sure the path matches the folder we're */
		/*  looking through */
		if (strlen(folder) && strncmp(dev->file_list[i].fn.path,
		    folder, strlen(folder)))
			continue;

		/* If it's not the root, then we skip the / as well */
		if (strlen(folder))
			path = dev->file_list[i].fn.path + strlen(folder) + 1;
		else
			path = dev->file_list[i].fn.path;

		if (!strlen(path))
			continue;

		if (strchr(path, '/') != path + strlen(path) - 1)
			continue;

		found = 0;
		for (i1 = 0; i1 < gp_list_count(list); i1++) {
			CameraListEntry *entry;

			entry = gp_list_entry(list, i1);
			if (!strcmp(entry->name, path)) {
				found = 1;
				break;
			}
		}

		if (!found)
			gp_list_append(list, path, GP_LIST_FOLDER);
	}

        return GP_OK;
}

int camera_file_list(Camera *camera, CameraList *list, char *folder)
{
	struct digita_device *dev = camera->camlib_data;
	int i;

printf("[digita]camera_file_list %s\n", folder);

	if (!dev)
		return GP_ERROR;

	/* We should probably check to see if we have a list already and */
	/*  get the changes since */
	if (digita_get_file_list(dev) < 0)
		return GP_ERROR;

	if (folder[0] == '/')
		/* FIXME: Why does the frontend return a path with multiple */
		/*  leading slashes? */
		while (folder[0] == '/')
			folder++;

	/* Walk through all of the pictures building a list of folders */
	for (i = 0; i < dev->num_pictures; i++) {
		if (strcmp(dev->file_list[i].fn.path, folder))
			continue;

		gp_list_append(list, dev->file_list[i].fn.dosname, GP_LIST_FOLDER);
	}

	return GP_OK;
}

#define GFD_BUFSIZE 19432
static char *digita_file_get(Camera *camera, char *folder, char *filename,
			int thumbnail, int *size)
{
	struct digita_device *dev = camera->camlib_data;
	struct filename fn;
	struct partial_tag tag;
	unsigned char *data;
	int pos, len, buflen;

printf("digita_file_get\n");

printf("digita: getting %s%s\n", folder, filename);

	/* Setup the filename */
	/* FIXME: This is kinda lame, but it's a quick hack */
	fn.driveno = dev->file_list[0].fn.driveno;
	strcpy(fn.path, folder);
	strcpy(fn.dosname, filename);

	/* How much data we're willing to accept */
	tag.offset = htonl(0);
	tag.length = htonl(GFD_BUFSIZE);
	tag.filesize = htonl(0);

	buflen = GFD_BUFSIZE;
	data = malloc(buflen);
	if (!data) {
		fprintf(stderr, "allocating memory\n");
		return NULL;
	}
	memset(data, 0, buflen);

	gp_frontend_progress(NULL, NULL, 0.00);

	if (digita_get_file_data(dev, thumbnail, &fn, &tag, data) < 0) {
		printf("digita_get_picture: digita_get_file_data failed\n");
		return NULL;
	}

	buflen = ntohl(tag.filesize);
	if (thumbnail)
		buflen += 16;

	data = realloc(data, buflen);
	if (!data) {
		fprintf(stderr, "couldn't reallocate memory\n");
		return NULL;
	}

	len = ntohl(tag.filesize);
	pos = ntohl(tag.length);
	while (pos < len) {
		gp_frontend_progress(NULL, NULL, (float)pos / (float)len);

		tag.offset = htonl(pos);
		if ((len - pos) > GFD_BUFSIZE)
			tag.length = htonl(GFD_BUFSIZE);
		else
			tag.length = htonl(len - pos);

		if (digita_get_file_data(dev, thumbnail, &fn, &tag, data + pos) < 0) {
			printf("digita_get_picture: digita_get_file_data failed\n");
			return NULL;
		}
		pos += ntohl(tag.length);
	}

        gp_frontend_progress(NULL, NULL, 1.00);

	if (size)
		*size = buflen;

        return data;
}

int camera_file_get(Camera *camera, CameraFile *file, char *folder,
			char *filename)
{
	struct digita_device *dev = camera->camlib_data;
        unsigned char *data;
        int buflen;

	if (!dev)
		return GP_ERROR;

	if (folder[0] == '/')
		folder++;

        data = digita_file_get(camera, folder, filename, 0, &buflen);
        if (!data)
                return GP_ERROR;

        file->data = data;
        file->size = buflen;

        strcpy(file->name, filename);
        strcpy(file->type, "image/jpeg");

        return GP_OK;
}

int camera_file_get_preview(Camera *camera, CameraFile *file, char *folder,
			char *filename)
{
	struct digita_device *dev = camera->camlib_data;
	int x, y;
	unsigned char *data, *buf, *rgb, *ps;
	unsigned int *ints, width, height;
	char ppmhead[64];

	if (!dev)
		return GP_ERROR;

	if (folder[0] == '/')
		folder++;

	data = digita_file_get(camera, folder, filename, 1, NULL);
	if (!data)
		return GP_ERROR;

	ints = (unsigned int *)data;
	width = ntohl(ints[2]);
	height = ntohl(ints[1]);

fprintf(stderr, "digita: picture size %dx%d\n", width, height);
fprintf(stderr, "digita: data size %d\n", ntohl(ints[0]));

	sprintf(ppmhead, "P6\n%i %i\n255\n",
		width, height);

	buf = malloc((width * height * 3) + strlen(ppmhead));
	if (!buf) {
		fprintf(stderr, "error allocating rgb data\n");
		return GP_ERROR;
	}

	strcpy(buf, ppmhead);

	rgb = buf + strlen(buf);

	/* Skip over the thumbnail header */
	ps = data + 16;
	for (y = 0; y < height; y++) {
		char *pd = rgb + (width * y * 3);

		for (x = 0; x < width / 2; x++) {
			int _y, u, y1, v, r, g, b;

#define LIMIT(_x) ((((_x)>0xffffff)?0xff0000:(((_x)<=0xffff)?0:(_x)&0xff0000))>>16)

u =  *ps++ - 128;
_y =  *ps++ - 16;
v =  *ps++ - 128;
y1 = *ps++ - 16;
r = 104635 * v;
g = -25690 * u + -53294 * v;
b = 132278 * u;
_y  *= 76310;
y1 *= 76310;
*pd++ = LIMIT(r + _y); *pd++ = LIMIT(g + _y); *pd++ = LIMIT(b + _y);
*pd++ = LIMIT(r + y1); *pd++ = LIMIT(g + y1); *pd++ = LIMIT(b + y1);
                }
        }

        free(data);

        file->data = buf;
        file->size = (width * height * 3) + strlen(ppmhead);

        strcpy(file->type, "image/ppm");
        strcpy(file->name, filename);

	return GP_OK;
}

int camera_file_put(Camera *camera, CameraFile *file, char *folder)
{
	struct digita_device *dev = camera->camlib_data;

	if (!dev)
		return GP_ERROR;

	return GP_ERROR_NOT_SUPPORTED;
}

int camera_file_delete(Camera *camera, char *folder, char *filename)
{
	struct digita_device *dev = camera->camlib_data;

	if (!dev)
		return GP_ERROR;

	return GP_ERROR_NOT_SUPPORTED;
}

int camera_config(Camera *camera)
{
	struct digita_device *dev = camera->camlib_data;

	if (!dev)
		return GP_ERROR;

	return GP_ERROR_NOT_SUPPORTED;
}

int camera_capture(Camera *camera, CameraFile *file, CameraCaptureInfo *info)
{
	struct digita_device *dev = camera->camlib_data;

	if (!dev)
		return GP_ERROR;

	return GP_ERROR_NOT_SUPPORTED;
}

int camera_summary(Camera *camera, CameraText *summary)
{
	struct digita_device *dev = camera->camlib_data;
	int taken;

	if (!dev)
		return GP_ERROR;

	if (digita_get_storage_status(dev, &taken, NULL, NULL) < 0)
		return 0;

	sprintf(summary->text, "Number of pictures: %d", taken);

	return GP_OK;
}

int camera_manual(Camera *camera, CameraText *manual)
{
	strcpy(manual->text, "Manual Not Available");

	return GP_ERROR;
}

int camera_about(Camera *camera, CameraText *about)
{
	strcpy(about->text, "Digita\n" \
		"Johannes Erdfelt <johannes@erdfelt.com>\n" \
		"VA Linux Systems, Inc");

	return GP_OK;
}

#if 0
static int delete_picture(int index)
{
	struct filename fn;

printf("digita_delete_picture\n");

	if (index > digita_num_pictures)
		return 0;

	index--;

printf("deleting %d, %s%s\n", index, digita_file_list[index].fn.path, digita_file_list[index].fn.dosname);

	/* Setup the filename */
	fn.driveno = digita_file_list[index].fn.driveno;
	strcpy(fn.path, digita_file_list[index].fn.path);
	strcpy(fn.dosname, digita_file_list[index].fn.dosname);

	if (digita_delete_picture(dev, &fn) < 0)
		return 0;

	if (digita_get_file_list(dev) < 0)
		return 0;

	return 1;
}
#endif
