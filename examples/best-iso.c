/* 
 * This program tries various ISOs for best ISO with a shutterspeed limit
 *
 * compile with: gcc -Wall -o best-iso best-iso.c -lgphoto2 -lgphoto2_port
 *
 */

#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <gphoto2/gphoto2.h>

#define DEBUG

static int aperture;
static float shutterspeed;

static void errordumper(GPLogLevel level, const char *domain, const char *str,
                 void *data) {
  fprintf(stdout, "%s\n", str);
}

static void
ctx_error_func (GPContext *context, const char *str, void *data)
{
        fprintf  (stderr, "\n*** Contexterror ***              \n%s\n",str);
        fflush   (stderr);
}

static void
ctx_status_func (GPContext *context, const char *str, void *data)
{
        fprintf  (stderr, "%s\n", str);
        fflush   (stderr);
}

static
GPContext* sample_create_context() {
	GPContext *context;

	/* This is the mandatory part */
	context = gp_context_new();

	/* All the parts below are optional! */
        gp_context_set_error_func (context, ctx_error_func, NULL);
        gp_context_set_status_func (context, ctx_status_func, NULL);

	/* also:
	gp_context_set_cancel_func    (p->context, ctx_cancel_func,  p);
        gp_context_set_message_func   (p->context, ctx_message_func, p);
        if (isatty (STDOUT_FILENO))
                gp_context_set_progress_funcs (p->context,
                        ctx_progress_start_func, ctx_progress_update_func,
                        ctx_progress_stop_func, p);
	 */
	return context;
}
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

/* Gets a string configuration value.
 * This can be:
 *  - A Text widget
 *  - The current selection of a Radio Button choice
 *  - The current selection of a Menu choice
 *
 * Sample (for Canons eg):
 *   get_config_value_string (camera, "owner", &ownerstr, context);
 */
static int
get_config_value_string (Camera *camera, const char *key, char **str, GPContext *context) {
	CameraWidget		*widget = NULL, *child = NULL;
	CameraWidgetType	type;
	int			ret;
	char			*val;

	ret = gp_camera_get_config (camera, &widget, context);
	if (ret < GP_OK) {
		fprintf (stderr, "camera_get_config failed: %d\n", ret);
		return ret;
	}
	ret = _lookup_widget (widget, key, &child);
	if (ret < GP_OK) {
		fprintf (stderr, "lookup widget failed: %d\n", ret);
		goto out;
	}

	/* This type check is optional, if you know what type the label
	 * has already. If you are not sure, better check. */
	ret = gp_widget_get_type (child, &type);
	if (ret < GP_OK) {
		fprintf (stderr, "widget get type failed: %d\n", ret);
		goto out;
	}
	switch (type) {
        case GP_WIDGET_MENU:
        case GP_WIDGET_RADIO:
        case GP_WIDGET_TEXT:
		break;
	default:
		fprintf (stderr, "widget has bad type %d\n", type);
		ret = GP_ERROR_BAD_PARAMETERS;
		goto out;
	}

	/* This is the actual query call. Note that we just
	 * a pointer reference to the string, not a copy... */
	ret = gp_widget_get_value (child, &val);
	if (ret < GP_OK) {
		fprintf (stderr, "could not query widget value: %d\n", ret);
		goto out;
	}
	/* Create a new copy for our caller. */
	*str = strdup (val);
out:
	gp_widget_free (widget);
	return ret;
}


/* Sets a string configuration value.
 * This can set for:
 *  - A Text widget
 *  - The current selection of a Radio Button choice
 *  - The current selection of a Menu choice
 *
 * Sample (for Canons eg):
 *   get_config_value_string (camera, "owner", &ownerstr, context);
 */
