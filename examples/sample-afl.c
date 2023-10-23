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

		if (!strlen(newfolder)) continue;

		buf = malloc (strlen(folder) + 1 + strlen(newfolder) + 1);
		strcpy(buf, folder);
		if (strcmp(folder,"/"))		/* avoid double / */
			strcat(buf, "/");
		strcat(buf, newfolder);

		fprintf(stderr,"newfolder=%s\n", newfolder);

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

	/* get file */
	gp_file_new (&file);
	ret = gp_camera_file_get (camera, folder, newfile, GP_FILE_TYPE_NORMAL, file, context);
	if ((ret != GP_OK) && (ret != GP_ERROR_NOT_SUPPORTED)) {
		gp_list_free (list);
		printf ("Could not get file.\n");
		return ret;
	}
	gp_file_unref (file);
	/* get preview */
	gp_file_new (&file);
	ret = gp_camera_file_get (camera, folder, newfile, GP_FILE_TYPE_PREVIEW, file, context);
	if ((ret != GP_OK) && (ret != GP_ERROR_NOT_SUPPORTED)) {
		gp_list_free (list);
		printf ("Could not get file preview.\n");
		return ret;
	}
	gp_file_unref (file);
	/* get exif */
	gp_file_new (&file);
	ret = gp_camera_file_get (camera, folder, newfile, GP_FILE_TYPE_EXIF, file, context);
	if ((ret != GP_OK) && (ret != GP_ERROR_NOT_SUPPORTED)) {
		gp_list_free (list);
		printf ("Could not get file preview.\n");
		return ret;
	}
	gp_file_unref (file);
	/* Trigger the ptp things */
	gp_file_new (&file);
	ret = gp_camera_file_get (camera, folder, newfile, GP_FILE_TYPE_METADATA, file, context);
	if ((ret != GP_OK) && (ret != GP_ERROR_NOT_SUPPORTED)) {
		gp_list_free (list);
		printf ("Could not get file metadata.\n");
		return ret;
	}
	gp_file_unref (file);
	if (foundfile) *foundfile = 1;
	gp_list_free (list);
	return GP_OK;
}


int main(int argc, char **argv) {
	Camera		*camera = NULL;
	int		ret, storagecnt;
	CameraStorageInformation	*storageinfo;
	GPPortInfoList	*gpinfolist;
	GPContext	*context;
	CameraWidget	*rootwidget;
	char		buf[200];
	const char	*name;
	CameraText		summary;
	CameraFile		*file;
	/*CameraFilePath		path;*/
	CameraList		*list;
	CameraAbilitiesList      *abilities = NULL;


        gp_log_add_func(GP_LOG_DEBUG, errordumper, NULL);

	context = sample_create_context (); /* see context.c */

	strcpy(buf,"usb:");
	if (argc > 1)
		strcat (buf, argv[1]);

	fprintf(stderr,"setting path %s.\n", buf);

	gp_port_info_list_new (&gpinfolist);
	ret = gp_port_info_list_load (gpinfolist);
	if (ret < GP_OK) return ret;

	ret = gp_port_info_list_lookup_path (gpinfolist, buf);
	if (ret < GP_OK) return ret;

        /* Detect all the cameras that can be autodetected... */
        ret = gp_list_new (&list);
        if (ret < GP_OK) return 1;

	/* Load all the camera drivers we have... */
	ret = gp_abilities_list_new (&abilities);
	if (ret < GP_OK) return ret;
	ret = gp_abilities_list_load (abilities, context);
	if (ret < GP_OK) return ret;
	ret = gp_abilities_list_detect (abilities, gpinfolist, list, context);
	if (ret < GP_OK) return ret;

	gp_port_info_list_free (gpinfolist);
	gp_abilities_list_free (abilities);

	fprintf(stderr, "detect list has count %d\n", gp_list_count(list));

	ret = gp_list_get_name(list, 0, &name);
	if (ret < GP_OK) goto out;

	fprintf(stderr, "name is %s\n", name);

	ret = sample_open_camera (&camera, name, buf, context);
        if (ret < GP_OK) {
		fprintf(stderr,"camera %s at %s not found.\n", name, buf);
		gp_list_free (list);
		goto out;
	}
	gp_list_free (list);

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
	if ((ret != GP_OK) && (ret != GP_ERROR_NOT_SUPPORTED)) {
		printf ("Could not get summary.\n");
		goto out;
	}

#if 1
	ret = gp_camera_get_config (camera, &rootwidget, context);
	if (ret < GP_OK) {
		fprintf (stderr,"Could not get config.\n");
		goto out;
	}
	gp_widget_free (rootwidget);
#endif
	printf ("OK, %s\n", summary.text);

	ret = gp_camera_get_storageinfo (camera, &storageinfo, &storagecnt, context);
	if ((ret != GP_OK) && (ret != GP_ERROR_NOT_SUPPORTED)) {
		printf ("Could not get storage info.\n");
		goto out;
	}
	free(storageinfo);

	ret = gp_camera_trigger_capture (camera, context);
	if ((ret != GP_OK) && (ret != GP_ERROR_NOT_SUPPORTED)) {
		printf ("Could not trigger capture.\n");
		goto out;
	}

	while (1) {
		CameraEventType evttype;
		void *data = NULL;

		ret = gp_camera_wait_for_event(camera, 1, &evttype, &data, context);
		if (ret < GP_OK) break;
		if (data) free (data);
		if (evttype == GP_EVENT_TIMEOUT) break;
	}

	gp_file_new (&file);
	ret = gp_camera_capture_preview (camera, file, context);
	if ((ret != GP_OK) && (ret != GP_ERROR_NOT_SUPPORTED)) {
		gp_file_free (file);
		printf ("Could not capture preview.\n");
		goto out;
	}
	gp_file_free (file);

#if 0
	/* this gives endless event check loops occasionally ... need review how to do this best */
	ret = gp_camera_capture (camera, GP_CAPTURE_IMAGE, &path, context);
	if ((ret != GP_OK) && (ret != GP_ERROR_NOT_SUPPORTED)) {
		printf ("Could not capture preview.\n");
		goto out;
	}
#endif

	/* AFL PART ENDS HERE */
out:
	gp_camera_exit (camera, context);
	gp_context_unref (context);
	gp_camera_free (camera);
	return 0;
}
