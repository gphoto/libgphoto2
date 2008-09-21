#include <stdio.h>
#include <string.h>

#include <gphoto2/gphoto2-camera.h>

#include "samples.h"

static GPPortInfoList		*portinfolist = NULL;
static CameraAbilitiesList	*abilities = NULL;

/*
 * This detects all currently attached cameras.
 */
int
sample_autodetect (CameraList *list, GPContext *context) {
	int			ret, i;
	CameraList		*xlist = NULL;

	ret = gp_list_new (&xlist);
	if (ret < GP_OK) goto out;
	if (!portinfolist) {
		ret = gp_port_info_list_new (&portinfolist);
		if (ret < GP_OK) goto out;
		ret = gp_port_info_list_load (portinfolist);
		if (ret < 0) goto out;
		ret = gp_port_info_list_count (portinfolist);
		if (ret < 0) goto out;
	}
	ret = gp_abilities_list_new (&abilities);
	if (ret < GP_OK) goto out;
	ret = gp_abilities_list_load (abilities, context);
	if (ret < GP_OK) goto out;
        ret = gp_abilities_list_detect (abilities, portinfolist, xlist, context);
	if (ret < GP_OK) goto out;

        ret = gp_list_count (xlist);
	if (ret < GP_OK) goto out;
	for (i=0;i<ret;i++) {
		const char *name, *value;

		gp_list_get_name (xlist, i, &name);
		gp_list_get_value (xlist, i, &value);
		if (!strcmp ("usb:",value)) continue;
		gp_list_append (list, name, value);
	}
out:
	gp_list_free (xlist);
	return gp_list_count(list);
}

int
sample_open_camera (Camera ** camera, const char *model, const char *port) {
	int		ret, m, p;
	CameraAbilities	a;
	GPPortInfo	pi;

	ret = gp_camera_new (camera);
	if (ret < GP_OK) return ret;

	/* First the model / driver */
        m = gp_abilities_list_lookup_model (abilities, model);
	if (m < GP_OK) return ret;
        ret = gp_abilities_list_get_abilities (abilities, m, &a);
	if (ret < GP_OK) return ret;
        ret = gp_camera_set_abilities (*camera, a);
	if (ret < GP_OK) return ret;

	/* Then associate with a port */

        p = gp_port_info_list_lookup_path (portinfolist, port);
        if (ret < GP_OK) return ret;
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
        if (ret < GP_OK) return ret;
        ret = gp_port_info_list_get_info (portinfolist, p, &pi);
        if (ret < GP_OK) return ret;
        ret = gp_camera_set_port_info (*camera, pi);
        if (ret < GP_OK) return ret;
	return GP_OK;
}
