/* abilities.c
 *
 * Copyright (C) 2000 Scott Fritzinger
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details. 
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "gphoto2-abilities.h"

#include <stdlib.h>

#include "gphoto2-result.h"
#include "gphoto2-core.h"
#include "gphoto2-debug.h"

#define CHECK_NULL(r)        {if (!(r)) return (GP_ERROR_BAD_PARAMETERS);}
#define CHECK_RESULT(result) {int r = (result); if (r < 0) return (r);}
#define CHECK_MEM(m)         {if (!(m)) return (GP_ERROR_NO_MEMORY);}

int
gp_abilities_new (CameraAbilities **abilities)
{
	CHECK_NULL (abilities);

	CHECK_MEM (*abilities = malloc (sizeof (CameraAbilities)));
	memset (*abilities, 0, sizeof (CameraAbilities));

	return (GP_OK);
}

int
gp_abilities_free (CameraAbilities *abilities)
{
	CHECK_NULL (abilities);

	free (abilities);

	return (GP_OK);
}

int
gp_abilities_dump (CameraAbilities *abilities)
{
	int x = 0;

	CHECK_NULL (abilities);

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
		abilities->file_operations & GP_FILE_OPERATION_PREVIEW? "yes":"no");
	gp_debug_printf(GP_DEBUG_LOW, "core", "File upload support                   : %s", 
		abilities->folder_operations & GP_FOLDER_OPERATION_PUT_FILE? "yes":"no");

	return (GP_OK);
}

int
gp_abilities_clear (CameraAbilities *abilities)
{
	CHECK_NULL (abilities);

	strcpy (abilities->model, "");
	abilities->port = GP_PORT_NONE;
	abilities->speed[0] = 0;
	abilities->operations        = GP_OPERATION_NONE;
	abilities->file_operations   = GP_FILE_OPERATION_NONE;
	abilities->folder_operations = GP_FOLDER_OPERATION_NONE;

	return (GP_OK);
}
