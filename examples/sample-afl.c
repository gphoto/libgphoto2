#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <gphoto2/gphoto2-camera.h>

#include "samples.h"

/* Sample for AFL usage.
 *
 */

static void errordumper(GPLogLevel level, const char *domain, const char *str,
                 void *data) {
	/* Do not log ... but let it appear here so we discover debug paths */
	fprintf(stderr, "%s:%s\n", domain, str);
}

static int 
recursive_directory(Camera *camera, const char *folder, GPContext *context, int *foundfile) {
	int		i, ret;
	CameraList	*list;
	const char	*newfile;
	CameraFileInfo	fileinfo;
	CameraFile	*file;

	ret = gp_list_new (&list);
	if (ret < GP_OK) {
		printf ("Could not allocate list.\n");
		return ret;
	}

	ret = gp_camera_folder_list_folders (camera, folder, list, context);
	if (ret < GP_OK) {
		printf ("Could not list folders.\n");
		gp_list_free (list);
		return ret;
	}
	gp_list_sort (list);

	for (i = 0; i < gp_list_count (list); i++) {
		const char *newfolder;
		char *buf;
		int havefile = 0;

		gp_list_get_name (list, i, &newfolder);

		buf = malloc (strlen(folder) + 1 + strlen(newfolder) + 1);
		strcpy(buf, folder);
		if (strcmp(folder,"/"))		/* avoid double / */
			strcat(buf, "/");
		strcat(buf, newfolder);

		ret = recursive_directory (camera, buf, context, &havefile);
		free (buf);
		if (ret != GP_OK) {
			gp_list_free (list);
			printf ("Failed to recursively list folders.\n");
			return ret;
		}
		if (havefile) /* only look for the first directory with a file */
			break;
	}
	gp_list_reset (list);

	ret = gp_camera_folder_list_files (camera, folder, list, context);
	if (ret < GP_OK) {
		gp_list_free (list);
		printf ("Could not list files.\n");
		return ret;
	}
	gp_list_sort (list);
	if (gp_list_count (list) <= 0) {
		gp_list_free (list);
		return GP_OK;
	}

	gp_list_get_name (list, 0, &newfile); /* only entry 0 needed */
	ret = gp_camera_file_get_info (camera, folder, newfile, &fileinfo, context);
	if (ret != GP_OK) {
		gp_list_free (list);
		printf ("Could not get file info.\n");
		return ret;
	}

	/* Trigger the ptp things */
	gp_file_new (&file);
	ret = gp_camera_file_get (camera, folder, newfile, GP_FILE_TYPE_METADATA, file, context);
	if ((ret != GP_OK) && (ret != GP_ERROR_NOT_SUPPORTED)) {
		gp_list_free (list);
		printf ("Could not get file metadata.\n");
		return ret;
	}
	gp_file_free (file);
	if (foundfile) *foundfile = 1;
	gp_list_free (list);
	return GP_OK;
}


int main(int argc, char **argv) {
	Camera		*camera = NULL;
	int		ret;
	GPContext	*context;
	CameraWidget	*rootwidget;
	char		buf[200];
	CameraText	summary;

        gp_log_add_func(GP_LOG_DEBUG, errordumper, NULL);

	context = sample_create_context (); /* see context.c */

	strcpy(buf,"usb:");
	if (argc > 1)
		strcat (buf, argv[1]);

	fprintf(stderr,"setting path %s.\n", buf);

	ret = sample_open_camera (&camera, "USB PTP Class Camera", buf, context);
        if (ret < GP_OK) {
		fprintf(stderr,"camera %s not found.\n", buf);
		goto out;
	}

	ret = gp_camera_init (camera, context);
	if (ret < GP_OK) {
		fprintf(stderr,"No camera auto detected.\n");
		goto out;
	}

	/* AFL PART STARTS HERE */

	ret = recursive_directory(camera, "/", context, NULL);
	if (ret < GP_OK) {
		printf ("Could not recursive list files.\n");
		goto out;
	}

	ret = gp_camera_get_summary (camera, &summary, context);
	if (ret < GP_OK) {
		printf ("Could not get summary.\n");
		goto out;
	}

#if 1
	ret = gp_camera_get_config (camera, &rootwidget, context);
	if (ret < GP_OK) {
		fprintf (stderr,"Could not get config.\n");
		goto out;
	}
#endif
	printf ("OK, %s\n", summary.text);
	while (1) {
		CameraEventType evttype;
		void *data = NULL;

		ret = gp_camera_wait_for_event(camera, 1, &evttype, &data, context);
		if (ret < GP_OK) break;
		if (data) free (data);
		if (evttype == GP_EVENT_TIMEOUT) break;
	}

	/* AFL PART ENDS HERE */
out:
	gp_camera_exit (camera, context);
	gp_camera_free (camera);
	return 0;
}
