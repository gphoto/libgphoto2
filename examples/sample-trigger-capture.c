/* 
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

#include "samples.h"

static void
errordumper(GPLogLevel level, const char *domain, const char *str, void *data) {
	fprintf(stderr, "%s\n", str);
}

static void
capture_to_memory(Camera *camera, GPContext *context, const char **ptr, unsigned long int *size) {
	int retval;
	CameraFile *file;
	CameraFilePath camera_file_path;

	printf("Capturing.\n");

	/* NOP: This gets overridden in the library to /capt0000.jpg */
	strcpy(camera_file_path.folder, "/");
	strcpy(camera_file_path.name, "foo.jpg");

	retval = gp_camera_capture(camera, GP_CAPTURE_IMAGE, &camera_file_path, context);
	printf("  Retval: %d\n", retval);

	printf("Pathname on the camera: %s/%s\n", camera_file_path.folder, camera_file_path.name);

	retval = gp_file_new(&file);
	printf("  Retval: %d\n", retval);
	retval = gp_camera_file_get(camera, camera_file_path.folder, camera_file_path.name,
		     GP_FILE_TYPE_NORMAL, file, context);
	printf("  Retval: %d\n", retval);

	gp_file_get_data_and_size (file, ptr, size);

	printf("Deleting.\n");
	retval = gp_camera_file_delete(camera, camera_file_path.folder, camera_file_path.name,
			context);
	printf("  Retval: %d\n", retval);

	gp_file_free(file);
}

static void
capture_to_file(Camera *camera, GPContext *context, char *fn) {
	int fd, retval;
	CameraFile *file;
	CameraFilePath camera_file_path;

	printf("Capturing.\n");

	/* NOP: This gets overridden in the library to /capt0000.jpg */
	strcpy(camera_file_path.folder, "/");
	strcpy(camera_file_path.name, "foo.jpg");

	retval = gp_camera_capture(camera, GP_CAPTURE_IMAGE, &camera_file_path, context);
	printf("  Retval: %d\n", retval);

	printf("Pathname on the camera: %s/%s\n", camera_file_path.folder, camera_file_path.name);

	fd = open(fn, O_CREAT | O_WRONLY, 0644);
	retval = gp_file_new_from_fd(&file, fd);
	printf("  Retval: %d\n", retval);
	retval = gp_camera_file_get(camera, camera_file_path.folder, camera_file_path.name,
		     GP_FILE_TYPE_NORMAL, file, context);
	printf("  Retval: %d\n", retval);

	printf("Deleting.\n");
	retval = gp_camera_file_delete(camera, camera_file_path.folder, camera_file_path.name,
			context);
	printf("  Retval: %d\n", retval);

	gp_file_free(file);
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
			fprintf(stderr,"starting download %d\n", ++nrdownloads);
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
		fprintf(stderr,"done camera readfile size was %d\n", size);
		if (retval != GP_OK) {
			fprintf (stderr,"gp_camera_file_read failed: %d\n", retval);
			return retval;
		}
		if (-1 == stat(queue[0].path.name, &stbuf))
			fd = creat(queue[0].path.name, 0644);
		else
			fd = open(queue[0].path.name, O_RDWR, 0644);
		if (fd == -1) perror(queue[0].path.name);
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
	time_t		lastsec;
	GPContext 	*context = sample_create_context();

	buffer = malloc(buffersize);
	if (!buffer) exit(1);

	gp_log_add_func(GP_LOG_ERROR, errordumper, NULL);
	//gp_log_add_func(GP_LOG_DEBUG, errordumper, NULL);
	gp_camera_new(&camera);

	lastsec = time(NULL);

	/* When I set GP_LOG_DEBUG instead of GP_LOG_ERROR above, I noticed that the
	 * init function seems to traverse the entire filesystem on the camera.  This
	 * is partly why it takes so long.
	 * (Marcus: the ptp2 driver does this by default currently.)
	 */
	retval = gp_camera_init(camera, context);
	if (retval != GP_OK) {
		printf("gp_camera_init: %d\n", retval);
		exit (1);
	}
	while (1) {
		if (lastsec + 3 <= time(NULL))  {
			lastsec = time(NULL);
			//fprintf(stderr,"triggering capture %d\n", ++nrcapture);
			retval = gp_camera_trigger_capture (camera, context);
			if (retval != GP_OK)
				break;
			//fprintf (stderr, "done triggering\n");
		}
		//fprintf(stderr,"waiting for events\n");
		wait_event_and_download(camera, 100, context);
		//fprintf(stderr,"end waiting for events\n");
	}
	gp_camera_exit(camera, context);
	return 0;
}
