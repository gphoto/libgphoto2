#include "samples.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * This function looks up a label or key entry of
 * a configuration widget.
 * The functions descend recursively, so you can just
 * specify the last component.
 */

static int
_lookup_widget(CameraWidget*widget, const char *key, CameraWidget **child) {
	int ret;
	ret = gp_widget_get_child_by_name (widget, key, child);
	if (ret < GP_OK)
		ret = gp_widget_get_child_by_label (widget, key, child);
	return ret;
}

/* calls the Nikon DSLR or Canon DSLR autofocus method. */
int
camera_auto_focus(Camera *camera, GPContext *context) {
	CameraWidget		*widget = NULL, *child = NULL;
	CameraWidgetType	type;
	int			ret,val;

	ret = gp_camera_get_config (camera, &widget, context);
	if (ret < GP_OK) {
		fprintf (stderr, "camera_get_config failed: %d\n", ret);
		return ret;
	}
	ret = _lookup_widget (widget, "autofocusdrive", &child);
	if (ret < GP_OK) {
		fprintf (stderr, "lookup 'autofocusdrive' failed: %d\n", ret);
		goto out;
	}

	/* check that this is a toggle */
	ret = gp_widget_get_type (child, &type);
	if (ret < GP_OK) {
		fprintf (stderr, "widget get type failed: %d\n", ret);
		goto out;
	}
	switch (type) {
        case GP_WIDGET_TOGGLE:
		break;
	default:
		fprintf (stderr, "widget has bad type %d\n", type);
		ret = GP_ERROR_BAD_PARAMETERS;
		goto out;
	}

	ret = gp_widget_get_value (child, &val);
	if (ret < GP_OK) {
		fprintf (stderr, "could not get widget value: %d\n", ret);
		goto out;
	}
	val++;
	ret = gp_widget_set_value (child, &val);
	if (ret < GP_OK) {
		fprintf (stderr, "could not set widget value to 1: %d\n", ret);
		goto out;
	}

	ret = gp_camera_set_config (camera, widget, context);
	if (ret < GP_OK) {
		fprintf (stderr, "could not set config tree to autofocus: %d\n", ret);
		goto out;
	}
out:
	gp_widget_free (widget);
	return ret;
}


/* Manual focusing a camera...
 * xx is -3 / -2 / -1 / 0 / 1 / 2 / 3
 */
int
camera_manual_focus (Camera *camera, int xx, GPContext *context) {
	CameraWidget		*widget = NULL, *child = NULL;
	CameraWidgetType	type;
	int			ret;
	float			rval;
	char			*mval;

	ret = gp_camera_get_config (camera, &widget, context);
	if (ret < GP_OK) {
		fprintf (stderr, "camera_get_config failed: %d\n", ret);
		return ret;
	}
	ret = _lookup_widget (widget, "manualfocusdrive", &child);
	if (ret < GP_OK) {
		fprintf (stderr, "lookup 'manualfocusdrive' failed: %d\n", ret);
		goto out;
	}

	/* check that this is a toggle */
	ret = gp_widget_get_type (child, &type);
	if (ret < GP_OK) {
		fprintf (stderr, "widget get type failed: %d\n", ret);
		goto out;
	}
	switch (type) {
        case GP_WIDGET_RADIO: {
		int choices = gp_widget_count_choices (child);

		ret = gp_widget_get_value (child, &mval);
		if (ret < GP_OK) {
			fprintf (stderr, "could not get widget value: %d\n", ret);
			goto out;
		}
		if (choices == 7) { /* see what Canon has in EOS_MFDrive */
			ret = gp_widget_get_choice (child, xx+4, (const char**)&mval);
			if (ret < GP_OK) {
				fprintf (stderr, "could not get widget choice %d: %d\n", xx+2, ret);
				goto out;
			}
			fprintf(stderr,"manual focus %d -> %s\n", xx, mval);
		}
		ret = gp_widget_set_value (child, mval);
		if (ret < GP_OK) {
			fprintf (stderr, "could not set widget value to 1: %d\n", ret);
			goto out;
		}
		break;
	}
        case GP_WIDGET_RANGE:
		ret = gp_widget_get_value (child, &rval);
		if (ret < GP_OK) {
			fprintf (stderr, "could not get widget value: %d\n", ret);
			goto out;
		}
	
		switch (xx) { /* Range is on Nikon from -32768 <-> 32768 */
		case -3:	rval = -1024;break;
		case -2:	rval =  -512;break;
		case -1:	rval =  -128;break;
		case  0:	rval =     0;break;
		case  1:	rval =   128;break;
		case  2:	rval =   512;break;
		case  3:	rval =  1024;break;

		default:	rval = xx;	break; /* hack */
		}

		fprintf(stderr,"manual focus %d -> %f\n", xx, rval);

		ret = gp_widget_set_value (child, &rval);
		if (ret < GP_OK) {
			fprintf (stderr, "could not set widget value to 1: %d\n", ret);
			goto out;
		}
		break;
	default:
		fprintf (stderr, "widget has bad type %d\n", type);
		ret = GP_ERROR_BAD_PARAMETERS;
		goto out;
	}


	ret = gp_camera_set_config (camera, widget, context);
	if (ret < GP_OK) {
		fprintf (stderr, "could not set config tree to autofocus: %d\n", ret);
		goto out;
	}
out:
	gp_widget_free (widget);
	return ret;
}
