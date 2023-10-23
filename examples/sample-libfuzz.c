/*
 * This is a sample for use by LibFuzzer.
 *
 * How to build:
 * install clang
 * CC="clang" CFLAGS="-fsanitize=address,fuzzer-no-link -O2 -g" ./configure --prefix=/usr --libdir=/usr/lib64 --enable-vusb
 * make
 * sudo make install
 * rm usb1.* from port drivers
 * in examples/
 * clang -fsanitize=address,fuzzer -O2 -g sample-libfuzz.c autodetect.c context.c -lgphoto2 -lgphoto2_port -o fuzzer
 * mkdir CORPUS
 * ./fuzzer -detect_leaks=0 CORPUS/
 *
 * FIXME:
 * - currently this seems to have memory leaks, it slows down and gets more and more memory over time. 
 *   restarting cures it for a while
 * - It crashes on start in 80% of the cases. You might need retry multiple times to start it.
 *   reason is i think the fuzzer creates a bitmap in an area where the loaded camlibs are mapped into after the fact.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <gphoto2/gphoto2-camera.h>
#include <gphoto2/gphoto2-port-log.h>

#include "samples.h"

/* Sample for AFL usage.
 *
 */

void errordumper(GPLogLevel level, const char *domain, const char *str,
                 void *data) {
	/* Do not log ... but let it appear here so we discover debug paths */
	//fprintf(stderr, "%s:%s\n", domain, str);
}
void contexterror (GPContext *context, const char *text, void *data) {
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
		//fprintf (stderr, "Could not list folders.\n");
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

		buf = (char*)malloc (strlen(folder) + 1 + strlen(newfolder) + 1);
		strcpy(buf, folder);
		if (strcmp(folder,"/"))		/* avoid double / */
			strcat(buf, "/");
		strcat(buf, newfolder);

		//fprintf(stderr,"newfolder=%s\n", newfolder);

		ret = recursive_directory (camera, buf, context, &havefile);
		free (buf);
		if (ret != GP_OK) {
			gp_list_free (list);
			//fprintf (stderr, "Failed to recursively list folders.\n");
			return ret;
		}
		if (havefile) /* only look for the first directory with a file */
			break;
	}
	gp_list_reset (list);

	ret = gp_camera_folder_list_files (camera, folder, list, context);
	if (ret < GP_OK) {
		gp_list_free (list);
		//fprintf (stderr, "Could not list files.\n");
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
		//fprintf (stderr, "Could not get file info.\n");
		return ret;
	}

	/* get file */
	gp_file_new (&file);
	ret = gp_camera_file_get (camera, folder, newfile, GP_FILE_TYPE_NORMAL, file, context);
	if ((ret != GP_OK) && (ret != GP_ERROR_NOT_SUPPORTED)) {
		gp_list_free (list);
		//fprintf (stderr, "Could not get file.\n");
		return ret;
	}
	gp_file_unref (file);
	/* get preview */
	gp_file_new (&file);
	ret = gp_camera_file_get (camera, folder, newfile, GP_FILE_TYPE_PREVIEW, file, context);
	if ((ret != GP_OK) && (ret != GP_ERROR_NOT_SUPPORTED)) {
		gp_list_free (list);
		// fprintf (stderr, "Could not get file preview.\n");
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


static GPPortInfoList		*gpinfolist = NULL;

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
	static int			initialized = 0;
	int				ret, storagecnt;
	static Camera			*camera = NULL;
	CameraStorageInformation	*storageinfo;

	GPPortInfo			pi;
	static GPContext		*context = NULL;
	char				buf[200];
	static const char		*name;
	static const char		*port;
	char				*xpath;
	char				tmpfn[200] = { 0 };

	int				fd;
	CameraWidget			*rootwidget;
	CameraText			summary;
	CameraFile			*file;
	/*CameraFilePath		path;*/
	CameraList			*list;

        gp_log_add_func(GP_LOG_DEBUG, errordumper, NULL);

	if (!gpinfolist) {
		gp_port_info_list_new (&gpinfolist);
		ret = gp_port_info_list_load (gpinfolist);
		if (ret < GP_OK) return 0;
	}
	if (!context) {
		context = sample_create_context (); /* see context.c */
		gp_context_set_error_func (context, contexterror, NULL);
	}

	if (!initialized) {
		/* Detect all the cameras that can be autodetected... */
		ret = gp_list_new (&list);
		if (ret < GP_OK) return 0;

		gp_camera_autodetect (list, context);

		//fprintf(stderr, "detect list has count %d\n", gp_list_count(list));
		if (gp_list_count(list) < 1) goto out;

		ret = gp_list_get_name(list, 0, &name);
		if (ret < GP_OK) goto out;
		ret = gp_list_get_value(list, 0, &port);
		if (ret < GP_OK) goto out;
		//fprintf(stderr,"camera %s detected at port %s.\n", name, port);

		ret = sample_open_camera (&camera, name, port, context);
		if (ret < GP_OK) {
			fprintf(stderr,"camera %s not found.\n", name);
			goto out;
		}
		initialized = 1;
	}
	/* code that runs all the time */

	strcpy(tmpfn,"/dev/shm/gphotofuzz.XXXXXX");
	fd = mkstemp(tmpfn);
	write (fd, Data, Size);
	close(fd);

	strcpy(buf,"vusb:"); strcat(buf,tmpfn);

	//fprintf(stderr,"setting path %s.\n", buf);

	ret = gp_port_info_list_lookup_path (gpinfolist, buf);
	if (ret < GP_OK) goto out;
	gp_port_info_list_get_info (gpinfolist, ret, &pi);

	ret = gp_camera_set_port_info (camera, pi);
	if (ret < GP_OK) {
		fprintf(stderr,"portinfo not set %d.\n", ret);
		goto out;
	}

	ret = gp_camera_init (camera, context);
	if (ret < GP_OK) {
		//fprintf(stderr,"Camera not initialized %d.\n", ret);
		goto out;
	}

	ret = recursive_directory(camera, "/", context, NULL);
	if (ret < GP_OK) {
		//fprintf (stderr, "Could not recursive list files.\n");
		goto out;
	}

	ret = gp_camera_get_summary (camera, &summary, context);
	if ((ret != GP_OK) && (ret != GP_ERROR_NOT_SUPPORTED)) {
		//fprintf (stderr, "Could not get summary.\n");
		goto out;
	}

#if 1
	ret = gp_camera_get_config (camera, &rootwidget, context);
	if (ret < GP_OK) {
		//fprintf (stderr,"Could not get config.\n");
		goto out;
	}
	gp_widget_free (rootwidget);
#endif
	//printf ("OK, %s\n", summary.text);

	ret = gp_camera_get_storageinfo (camera, &storageinfo, &storagecnt, context);
	if ((ret != GP_OK) && (ret != GP_ERROR_NOT_SUPPORTED)) {
		//fprintf (stderr, "Could not get storage info.\n");
		goto out;
	}
	free(storageinfo);

	ret = gp_camera_trigger_capture (camera, context);
	if ((ret != GP_OK) && (ret != GP_ERROR_NOT_SUPPORTED)) {
		//fprintf (stderr, "Could not trigger capture.\n");
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
		//fprintf (stderr, "Could not capture preview.\n");
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
	if (camera) gp_camera_exit (camera, context);
	/*gp_context_unref (context);*/
	/*gp_camera_free (camera);*/
	if (tmpfn[0]) unlink (tmpfn);
	return 0;
}
