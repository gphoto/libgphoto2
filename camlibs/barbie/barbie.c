#include <config.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gphoto2.h>

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

#include "barbie.h"

extern char packet_1[];

static char *models[] = {
        "Barbie",
        "Nick Click",
        "WWF",
        "Hot Wheels",
        NULL
};

int
camera_id (CameraText *id)
{
        strcpy(id->text, "barbie");

        return (GP_OK);
}

int
camera_abilities (CameraAbilitiesList *list)
{
        int x=0;
        CameraAbilities a;

        while (models[x]) {

                /* Fill in the appropriate flags/data */
		memset(&a, 0, sizeof(a));
                strcpy(a.model, models[x]);
		a.status = GP_DRIVER_STATUS_PRODUCTION;
                a.port      = GP_PORT_SERIAL;
                a.speed[0]  = 57600;
                a.speed[1]  = 0;
                a.operations        = GP_OPERATION_NONE;
                a.file_operations   = GP_FILE_OPERATION_PREVIEW;
                a.folder_operations = GP_FOLDER_OPERATION_NONE;

                /* Append it to the list */
                gp_abilities_list_append(list, a);

                x++;
        }
        return (GP_OK);
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
                void *data)
{
        Camera *camera = data;
        int count;

        count = barbie_file_count (camera->port);
        gp_list_populate (list, "mattel%02i.ppm", count);

        return GP_OK;
}

static int
get_file_func (CameraFilesystem *fs, const char *folder, const char *filename,
	       CameraFileType type, CameraFile *file, void *user_data)
{
	Camera *camera = user_data;
        int size, num;
        char *data;

        /* Retrieve the number of the photo on the camera */
        num = gp_filesystem_number (camera->fs, "/", filename);

        switch (type) {
        case GP_FILE_TYPE_NORMAL:
                data = barbie_read_picture (camera->port, num, 0, &size);
                break;
        case GP_FILE_TYPE_PREVIEW:
                data = barbie_read_picture (camera->port, num, 1, &size);
                break;
        default:
                return (GP_ERROR_NOT_SUPPORTED);
        }

        if (!data)
                return GP_ERROR;

        gp_file_set_name (file, filename);
        gp_file_set_mime_type (file, "image/ppm");
        gp_file_set_data_and_size (file, data, size);

        return GP_OK;
}

#if 0
int camera_capture (Camera *camera, int capture_type, CameraFilePath *path) {

        char cmd[4], resp[4];

        memcpy(cmd, packet_1, 4);

        cmd[COMMAND_BYTE] = 'G';
        cmd[DATA1_BYTE]   = 0x40;
        if (barbie_exchange(cmd, 4, resp, 4) == 0)
                return (0);

        cmd[COMMAND_BYTE] = 'Y';
        cmd[DATA1_BYTE]   = 0;
        if (barbie_exchange(cmd, 4, resp, 4) == 0)
                return (0);

        return(resp[DATA1_BYTE] == 0? GP_OK: GP_ERROR);

        return (GP_ERROR);
}
#endif

static int
camera_summary (Camera *camera, CameraText *summary)
{
        int num;
        char *firm;

        num = barbie_file_count (camera->port);
        firm = barbie_read_firmware (camera->port);

        sprintf(summary->text, _("Number of pictures: %i\nFirmware Version: %s"), num,firm);

        free(firm);

        return GP_OK;
}

static int
camera_manual (Camera *camera, CameraText *manual)
{
        strcpy (manual->text, _("No manual available."));

        return GP_OK;
}
static int
        camera_about (Camera *camera, CameraText *about)
{
        strcpy (about->text,_("Barbie/HotWheels/WWF\nScott Fritzinger <scottf@unr.edu>\nAndreas Meyer <ahm@spies.com>\nPete Zaitcev <zaitcev@metabyte.com>\n\nReverse engineering of image data by:\nJeff Laing <jeffl@SPATIALinfo.com>\n\nImplemented using documents found on\nthe web. Permission given by Vision."));
        return GP_OK;
}

int
camera_init (Camera *camera)
{
	GPPortSettings settings;
        int res;

        /* First, set up all the function pointers */
        camera->functions->summary      = camera_summary;
        camera->functions->manual       = camera_manual;
        camera->functions->about        = camera_about;

        /* Set up the CameraFilesystem */
        gp_filesystem_set_list_funcs (camera->fs, file_list_func, NULL,
                                      camera);
	gp_filesystem_set_file_funcs (camera->fs, get_file_func, NULL, camera);

	/* Set up the port */
        gp_port_set_timeout (camera->port, 5000);
	gp_port_get_settings (camera->port, &settings);
        settings.serial.speed   = 57600;
        settings.serial.bits    = 8;
        settings.serial.parity  = 0;
        settings.serial.stopbits= 1;
        gp_port_set_settings (camera->port, settings);

        res = barbie_ping (camera->port);
        if (res)
                return (GP_OK);

        return (GP_ERROR);
}