static int
set_config_value_string (Camera *camera, const char *key, const char *val, GPContext *context) {
	CameraWidget		*widget = NULL, *child = NULL;
	CameraWidgetType	type;
	int			ret;

	ret = gp_camera_get_config (camera, &widget, context);
	if (ret < GP_OK) {
		fprintf (stderr, "camera_get_config failed: %d\n", ret);
		return ret;
	}
	ret = _lookup_widget (widget, key, &child);
	if (ret < GP_OK) {
		fprintf (stderr, "lookup widget failed: %d\n", ret);
		goto out;
	}

	/* This type check is optional, if you know what type the label
	 * has already. If you are not sure, better check. */
	ret = gp_widget_get_type (child, &type);
	if (ret < GP_OK) {
		fprintf (stderr, "widget get type failed: %d\n", ret);
		goto out;
	}
	switch (type) {
        case GP_WIDGET_MENU:
        case GP_WIDGET_RADIO:
        case GP_WIDGET_TEXT:
		/* This is the actual set call. Note that we keep
		 * ownership of the string and have to free it if necessary.
		 */
		ret = gp_widget_set_value (child, val);
		if (ret < GP_OK) {
			fprintf (stderr, "could not set widget value: %d\n", ret);
			goto out;
		}
		break;
        case GP_WIDGET_TOGGLE: {
		int ival;

		sscanf(val,"%d",&ival);
		ret = gp_widget_set_value (child, &ival);
		if (ret < GP_OK) {
			fprintf (stderr, "could not set widget value: %d\n", ret);
			goto out;
		}
		break;
	}
	default:
		fprintf (stderr, "widget has bad type %d\n", type);
		ret = GP_ERROR_BAD_PARAMETERS;
		goto out;
	}

	/* This stores it on the camera again */
	ret = gp_camera_set_config (camera, widget, context);
	if (ret < GP_OK) {
		fprintf (stderr, "camera_set_config failed: %d\n", ret);
		return ret;
	}
out:
	gp_widget_free (widget);
	return ret;
}


static int
camera_tether(Camera *camera, GPContext *context) {
	int fd, retval;
	CameraFile *file;
	CameraEventType	evttype;
	CameraFilePath	*path;
	void	*evtdata;

	printf("Tethering...\n");

	while (1) {
		retval = gp_camera_wait_for_event (camera, 2000, &evttype, &evtdata, context);
		if (retval != GP_OK)
			return retval;
		switch (evttype) {
		case GP_EVENT_FILE_ADDED:
			path = (CameraFilePath*)evtdata;
			printf("File added on the camera: %s/%s\n", path->folder, path->name);

			fd = open(path->name, O_CREAT | O_WRONLY, 0644);
			retval = gp_file_new_from_fd(&file, fd);
			printf("  Downloading %s...\n", path->name);
			retval = gp_camera_file_get(camera, path->folder, path->name,
				     GP_FILE_TYPE_NORMAL, file, context);

			printf("  Deleting %s on camera...\n", path->name);
			retval = gp_camera_file_delete(camera, path->folder, path->name, context);
			gp_file_free(file);
			break;
		case GP_EVENT_FOLDER_ADDED:
			path = (CameraFilePath*)evtdata;
			printf("Folder added on camera: %s / %s\n", path->folder, path->name);
			break;
		case GP_EVENT_CAPTURE_COMPLETE:
			printf("Capture Complete.\n");
			break;
		case GP_EVENT_TIMEOUT:
			printf("Timeout.\n");
			return GP_OK;
		case GP_EVENT_UNKNOWN:
			if (evtdata) {
				char *val;
				int ret;

				/* printf("Unknown event: %s.\n", (char*)evtdata); */

				/* 3 eos properties hardcoded */
				if (strstr(evtdata,"d103")) {
					ret = get_config_value_string(camera,"iso",&val,context);
					if (ret == GP_OK) {
						printf("ISO is %s\n", val);
						free(val);
					}
				}
				if (strstr(evtdata,"d102")) {
					ret = get_config_value_string(camera,"shutterspeed",&val,context);
					if (ret == GP_OK) {
						if (strchr(val,'/')) {
							int zaehler,nenner;
							sscanf(val,"%d/%d",&zaehler,&nenner);
							shutterspeed = 1.0*zaehler/nenner;
						} else {
							if (!sscanf(val,"%g",&shutterspeed))
								shutterspeed = 0.0;
						}
						printf("Shutterspeed is %s (%g)\n", val, shutterspeed);
						free(val);
					}
				}
				if (strstr(evtdata,"d101")) {
					ret = get_config_value_string(camera,"aperture",&val,context);
					if (ret == GP_OK) {
						float ap;

						sscanf(val,"%g",&ap);
						aperture = 10*ap;
						printf("Aperture is %s (%d)\n", val, aperture);
						free (val);
					}
				}
			} else {
				printf("Unknown event.\n");
			}
			break;
		default:
			printf("Type %d?\n", evttype);
			break;
		}
	}
}

