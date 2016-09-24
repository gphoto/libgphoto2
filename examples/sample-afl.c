#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <gphoto2/gphoto2-camera.h>

#include "samples.h"

/* Sample for AFL usage.
 *
 */

int main(int argc, char **argv) {
	Camera		*camera = NULL;
	int		ret;
	GPContext	*context;
	CameraWidget	*rootwidget;
	GPPortInfo	pi;
	char		buf[200];
	GPPortInfoList	*portinfolist = NULL;
	CameraAbilitiesList      *abilities = NULL;
	CameraAbilities	a;
	CameraText	summary;

	context = sample_create_context (); /* see context.c */

	ret = gp_port_info_list_new (&portinfolist);
	if (ret < GP_OK) {
		fprintf(stderr,"no portlist allocated.\n");
		goto out;
	}
	ret = gp_port_info_list_load (portinfolist);
	if (ret < GP_OK) {
		fprintf(stderr,"no portlist loaded.\n");
		goto out;
	}
	gp_camera_new (&camera);
	strcpy(buf,"usb:");
	if (argc > 1)
		strcat (buf, argv[1]);

        ret = gp_port_info_list_lookup_path (portinfolist, buf);
        if (ret < GP_OK) {
		fprintf(stderr,"path %s not found.\n", buf);
		goto out;
	}
        ret = gp_port_info_list_get_info (portinfolist, ret, &pi);
        if (ret < GP_OK) {
		fprintf(stderr,"port info not gotten.\n");
		goto out;
	}
	ret = gp_camera_set_port_info (camera, pi);
        if (ret < GP_OK) {
		fprintf(stderr,"port info not set in camera.\n");
		goto out;
	}

        /* First lookup the model / driver */

	/* Load all the camera drivers we have... */
	ret = gp_abilities_list_new (&abilities);
	if (ret < GP_OK) {
		fprintf(stderr,"Abilitylist not allocated.\n");
		goto out;
	}
	ret = gp_abilities_list_load (abilities, context);
	if (ret < GP_OK) {
		fprintf(stderr,"Abilitylist not loaded.\n");
		goto out;
	}

        ret = gp_abilities_list_lookup_model (abilities, "USB PTP Class Camera");
        if (ret < GP_OK) {
		fprintf(stderr,"Camera not found.\n");
	}
        ret = gp_abilities_list_get_abilities (abilities, ret, &a);
        if (ret < GP_OK) {
		fprintf(stderr,"Abilities not gotten.\n");
	}
        ret = gp_camera_set_abilities (camera, a);
        if (ret < GP_OK) {
		fprintf(stderr,"Abilities not set in camera.\n");
	}

	ret = gp_camera_init (camera, context);
	if (ret < GP_OK) {
		fprintf(stderr,"No camera auto detected.\n");
		goto out;
	}

	/* AFL PART STARTS HERE */
	ret = gp_camera_get_summary (camera, &summary, context);
	if (ret < GP_OK) {
		printf ("Could not get summary.\n");
		goto out;
	}

#if 0
	ret = gp_camera_get_config (camera, &rootwidget, context);
	if (ret < GP_OK) {
		fprintf (stderr,"Could not get config.\n");
		goto out;
	}
#endif
	printf ("OK, %s\n", summary.text);

	/* AFL PART ENDS HERE */
out:
	gp_abilities_list_free (abilities);
	gp_port_info_list_free (portinfolist);
	gp_camera_exit (camera, context);
	gp_camera_free (camera);
	return 0;
}
