/*
 * digita.c
 *
 * Copyright (c) 1999-2000 Johannes Erdfelt
 * Copyright (c) 1999-2000 VA Linux Systems
 */

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <termios.h>

#include <gphoto2.h>

#include "digita.h"

#include <gpio/gpio.h>

static int debug = 0;
static struct digita_device *dev = NULL;

int (*digita_send)(struct digita_device *dev, void *buffer, int buflen) = NULL;
int (*digita_read)(struct digita_device *dev, void *buffer, int buflen) = NULL;

int camera_id(char *id)
{
	strcpy(id, "digita");

	return GP_OK;
}

int camera_abilities(CameraAbilities *abilities, int *count)
{
	*count = 1;

	strcpy(abilities[0].model, "Kodak DC260");
	abilities[0].serial	= 1;
	abilities[0].parallel	= 0;
	abilities[0].usb	= 1;
	abilities[0].ieee1394	= 0;
	abilities[0].speed[0]	= 57600;
	abilities[0].speed[1]	= 0;
	abilities[0].capture	= 1;
	abilities[0].config	= 0;
	abilities[0].file_delete = 1;
	abilities[0].file_preview = 1;
	abilities[0].file_put = 0;

	return GP_OK;
}

int camera_init(CameraInit *init)
{
	debug = init->debug;

	if (debug)
		printf("digita: Initializing the camera\n");

	if (dev) {
		gpio_close(dev);
		gpio_open(dev);
	}

	dev = digita_usb_open();

/*
	if (camera_type == GPHOTO_CAMERA_USB)
		dev = digita_usb_open();
	else
		dev = digita_serial_open();
*/

	return dev ? GP_OK : GP_ERROR;
}

int camera_exit(void)
{
	return GP_OK;
}

int camera_folder_list(char *folder_name, CameraFolderInfo *list)
{
	return GP_OK;
}

int camera_folder_set(char *folder_name)
{
	return GP_OK;
}

int camera_file_count(void)
{
	int taken;

	printf("digita: camera_file_count\n");

	if (digita_get_storage_status(dev, &taken, NULL, NULL) < 0)
		return 0;

	if (digita_get_file_list(dev) < 0)
		return 0;

	return taken;
}

#define GFD_BUFSIZE 19432
static char *digita_file_get(int index, int thumbnail, int *size)
{
	struct filename fn;
	struct partial_tag tag;
	unsigned char *data;
	int i, ret, pos, len, buflen;

	printf("digita: camera_file_get\n");

	if (index > digita_num_pictures) {
		fprintf(stderr, "digita: index %d out of range\n", index);
 		return NULL;
	}

	printf("digita: getting %d, %s%s\n", index, digita_file_list[index].fn.path, digita_file_list[index].fn.dosname);

	/* Setup the filename */
	fn.driveno = digita_file_list[index].fn.driveno;
	strcpy(fn.path, digita_file_list[index].fn.path);
	strcpy(fn.dosname, digita_file_list[index].fn.dosname);

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

	gp_progress(0.00);

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
		gp_progress((float)pos / (float)len);
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

	gp_progress(1.00);

	return data;
}

int camera_file_get(int index, CameraFile *file)
{
	unsigned char *data;
	int buflen;

	data = digita_file_get(index, 0, &buflen);
	if (!data)
		return GP_ERROR;

	file->data = data;
	strcpy(file->type, "image/jpeg");
	file->size = buflen;

	snprintf(file->name, sizeof(file->name), "%s%s",
		digita_file_list[index].fn.path,
		digita_file_list[index].fn.dosname);

	return GP_OK;
}

char *ppmheadfmt = "P6\n# test.ppm\n%i %i\n255\n";

int camera_file_get_preview(int index, CameraFile *file)
{
	unsigned char *data;
	int i, buflen;
	unsigned char *buf, *rgb, *ps;
	unsigned int *ints, width, height;
	char ppmhead[64];

	data = digita_file_get(index, 0, &buflen);
	if (!data)
		return GP_ERROR;

	ints = (unsigned int *)data;
	width = ntohl(ints[2]);
	height = ntohl(ints[1]);

	sprintf(ppmhead, ppmheadfmt, height - 1, width - 1);

	buf = malloc((width * height * 3) + strlen(ppmhead));
	if (!buf) {
		fprintf(stderr, "error allocating rgb data\n");
		return GP_ERROR;
	}

	strcpy(buf, ppmhead);

	rgb = buf + strlen(buf);

	ps = data + 16;
	for (i = 0; i < height; i++) {
		char *pd = rgb + (width * ((height - 1) - i) * 3);
		while (ps < data + 16 + (width * i * 2)) {
			int y, u, y1, v, r, g, b;

#define LIMIT(x) ((((x)>0xffffff)?0xff0000:(((x)<=0xffff)?0:(x)&0xff0000))>>16)

u =  *ps++ - 128;
y =  *ps++ - 16;
v =  *ps++ - 128;
y1 = *ps++ - 16;
r = 104635 * v;
g = -25690 * u + -53294 * v;
b = 132278 * u;
y  *= 76310;
y1 *= 76310;
*pd++ = LIMIT(b + y); *pd++ = LIMIT(g + y); *pd++ = LIMIT(r + y);
*pd++ = LIMIT(b + y1); *pd++ = LIMIT(g + y1); *pd++ = LIMIT(r + y1);
		}
	}

	free(data);
		
	file->data = buf;
	strcpy(file->type, "image/ppm");
	file->size = (width * height * 3) + sizeof(ppmhead);

	snprintf(file->name, sizeof(file->name), "%s%s",
		digita_file_list[index].fn.path,
		digita_file_list[index].fn.dosname);

	return GP_OK;
}

int camera_file_put(CameraFile *file)
{
	return GP_OK;
}

int camera_file_delete(int file_number)
{
	return GP_OK;
}

int camera_config_get(CameraWidget *window)
{
	return GP_ERROR;
}

int camera_config_set(CameraSetting *setting, int count)
{
	return GP_ERROR;
}

int camera_capture(CameraFile *file, CameraCaptureInfo *info)
{
	return GP_ERROR;
}

int camera_summary(CameraText *summary)
{
	int taken;

	if (digita_get_storage_status(dev, &taken, NULL, NULL) < 0)
		return 0;

	sprintf(summary->text, "Number of pictures: %d", taken);

	return GP_OK;
}

int camera_manual(CameraText *manual)
{
	strcpy(manual->text, "Manual Not Available");

	return GP_OK;
}

int camera_about(CameraText *about)
{
	strcpy(about->text, "Digita\n" \
		"Johannes Erdfelt <johannes@erdfelt.com>\n" \
		"VA Linux Systems, Inc");

	return GP_OK;
}

static int delete_picture(int index)
{
	struct filename fn;
	int ret;

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

