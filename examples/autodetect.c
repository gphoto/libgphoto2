#include <stdio.h>
#include <string.h>

#include <gphoto2/gphoto2-camera.h>

#include "samples.h"

static GPPortInfoList		*portinfolist = NULL;
static CameraAbilitiesList	*abilities = NULL;

/*
 * This detects all currently attached cameras and returns
 * them in a list. It avoids the generic usb: entry.
 *
 * This function does not open nor initialize the cameras yet.
 */
int
sample_autodetect (CameraList *list, GPContext *context) {
	gp_list_reset (list);
        return gp_camera_autodetect (list, context);
}

/*
 * This function opens a camera depending on the specified model and port.
 */
int
sample_open_camera (Camera ** camera, const char *model, const char *port, GPContext *context) {
	int		ret, m, p;
	CameraAbilities	a;
	GPPortInfo	pi;

	ret = gp_camera_new (camera);
	if (ret < GP_OK) return ret;

	if (!abilities) {
		/* Load all the camera drivers we have... */
		ret = gp_abilities_list_new (&abilities);
		if (ret < GP_OK) return ret;
		ret = gp_abilities_list_load (abilities, context);
		if (ret < GP_OK) return ret;
	}

	/* First lookup the model / driver */
        m = gp_abilities_list_lookup_model (abilities, model);
	if (m < GP_OK) return ret;
        ret = gp_abilities_list_get_abilities (abilities, m, &a);
	if (ret < GP_OK) return ret;
        ret = gp_camera_set_abilities (*camera, a);
	if (ret < GP_OK) return ret;

	if (!portinfolist) {
		/* Load all the port drivers we have... */
		ret = gp_port_info_list_new (&portinfolist);
		if (ret < GP_OK) return ret;
		ret = gp_port_info_list_load (portinfolist);
		if (ret < 0) return ret;
		ret = gp_port_info_list_count (portinfolist);
		if (ret < 0) return ret;
	}

	/* Then associate the camera with the specified port */
        p = gp_port_info_list_lookup_path (portinfolist, port);
        switch (p) {
        case GP_ERROR_UNKNOWN_PORT:
                fprintf (stderr, "The port you specified "
                        "('%s') can not be found. Please "
                        "specify one of the ports found by "
                        "'gphoto2 --list-ports' and make "
                        "sure the spelling is correct "
                        "(i.e. with prefix 'serial:' or 'usb:').",
                                port);
                break;
        default:
                break;
        }
        if (p < GP_OK) return p;

        ret = gp_port_info_list_get_info (portinfolist, p, &pi);
        if (ret < GP_OK) return ret;
        ret = gp_camera_set_port_info (*camera, pi);
        if (ret < GP_OK) return ret;
	return GP_OK;
}
