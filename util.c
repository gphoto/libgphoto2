#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <gphoto2.h>

#include "util.h"

int gp_abilities_dump (CameraAbilities *abilities) {

	int x=0;

	gp_debug_printf(GP_DEBUG_LOW, "core", "Abilities for camera                  : %s", 
		abilities->model);
	gp_debug_printf(GP_DEBUG_LOW, "core", "Serial port support                   : %s", 
		SERIAL_SUPPORTED(abilities->port)? "yes":"no");
	gp_debug_printf(GP_DEBUG_LOW, "core", "Parallel port support                 : %s", 
		PARALLEL_SUPPORTED(abilities->port)? "yes":"no");
	gp_debug_printf(GP_DEBUG_LOW, "core", "USB support                           : %s", 
		USB_SUPPORTED(abilities->port)? "yes":"no");
	gp_debug_printf(GP_DEBUG_LOW, "core", "IEEE1394 support                      : %s", 
		IEEE1394_SUPPORTED(abilities->port)? "yes":"no");
	gp_debug_printf(GP_DEBUG_LOW, "core", "Network support                       : %s", 
		NETWORK_SUPPORTED(abilities->port)? "yes":"no");
	if (abilities->speed[0] != 0) {
	gp_debug_printf(GP_DEBUG_LOW, "core", "Transfer speeds supported             :");
        x=0;
        do {
	gp_debug_printf(GP_DEBUG_LOW, "core", "                                      : %i", abilities->speed[x]);
			x++;
		} while (abilities->speed[x]!=0);
	}

        gp_debug_printf(GP_DEBUG_LOW, "core", "Capture choices                       :");
        if (abilities->operations & GP_OPERATION_CAPTURE_IMAGE)
            gp_debug_printf(GP_DEBUG_LOW, "core", "                                      : Image\n");
        if (abilities->operations & GP_OPERATION_CAPTURE_VIDEO)
            gp_debug_printf(GP_DEBUG_LOW, "core", "                                      : Video\n");
        if (abilities->operations & GP_OPERATION_CAPTURE_AUDIO)
            gp_debug_printf(GP_DEBUG_LOW, "core", "                                      : Audio\n");
	if (abilities->operations & GP_OPERATION_CAPTURE_PREVIEW)
	    gp_debug_printf(GP_DEBUG_LOW, "core", "                                      : Preview\n");
        gp_debug_printf(GP_DEBUG_LOW, "core", "Configuration support                 : %s",
                abilities->operations & GP_OPERATION_CONFIG? "yes":"no");
	gp_debug_printf(GP_DEBUG_LOW, "core", "Delete files on camera support        : %s", 
		abilities->file_operations & GP_FILE_OPERATION_DELETE? "yes":"no");
	gp_debug_printf(GP_DEBUG_LOW, "core", "File preview (thumbnail) support      : %s", 
		abilities->file_operations & GP_FILE_OPERATION_PREVIEW? "no":"yes");
	gp_debug_printf(GP_DEBUG_LOW, "core", "File upload support                   : %s", 
		abilities->folder_operations & GP_FOLDER_OPERATION_PUT_FILE? "no":"yes");

	return (GP_OK);
}

int gp_abilities_clear (CameraAbilities *abilities) {

	strcpy(abilities->model, "");
	abilities->port = 0;
	abilities->speed[0] = 0;
	abilities->operations        = GP_OPERATION_NONE;
	abilities->file_operations   = GP_FILE_OPERATION_NONE;
	abilities->folder_operations = GP_FOLDER_OPERATION_NONE;

	return (GP_OK);
}
