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
#ifdef OS2
#include <db.h>
#endif
#include <netinet/in.h>

#include <gphoto2.h>

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define _(String) (String)
#    define N_(String) (String)
#  endif
#else
#  define _(String) (String)
#  define N_(String) (String)
#endif

#include "digita.h"

#include <gphoto2-port.h>

int (*digita_send)(struct digita_device *dev, void *buffer, int buflen) = NULL;
int (*digita_read)(struct digita_device *dev, void *buffer, int buflen) = NULL;

int camera_id(CameraText *id)
{
        strcpy(id->text, "digita");

        return GP_OK;
}

static char *models[] = {
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
                gp_abilities_new(&a);
                strcpy(a->model, models[i]);
                a->port       = GP_PORT_SERIAL | GP_PORT_USB;
                a->speed[0]   = 57600;
                a->speed[1]   = 0;
                a->operations        = 	GP_OPERATION_NONE;
		a->folder_operations = 	GP_FOLDER_OPERATION_NONE;
                a->file_operations   = 	GP_FILE_OPERATION_PREVIEW | 
					GP_FILE_OPERATION_DELETE;

                gp_abilities_list_append(list, a);
        }

        return GP_OK;
}

int camera_exit (Camera *camera)
{
        return GP_OK;
}

int camera_folder_list_folders (Camera *camera, const char *folder, 
				CameraList *list)
{
        struct digita_device *dev = camera->camlib_data;
        int i, i1;

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
			const char *name;

			gp_list_get_name (list, i1, &name);
                        if (!strcmp(name, path)) {
                                found = 1;
                                break;
                        }
                }

                if (!found)
                        gp_list_append(list, path, NULL);
        }

        return GP_OK;
}

int camera_folder_list_files (Camera *camera, const char *folder, 
			      CameraList *list)
{
        struct digita_device *dev = camera->camlib_data;
        int i;

	gp_debug_printf (GP_DEBUG_HIGH, "digita", "camera_file_list %s\n", 
			 folder);

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

                gp_list_append (list, dev->file_list[i].fn.dosname, NULL);
        }

        return GP_OK;
}

#define GFD_BUFSIZE 19432
static char *digita_file_get (Camera *camera, const char *folder, 
			      const char *filename, int thumbnail, int *size)
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

int camera_file_get (Camera *camera, const char *folder, const char *filename,
                     CameraFileType type, CameraFile *file)
{
        struct digita_device *dev = camera->camlib_data;
	unsigned char *data;
	unsigned int *ints, width, height;
	char ppmhead[64]; 
        int buflen;

        if (!dev)
                return GP_ERROR;

        if (folder[0] == '/')
                folder++;

	switch (type) {
	case GP_FILE_TYPE_NORMAL:
	        data = digita_file_get(camera, folder, filename, 0, &buflen);
		break;
	case GP_FILE_TYPE_PREVIEW:
		data = digita_file_get(camera, folder, filename, 1, NULL);
		break;
	default:
		return (GP_ERROR_NOT_SUPPORTED);
	}
        if (!data)
                return GP_ERROR;

	gp_file_set_name (file, filename);
	gp_file_set_data_and_size (file, data, buflen);

	switch (type) {
	case GP_FILE_TYPE_NORMAL:
		gp_file_set_mime_type (file, GP_MIME_JPEG);
		break;
	case GP_FILE_TYPE_PREVIEW:
		ints = (unsigned int *)data;
		width = ntohl(ints[2]);
		height = ntohl(ints[1]);

		fprintf(stderr, "digita: picture size %dx%d\n", width, height);
		fprintf(stderr, "digita: data size %d\n", ntohl(ints[0]));

		sprintf (ppmhead, "P6\n%i %i\n255\n", width, height);
		gp_file_set_mime_type (file, GP_MIME_RAW);
		gp_file_set_header (file, ppmhead);
		gp_file_set_width_and_height (file, width, height);
		gp_file_set_conversion_method (file,
					GP_FILE_CONVERSION_METHOD_JOHANNES);
		gp_file_convert (file, GP_MIME_PPM);
		break;
	default:
		return (GP_ERROR_NOT_SUPPORTED);
	}

        return GP_OK;
}

int camera_summary(Camera *camera, CameraText *summary)
{
        struct digita_device *dev = camera->camlib_data;
        int taken;

        if (!dev)
                return GP_ERROR;

        if (digita_get_storage_status(dev, &taken, NULL, NULL) < 0)
                return 0;

        sprintf(summary->text, _("Number of pictures: %d"), taken);

        return GP_OK;
}

int camera_manual(Camera *camera, CameraText *manual)
{
        strcpy(manual->text, _("Manual Not Available"));

        return GP_ERROR;
}

int camera_about(Camera *camera, CameraText *about)
{
        strcpy(about->text, _("Digita\n" \
                "Johannes Erdfelt <johannes@erdfelt.com>\n" \
                "VA Linux Systems, Inc"));

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

int camera_init(Camera *camera)
{
        struct digita_device *dev;
        int ret = 0;

        if (!camera)
                return GP_ERROR;

        /* First, set up all the function pointers */
        camera->functions->exit         = camera_exit;
        camera->functions->folder_list_folders  = camera_folder_list_folders;
        camera->functions->folder_list_files    = camera_folder_list_files;
        camera->functions->file_get     = camera_file_get;
        camera->functions->summary      = camera_summary;
        camera->functions->manual       = camera_manual;
        camera->functions->about        = camera_about;

        gp_debug_printf (GP_DEBUG_LOW, "digita", "Initializing the camera\n");

        dev = malloc(sizeof(*dev));
        if (!dev) {
                fprintf(stderr, "Couldn't allocate digita device\n");
                return GP_ERROR;
        }
        memset((void *)dev, 0, sizeof(*dev));

        switch (camera->port->type) {
        case GP_PORT_USB:
                ret = digita_usb_open(dev, camera);
                break;
        case GP_PORT_SERIAL:
                ret = digita_serial_open(dev, camera);
                break;
        default:
                fprintf(stderr, "Unknown port type %d\n", camera->port->type);
                return GP_ERROR;
        }

        if (ret < 0) {
                fprintf(stderr, "Couldn't open digita device\n");
                return GP_ERROR;
        }

        camera->camlib_data = dev;

        return GP_OK;
}

