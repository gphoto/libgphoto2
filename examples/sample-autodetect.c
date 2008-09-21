#include <stdio.h>
#include <string.h>

#include <gphoto2/gphoto2-camera.h>

GPPortInfoList		*portinfolist = NULL;
CameraAbilitiesList	*abilities = NULL;

/*
 * This detects all currently attached cameras.
 */
static int
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

static int
sample_open_camera (Camera ** camera, const char *model, const char *port) {
	int		ret, m;
	CameraAbilities	a;

	ret = gp_camera_new (camera);
	if (ret < GP_OK) return ret;

	/* First the model / driver */
        m = gp_abilities_list_lookup_model (abilities, model);
	if (m < GP_OK) return ret;
        ret = gp_abilities_list_get_abilities (abilities, m, &a);
	if (ret < GP_OK) return ret;
        ret = gp_camera_set_abilities (*camera, a);
	if (ret < GP_OK) return ret;

	return GP_OK;
}

int main(int argc, char **argv) {
	CameraList	*list;
	Camera		**cams;
	int		i, ret;
	const char	*name, *value;

	ret = gp_list_new (&list);
	if (ret < GP_OK) return 1;
	ret = sample_autodetect (list, NULL);
	cams = calloc(sizeof(Camera*),ret);
        for (i = 0; i < ret; i++) {
                gp_list_get_name  (list, i, &name);
                gp_list_get_value (list, i, &value);
                printf("%-30s %-16s\n", name, value);
		ret = sample_open_camera (&cams[i], name, value);
		if (ret < GP_OK) fprintf(stderr,"Camera %s on port %s failed to open\n", name, value);
        }
	return 0;
}