int
main(int argc, char **argv) {
	Camera	*camera;
	int	retval, iso, tries;
	char	buf[20];
	GPContext *context = sample_create_context();
        int	fd;
        CameraFile *file;
        CameraFilePath camera_file_path;

	gp_log_add_func(GP_LOG_ERROR, errordumper, NULL);
	gp_camera_new(&camera);

	retval = gp_camera_init(camera, context);
	if (retval != GP_OK) {
		printf("  camera failed to initialize: %d\n", retval);
		exit (1);
	}

	retval = camera_tether(camera, context);
	if (retval != GP_OK) {
		printf("Tether error: %d\n", retval);
		exit (1);
	}

	iso = 100;

	shutterspeed = 0.0;
	aperture = 0;

#if 1
	gp_file_new (&file);
	retval = gp_camera_capture_preview (camera, file, context);
	if (retval != GP_OK) {
		printf("Preview capture error: %d\n", retval);
		exit (1);
	}
#endif

	while (1) {
		sprintf(buf,"%d",iso);
		printf("Setting ISO to %d\n",iso);

		retval = set_config_value_string(camera, "iso", buf, context);
		if (retval != GP_OK) {
			printf("  failed setting ISO to %d: %d\n", iso, retval);

			/* usually also happens if too dark */
			exit (1);
		}

		printf("eosremoterelease half press\n");
		retval = set_config_value_string(camera,"eosremoterelease", "Press Half", context);
		if (retval != GP_OK) {
			printf("  failed pressing shutter to half: %d\n", retval);
			exit (1);
		}

		retval = camera_tether(camera, context);
		if (retval != GP_OK) {
			printf("Tether error: %d\n", retval);
			exit (1);
		}

		if (shutterspeed < 0.000000001) {
			printf("Camera did not report shutterspeed?\n");
		}

		printf("eosremoterelease release\n");
		retval = set_config_value_string(camera,"eosremoterelease", "Release Half", context);
		if (retval != GP_OK) {
			printf("  failed releasing shutter button from half: %d\n", retval);
			exit (1);
		}

		if (shutterspeed < 30.0) {

			printf("ISO %d gets Aperture %g and Shutterspeed %g\n", iso, aperture/10.0, shutterspeed);

			break;
		}
		/* ISO always goes up in 2 multiplicator steps */
		iso = iso*2;
	}

	/* we can take the picture now */

#if 0

        printf("Capturing.\n");

        retval = gp_camera_capture(camera, GP_CAPTURE_IMAGE, &camera_file_path, context);
        if (retval != GP_OK) {
		printf("  capture failed: %d\n", retval);
		exit(1);
	}

        printf("Pathname on the camera: %s/%s\n", camera_file_path.folder, camera_file_path.name);

        fd = open("capture.jpg", O_CREAT | O_WRONLY, 0644);

        retval = gp_file_new_from_fd(&file, fd);
        if (retval != GP_OK) printf("  gp_file_new failed: %d\n", retval);

        retval = gp_camera_file_get(camera, camera_file_path.folder, camera_file_path.name,
                     GP_FILE_TYPE_NORMAL, file, context);
        if (retval != GP_OK) printf("  file_get failed: %d\n", retval);

        printf("Deleting downloaded image.\n");
        retval = gp_camera_file_delete(camera, camera_file_path.folder, camera_file_path.name,
                        context);
        if (retval != GP_OK) printf("  Retval: %d\n", retval);

        gp_file_free(file);

#endif
#if 1
	/* SAMPLE Capture code with eosremoterelease ... normal gp_camera_capture_image also works. */
	printf("eosremoterelease release\n");
	retval = set_config_value_string(camera,"eosremoterelease", "Immediate", context);

	if (retval != GP_OK) {
		printf("  failed pressing shutter button full: %d\n", retval);
		exit (1);
	}

	retval = set_config_value_string(camera,"eosremoterelease", "Release Full", context);

	if (retval != GP_OK) {
		printf("  failed releasing shutter button full: %d\n", retval);
		exit (1);
	}


	retval = camera_tether(camera, context);
	if (retval != GP_OK) {
		printf("Tether error: %d\n", retval);
		exit (1);
	}
#endif

	tries = 30; /* seconds at most, discounting events. */
	while (tries--) {
		retval = camera_tether(camera, context);
		if (retval != GP_OK) {
			printf("Tether error: %d\n", retval);
			exit (1);
		}
	}

#if 1
	retval = set_config_value_string(camera,"eosviewfinder", "0", context);
	if (retval != GP_OK) {
		printf("setting eosviewfinder off error: %d\n", retval);
		exit (1);
	}
#endif

	gp_camera_exit(camera, context);
	return 0;
}
