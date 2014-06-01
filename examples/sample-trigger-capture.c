/* 
 */

#include <unistd.h>
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

static char *buffer;
static int buffersize = 256*1024;

static int
wait_event_and_download (Camera *camera, int waittime, GPContext *context) {
	CameraEventType	evtype;
	CameraFilePath	*path;
	void		*data;
	int		retval;

	data = NULL;
	if (nrofqueue)
		waittime = 10; /* just drain the event queue */
        retval = gp_camera_wait_for_event(camera, waittime, &evtype, &data, context);
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
		break;
	}
	if (nrofqueue) {
		uint64_t	size = buffersize;
		int		fd;
		struct stat	stbuf;

		if (queue[0].offset == 0)
			fprintf(stderr,"starting download %d (queuelength = %d)\n", ++nrdownloads,
				nrofqueue
			);
		fprintf(stderr,"camera readfile of %s / %s at offset %d\n",
			queue[0].path.folder,
			queue[0].path.name,
			queue[0].offset
		);
		retval = gp_camera_file_read (camera,
			queue[0].path.folder,
			queue[0].path.name,
			GP_FILE_TYPE_NORMAL,
			queue[0].offset,
			buffer,
			&size,
			context
		);
		/*fprintf(stderr,"done camera readfile size was %d\n", size);*/
		if (retval != GP_OK) {
			fprintf (stderr,"gp_camera_file_read failed: %d\n", retval);
			return retval;
		}
		if (-1 == stat(queue[0].path.name, &stbuf))
			fd = creat(queue[0].path.name, 0644);
		else
			fd = open(queue[0].path.name, O_RDWR, 0644);
		if (fd == -1) {
			perror(queue[0].path.name);
			return GP_ERROR;
		}
		if (-1 == lseek(fd, queue[0].offset, SEEK_SET))
			perror("lseek");
		if (-1 == write (fd, buffer, size)) 
			perror("write");
		close (fd);
		queue[0].offset += size;
		if (size != buffersize) {
			fprintf(stderr, "%s/%s is at end of file (read %d of %d bytes)\n",
				queue[0].path.folder, queue[0].path.name, 
				(int)size, buffersize
			);
			fprintf(stderr,"ending download %d, deleting file.\n", nrdownloads);
			retval = gp_camera_file_delete(camera, queue[0].path.folder, queue[0].path.name, context);
			memmove(&queue[0],&queue[1],sizeof(queue[0])*(nrofqueue-1));
			nrofqueue--;
		}
	}
	return GP_OK;
}

int
main(int argc, char **argv) {
	Camera		*camera;
	int		retval, nrcapture = 0;
	struct timeval	tval;
	GPContext 	*context = sample_create_context();

	buffer = malloc(buffersize);
	if (!buffer) exit(1);

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
