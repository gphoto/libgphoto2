/*
 * digita.c
 *
 * Copyright (c) 1999-2000 Johannes Erdfelt
 * Copyright (c) 1999-2000 VA Linux Systems
 */

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>

#include <gphoto2.h>

#include "digita.h"

#include <gpio.h>

static int debug = 0;
static struct digita_device *dev = NULL;

int (*digita_send)(struct digita_device *dev, void *buffer, int buflen) = NULL;
int (*digita_read)(struct digita_device *dev, void *buffer, int buflen) = NULL;

int camera_id(CameraText *id)
{
        strcpy(id->text, "digita");

        return GP_OK;
}

char *models[] = {  
        "Kodak DC265",
        "Kodak DC290",
        "Kodak DC220",
        NULL
};

int camera_abilities(CameraAbilitiesList *list)
{
        int x=0;
        CameraAbilities *a;

        while (models[x]) {
                a = gp_abilities_new();
                strcpy(a->model, models[x]);
                a->port       = GP_PORT_SERIAL | GP_PORT_USB;
                a->speed[0]   = 57600;
                a->speed[1]   = 0;
                a->capture    = 1;
                a->config     = 0;
                a->file_delete = 1;
                a->file_preview = 1;
                a->file_put = 0;

                gp_abilities_list_append(list, a);
                x++;
        }

	return (GP_OK);
}

int camera_init(Camera *camera, CameraInit *init)
{
        debug = init->debug;

        /* First, set up all the function pointers */
        camera->functions->id           = camera_id;
        camera->functions->abilities    = camera_abilities;
        camera->functions->init         = camera_init;
        camera->functions->exit         = camera_exit;
        camera->functions->folder_list  = camera_folder_list;
	camera->functions->file_list	= camera_file_list;
        camera->functions->file_get     = camera_file_get;
        camera->functions->file_get_preview =  camera_file_get_preview;
        camera->functions->file_put     = NULL;
        camera->functions->file_delete  = camera_file_delete;
        camera->functions->config       = camera_config;
        camera->functions->capture      = camera_capture;
        camera->functions->summary      = camera_summary;
        camera->functions->manual       = camera_manual;
        camera->functions->about        = camera_about;

        if (debug)
                printf("digita: Initializing the camera\n");

        if (dev) {
                gpio_close(dev->gpdev);
                gpio_open(dev->gpdev);
        }

        if (!strcmp(init->port.path, "usb"))
                dev = digita_usb_open(camera);
        else
                dev = digita_serial_open(camera,init);

	camera->camlib_data = dev;

        return dev ? GP_OK : GP_ERROR;
}

int camera_exit(Camera *camera)
{
        return GP_OK;
}

int camera_folder_list(Camera *camera, CameraList *list, char *folder_name)
{
        return GP_OK;
}

int camera_file_list(Camera *camera, CameraList *list, char *folder)
{
	return GP_OK;
}

#if 0
int camera_file_count(Camera *camera)
{
        int taken;

        printf("digita: camera_file_count\n");

        if (digita_get_storage_status(dev, &taken, NULL, NULL) < 0)
                return 0;

        if (digita_get_file_list(dev) < 0)
                return 0;

        return taken;
}
#endif

#define GFD_BUFSIZE 19432
static char *digita_file_get(char *folder, char *filename, int thumbnail,
			int *size)
{
	struct filename fn;
	struct partial_tag tag;
	unsigned char *data;
	int pos, len, buflen;

	printf("digita: camera_file_get\n");

	printf("digita: getting %s%s\n", folder, filename);

	/* Setup the filename */
/*
	fn.driveno = digita_file_list[index].fn.driveno;
*/
	fn.driveno = 0;
	strcpy(fn.path, folder);
	strcpy(fn.dosname, filename);
/*
digita_file_list[index].fn.path);
digita_file_list[index].fn.dosname);
*/

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

        return data;
}

int camera_file_get(Camera *camera, CameraFile *file, char *folder,
			char *filename)
{
        unsigned char *data;
        int buflen;

        data = digita_file_get(folder, filename, 0, &buflen);
        if (!data)
                return GP_ERROR;

        file->data = data;
        strcpy(file->type, "image/jpeg");
        file->size = buflen;

        snprintf(file->name, sizeof(file->name), "%s%s",
		folder, filename);

        return GP_OK;
}

int camera_file_get_preview(Camera *camera, CameraFile *file, char *folder,
			char *filename)
{
	unsigned char *data;
	int i, buflen;
	unsigned char *buf, *rgb, *ps;
	unsigned int *ints, width, height;
	char ppmhead[64];

	data = digita_file_get(folder, filename, 1, &buflen);
	if (!data)
		return GP_ERROR;

	ints = (unsigned int *)data;
	width = ntohl(ints[2]);
	height = ntohl(ints[1]);

	fprintf(stderr, "digita: height: %d, width: %d\n", height, width);

	sprintf(ppmhead, "P6\n# test.ppm\n%i %i\n255\n",
		height - 1, width - 1);

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
		folder, filename);

/*
                digita_file_list[index].fn.path,
                digita_file_list[index].fn.dosname);
*/

        return GP_OK;
}

int camera_file_put(Camera *camera, CameraFile *file, char *folder)
{
        return GP_OK;
}

int camera_file_delete(Camera *camera, char *folder, char *filename)
{
        return GP_OK;
}

int camera_config(Camera *camera)
{
        return GP_ERROR;
}

int camera_capture(Camera *camera, CameraFile *file, CameraCaptureInfo *info)
{
        return GP_ERROR;
}

int camera_summary(Camera *camera, CameraText *summary)
{
        int taken;

        if (digita_get_storage_status(dev, &taken, NULL, NULL) < 0)
                return 0;

        sprintf(summary->text, "Number of pictures: %d", taken);

        return GP_OK;
}

int camera_manual(Camera *camera, CameraText *manual)
{
        strcpy(manual->text, "Manual Not Available");

        return GP_OK;
}

int camera_about(Camera *camera, CameraText *about)
{
        strcpy(about->text, "Digita\n" \
                "Johannes Erdfelt <johannes@erdfelt.com>\n" \
                "VA Linux Systems, Inc");

        return GP_OK;
}

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
