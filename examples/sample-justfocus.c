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

int main(int argc, char *argv[]){
        Camera		*camera;
        GPContext	*context = gp_context_new();
        int		retval;
	CameraEventType evttype;
	void    	*evtdata;
	CameraFile	*file;

        gp_camera_new(&camera);
	retval = gp_camera_init(camera, context);
        if(retval != GP_OK)
        {
                printf("Error: %s\n", gp_result_as_string(retval));
                return 1;
        }

	do {
		retval = gp_camera_wait_for_event (camera, 10, &evttype, &evtdata, context);
	} while ((retval == GP_OK) && (evttype != GP_EVENT_TIMEOUT));

	retval = gp_file_new(&file);
	if (retval != GP_OK) {
		printf("gp_file_new: %d\n", retval);
		return 1;
	}
	/*retval = gp_camera_capture_preview(camera, file, context); */
	if (retval != GP_OK) {
		printf("gp_camera_capture_preview: %d\n", retval);
		return 1;
	}
	gp_file_free (file);

        if(argc == 1)
        {
                retval = camera_auto_focus(camera, context, 1);
		if(retval != GP_OK) {
			printf("Error: %s\n", gp_result_as_string(retval));
			return 1;
		}
        }
        else if(argc == 2)
        {
                int value = atoi(argv[1]);
                retval = camera_manual_focus(camera, value, context);
		if(retval != GP_OK) {
			printf("Error: %s\n", gp_result_as_string(retval));
			return 1;
		}
        }
	do {
		retval = gp_camera_wait_for_event (camera, 10, &evttype, &evtdata, context);
	} while ((retval == GP_OK) && (evttype != GP_EVENT_TIMEOUT));

	retval = camera_auto_focus(camera, context, 0);
	if(retval != GP_OK) {
		printf("Error: %s\n", gp_result_as_string(retval));
		return 1;
	}

        gp_camera_exit(camera, context);
        return 0;
}
