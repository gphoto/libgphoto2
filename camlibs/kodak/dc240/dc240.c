#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gphoto2.h>
#include <gphoto2-port.h>

#include "dc240.h"
#include "library.h"

int camera_id (CameraText *id) {

	strcpy(id->text, "kodak-dc240");

	return (GP_OK);
}

int camera_abilities (CameraAbilitiesList *list) {

	CameraAbilities *a;

	a = gp_abilities_new();

	strcpy(a->model, "Kodak DC240");
	a->port     = GP_PORT_SERIAL | GP_PORT_USB;
	a->speed[0] = 9600;
	a->speed[1] = 19200;
	a->speed[2] = 38400;
	a->speed[3] = 57600;
	a->speed[4] = 115200;
	a->speed[5] = 0;
        a->usb_vendor  = 0x040A;
        a->usb_product = 0x0120;
        a->capture  = GP_CAPTURE_IMAGE;
	a->config   = 0;
	a->file_delete  = 1;
	a->file_preview = 1;
	a->file_put = 0;

	gp_abilities_list_append(list, a);

	return (GP_OK);
}

int camera_init (Camera *camera) {

    gp_port_settings settings;
    DC240Data *dd;

    if (!camera)
        return (GP_ERROR);

    dd = (DC240Data*)malloc(sizeof(DC240Data));
    if (!dd)
        return (GP_ERROR);

    /* First, set up all the function pointers */
    camera->functions->id 		= camera_id;
    camera->functions->abilities 	= camera_abilities;
    camera->functions->init 	        = camera_init;
    camera->functions->exit 	        = camera_exit;
    camera->functions->folder_list      = camera_folder_list;
    camera->functions->file_list	= camera_file_list;
    camera->functions->file_get 	= camera_file_get;
    camera->functions->file_get_preview = camera_file_get_preview;
    camera->functions->file_put 	= camera_file_put;
    camera->functions->file_delete 	= camera_file_delete;
    camera->functions->config           = camera_config;
    camera->functions->capture 	        = camera_capture;
    camera->functions->summary	        = camera_summary;
    camera->functions->manual 	        = camera_manual;
    camera->functions->about 	        = camera_about;

    switch (camera->port->type) {
    case GP_PORT_SERIAL:
        dd->dev = gp_port_new(GP_PORT_SERIAL);
        if (!dd->dev) {
            free(dd);
            return (GP_ERROR);
        }
        strcpy(settings.serial.port, camera->port->path);
        settings.serial.speed    = 9600;
        settings.serial.bits     = 8;
        settings.serial.parity   = 0;
        settings.serial.stopbits = 1;
        break;
    case GP_PORT_USB:
        dd->dev = gp_port_new(GP_PORT_USB);
        if (!dd->dev) {
            free(dd);
            return (GP_ERROR);
        }
        if (gp_port_usb_find_device(dd->dev, 0x040A, 0x0120) == GP_ERROR) {
            gp_port_free(dd->dev);
            free (dd);
            return (GP_ERROR);
        }
        settings.usb.inep       = 0x82;
        settings.usb.outep      = 0x01;
        settings.usb.config     = 1;
        settings.usb.interface  = 0;
        settings.usb.altsetting = 0;
        break;
    default:
        return (GP_ERROR);
    }

    if (gp_port_settings_set(dd->dev, settings) == GP_ERROR) {
        gp_port_free(dd->dev);
        free(dd);
        return (GP_ERROR);
    }

    if (gp_port_open(dd->dev) == GP_ERROR) {
        gp_port_free(dd->dev);
        free(dd);
        return (GP_ERROR);
    }

    gp_port_timeout_set(dd->dev, TIMEOUT);

    if (camera->port->type == GP_PORT_SERIAL) {
        /* Reset the camera to 9600 */
        gp_port_send_break(dd->dev, 1);

        /* Wait for it to reset */
        GP_SYSTEM_SLEEP(1500);

        if (dc240_set_speed(dd, camera->port->speed) == GP_ERROR) {
            gp_port_close(dd->dev);
            gp_port_free(dd->dev);
            free(dd);
            return (GP_ERROR);
        }
    }

    /* Open the CF card */
    if (dc240_open(dd) == GP_ERROR) {
        gp_port_close(dd->dev);
        gp_port_free(dd->dev);
        free(dd);
        return (GP_ERROR);
    }

    if (dc240_packet_set_size(dd, HPBS+2) == GP_ERROR) {
        gp_port_close(dd->dev);
        gp_port_free(dd->dev);
        free(dd);
        return (GP_ERROR);
    }

    camera->camlib_data = dd;

    return (GP_OK);
}

int camera_exit (Camera *camera) {

    DC240Data *dd = camera->camlib_data;

    if (!dd)
        return (GP_OK);

    dc240_close(dd);

    if (dd->dev) {
        if (gp_port_close(dd->dev) == GP_ERROR)
                { /* camera did a bad, bad thing */ }
        gp_port_free(dd->dev);
    }
    free(dd);
 
    return (GP_OK);
}

int camera_folder_list	(Camera *camera, CameraList *list, char *folder) {

    DC240Data *dd = camera->camlib_data;

    return (dc240_get_folders(dd, list, folder));
}

int camera_file_list (Camera *camera, CameraList *list, char *folder) {

    DC240Data *dd = camera->camlib_data;

    return (dc240_get_filenames(dd, list, folder));
}

int camera_file_get (Camera *camera, CameraFile *file, char *folder,
                     char *filename) {

    DC240Data *dd = camera->camlib_data;

    return (dc240_file_action(dd, DC240_ACTION_IMAGE, file, folder, filename));
}

int camera_file_get_preview (Camera *camera, CameraFile *file,
			     char *folder, char *filename) {

    DC240Data *dd = camera->camlib_data;

    return (dc240_file_action(dd, DC240_ACTION_PREVIEW, file, folder, filename));
}

int camera_file_put (Camera *camera, CameraFile *file, char *folder) {

    /* DC240Data *dd = camera->camlib_data; */

    return (GP_ERROR);
}

int camera_file_delete (Camera *camera, char *folder, char *filename) {

    DC240Data *dd = camera->camlib_data;

    return (dc240_file_action(dd, DC240_ACTION_DELETE, NULL, folder, filename));
}

int camera_config (Camera *camera) {

    /* DC240Data *dd = camera->camlib_data; */

    return (GP_ERROR);
}

int camera_capture (Camera *camera, CameraFile *file, CameraCaptureInfo *info) {

    DC240Data *dd = camera->camlib_data;

    return (dc240_capture(dd, file));
}

int camera_summary (Camera *camera, CameraText *summary) {

/*	DC240Data *dd = camera->camlib_data; */

	strcpy(summary->text, "No summary information.");

	return (GP_OK);
}

int camera_manual (Camera *camera, CameraText *manual) {

/*	DC240Data *dd = camera->camlib_data; */

	strcpy(manual->text, "No Manual Available");

	return (GP_OK);
}

int camera_about (Camera *camera, CameraText *about) {

/*	DC240Data *dd = camera->camlib_data; */

	strcpy(about->text, 
"Kodak DC240 Camera Library \
Scott Fritzinger <scottf@gphoto.net> \
Camera Library for the Kodak DC240 camera. \
Rewritten and updated for gPhoto2.");

	return (GP_OK);
}
