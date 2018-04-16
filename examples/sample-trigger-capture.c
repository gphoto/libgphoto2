/* 
 */

#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <gphoto2/gphoto2.h>

#include "samples.h"

static void
errordumper(GPLogLevel level, const char *domain, const char *str, void *data) {
	fprintf(stderr, "%s\n", str);
}

static struct queue_entry {
	CameraFilePath	path;
	int offset;
} *queue = NULL;
static int nrofqueue=0;
static int nrdownloads=0;

static const char *buffer;

static int
wait_event_and_download (Camera *camera, int waittime, GPContext *context) {
	CameraEventType	evtype;
	CameraFilePath	*path;
	void		*data;
	int		retval;
        struct timeval	start, curtime;

        gettimeofday (&start, NULL);
	data = NULL;
	if (nrofqueue)
		waittime = 10; /* just drain the event queue */

	while (1) {
		unsigned int timediff;

	        gettimeofday (&curtime, NULL);

		timediff = ((curtime.tv_sec - start.tv_sec)*1000)+((curtime.tv_usec - start.tv_usec)/1000);
		if (timediff >= waittime) 
			break;

		retval = gp_camera_wait_for_event(camera, waittime - timediff, &evtype, &data, context);
		if (retval != GP_OK) {
			fprintf (stderr, "return from waitevent in trigger sample with %d\n", retval);
			return retval;
		}
		path = data;
		switch (evtype) {
		case GP_EVENT_CAPTURE_COMPLETE:
			fprintf (stderr, "wait for event CAPTURE_COMPLETE\n");
			break;
		case GP_EVENT_UNKNOWN:
		case GP_EVENT_TIMEOUT:
			break;
		case GP_EVENT_FOLDER_ADDED:
			fprintf (stderr, "wait for event FOLDER_ADDED\n");
			free(data);
			break;
		case GP_EVENT_FILE_CHANGED:
			fprintf (stderr, "wait for event FILE_CHANGED\n");
			free(data);
			break;
		case GP_EVENT_FILE_ADDED:
			fprintf (stderr, "File %s / %s added to queue.\n", path->folder, path->name);
			if (nrofqueue) {
				struct queue_entry *q;
				q = realloc(queue, sizeof(struct queue_entry)*(nrofqueue+1));
				if (!q) return GP_ERROR_NO_MEMORY;
				queue = q;
			} else {
				queue = malloc (sizeof(struct queue_entry));
				if (!queue) return GP_ERROR_NO_MEMORY;
			}
			memcpy (&queue[nrofqueue].path, path, sizeof(CameraFilePath));
			queue[nrofqueue].offset = 0;
			nrofqueue++;
			free(data);
			break;
		}
	}
	if (nrofqueue) {
		unsigned long	size;
		int		fd;
		struct stat	stbuf;
		CameraFile	*file;

		retval = gp_file_new(&file);

		fprintf(stderr,"camera getfile of %s / %s\n",
			queue[0].path.folder,
			queue[0].path.name
		);
		retval = gp_camera_file_get(camera, queue[0].path.folder, queue[0].path.name,
			GP_FILE_TYPE_NORMAL, file, context);
		if (retval != GP_OK) {
			fprintf (stderr,"gp_camera_file_get failed: %d\n", retval);
			gp_file_free (file);
			return retval;
		}

		/* buffer is returned as pointer, not as a copy */
		retval = gp_file_get_data_and_size (file, &buffer, &size);

		if (retval != GP_OK) {
			fprintf (stderr,"gp_file_get_data_and_size failed: %d\n", retval);
			gp_file_free (file);
			return retval;
		}
		if (-1 == stat(queue[0].path.name, &stbuf))
			fd = creat(queue[0].path.name, 0644);
		else
			fd = open(queue[0].path.name, O_RDWR | O_BINARY, 0644);
		if (fd == -1) {
			perror(queue[0].path.name);
			return GP_ERROR;
		}
		if (-1 == lseek(fd, 0, SEEK_SET))
			perror("lseek");
		if (-1 == write (fd, buffer, size)) 
			perror("write");
		close (fd);

		gp_file_free (file); /* Note: this invalidates buffer. */

		fprintf(stderr,"ending download %d, deleting file.\n", nrdownloads);
		retval = gp_camera_file_delete(camera, queue[0].path.folder, queue[0].path.name, context);
		memmove(&queue[0],&queue[1],sizeof(queue[0])*(nrofqueue-1));
		nrofqueue--;
	}
	return GP_OK;
}

int
main(int argc, char **argv) {
	Camera		*camera;
	int		retval, nrcapture = 0;
	struct timeval	tval;
	GPContext 	*context = sample_create_context();

	gp_log_add_func(GP_LOG_ERROR, errordumper, NULL);
	/*gp_log_add_func(GP_LOG_DATA, errordumper, NULL); */
	gp_camera_new(&camera);

	retval = gp_camera_init(camera, context);
	if (retval != GP_OK) {
		printf("gp_camera_init: %d\n", retval);
		exit (1);
	}
	while (1) {
		if ((time(NULL) & 1) == 1)  {
			fprintf(stderr,"triggering capture %d\n", ++nrcapture);
			retval = gp_camera_trigger_capture (camera, context);
			if ((retval != GP_OK) && (retval != GP_ERROR) && (retval != GP_ERROR_CAMERA_BUSY)) {
				fprintf(stderr,"triggering capture had error %d\n", retval);
				break;
			}
			fprintf (stderr, "done triggering\n");
		}
		/*fprintf(stderr,"waiting for events\n");*/
		retval = wait_event_and_download(camera, 100, context);
		if (retval != GP_OK)
			break;
		/*fprintf(stderr,"end waiting for events\n");*/
		gettimeofday (&tval, NULL);
		fprintf(stderr,"loop is at %d.%06d\n", (int)tval.tv_sec,(int)tval.tv_usec);
	}
	gp_camera_exit(camera, context);
	return 0;
}
