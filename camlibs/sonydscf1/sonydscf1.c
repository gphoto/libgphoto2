#include <config.h>
#include "sonydscf1.h"

#include <stdio.h>
#include <string.h>

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

#include "command.h"
#include "chotplay.h"

gp_port *dev;

int camera_id (CameraText *id) {

        strcpy(id->text, "sonydscf1-bvl");

        return (GP_OK);
}

int camera_abilities (CameraAbilitiesList *list) {

        //*count = 1;
        CameraAbilities a;

        /* Fill in each camera model's abilities */
        /* Make separate entries for each conneciton type (usb, serial, etc...)
           if a camera supported multiple ways. */
	memset (&a, 0, sizeof(a));
        strcpy(a.model, "Sony DSC-F1");
	a.status = GP_DRIVER_STATUS_EXPERIMENTAL;
        a.port=GP_PORT_SERIAL;
        a.speed[0] = 9600;
        a.speed[1] = 19200;
        a.speed[2] = 38400;
        a.operations        =  GP_OPERATION_NONE;
        a.file_operations   =  GP_FILE_OPERATION_DELETE |
                                GP_FILE_OPERATION_PREVIEW;
        a.folder_operations =  GP_FOLDER_OPERATION_NONE;
        gp_abilities_list_append(list, a);

        return (GP_OK);
}

static int camera_exit (Camera *camera, GPContext *context) {
        if(F1ok())
           return(GP_ERROR);
        return (F1fclose());
}

static int get_file_func (CameraFilesystem *fs, const char *folder,
			  const char *filename, CameraFileType type,
			  CameraFile *file, void *user_data, GPContext *context)
{
	Camera *camera = user_data;
        int num;
	long int size;
	char *data = NULL;
        printf("folder: %s, file: %s\n", folder, filename);
        if(!F1ok())
           return (GP_ERROR);

	gp_file_set_name (file, filename);
	gp_file_set_mime_type (file, "image/jpeg");

        /* Retrieve the number of the photo on the camera */
	num = gp_filesystem_number(camera->fs, "/", filename, context);

	switch (type) {
	case GP_FILE_TYPE_NORMAL:
		size = get_picture (num, &data, JPEG, 0, F1howmany());
		break;
	case GP_FILE_TYPE_PREVIEW:
		size = get_picture (num, &data, JPEG_T, TRUE, F1howmany());
		break;
	default:
		return (GP_ERROR_NOT_SUPPORTED);
	}

        if (!data)
                return GP_ERROR;

	gp_file_set_data_and_size (file, data, size);

        return GP_OK;
}

static int delete_file_func (CameraFilesystem *fs, const char *folder,
			     const char *filename, void *data,
			     GPContext *context)
{
	Camera *camera = data;
        int max, num;

        num = gp_filesystem_number(camera->fs, "/", filename, context);
        max = gp_filesystem_count(camera->fs,folder, context);
        printf("sony dscf1: file delete: %d\n",num);
        if(!F1ok())
           return (GP_ERROR);
        delete_picture(num,max);
        return(GP_OK);
        /*return (F1deletepicture(file_number));*/
}

static int camera_summary (Camera *camera, CameraText *summary, GPContext *context)
{
        /*printf("->camera summary");*/
        int i;
        if(!F1ok())
           return (GP_ERROR);
        get_picture_information(&i,2);
        return (F1newstatus(1, summary->text));
}

static int camera_about (Camera *camera, CameraText *about, GPContext *context)
{
        strcpy(about->text,
_("Sony DSC-F1 Digital Camera Support\nM. Adam Kendall <joker@penguinpub.com>\nBased on the chotplay CLI interface from\nKen-ichi Hayashi\nGphoto2 port by Bart van Leeuwen <bart@netage.nl>"));

        return (GP_OK);
}

static int file_list_func (CameraFilesystem *fs, const char *folder,
			   CameraList *list, void *data, GPContext *context)
{
/*	Camera *camera = data; */
        int count;
        F1ok();
        /*if(F1ok())
           return(GP_ERROR);*/
        count = F1howmany();

        /* Populate the list */
        gp_list_populate(list, "PSN%05i.jpg", count);

        return GP_OK;
}

int camera_init (Camera *camera, GPContext *context) {
        GPPortSettings settings;

        camera->functions->exit         = camera_exit;
        camera->functions->summary      = camera_summary;
        camera->functions->about        = camera_about;

	/* FIXME: This won't work with several frontends. NO GLOBALS PLEASE! */
	dev = camera->port;

	/* Configure the port */
        gp_port_set_timeout (camera->port, 5000);
	gp_port_get_settings (camera->port, &settings);
        settings.serial.bits    = 8;
        settings.serial.parity  = 0;
        settings.serial.stopbits= 1;
        gp_port_set_settings (camera->port, settings);

	/* Set up the filesystem */
	gp_filesystem_set_list_funcs (camera->fs, file_list_func, NULL, camera);
	gp_filesystem_set_file_funcs (camera->fs, get_file_func,
				      delete_file_func, camera);

        return (GP_OK);
}

