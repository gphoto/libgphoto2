/* gphoto2-core.c
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

#include <config.h>
#include "gphoto2-core.h"

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include "gphoto2-abilities.h"
#include "gphoto2-setting.h"
#include "gphoto2-result.h"
#include "gphoto2-abilities-list.h"
#include "gphoto2-camera.h"
#include "gphoto2-debug.h"

#define CHECK_NULL(r)        {if (!(r)) return (GP_ERROR_BAD_PARAMETERS);}
#define CHECK_RESULT(result) {int r = (result); if (r < 0) return (r);}
#define CHECK_INIT           {if (!gal) CHECK_RESULT (gp_init ());}

/* Camera List */
CameraAbilitiesList *gal = NULL;

static int
gp_init (void)
{
        char buf[1024];
        int count;

        gp_debug_printf (GP_DEBUG_LOW, "core", "Initializing GPhoto...");

	/* Check if we are already initialized */
        if (gal)
                return (GP_OK);

        /* Make sure the directories are created */
        gp_debug_printf (GP_DEBUG_LOW, "core", "Creating $HOME/.gphoto");
#ifdef WIN32
        GetWindowsDirectory (buf, 1024);
        strcat (buf, "\\gphoto");
#else
        sprintf (buf, "%s/.gphoto", getenv ("HOME"));
#endif
        (void)GP_SYSTEM_MKDIR (buf);

        /* Load settings */
        gp_debug_printf (GP_DEBUG_LOW, "core", "Trying to load settings...");
        load_settings();

	/* What cameras do we support? */
	CHECK_RESULT (gp_abilities_list_new (&gal));
	CHECK_RESULT (gp_abilities_list_load (gal)); 

        gp_debug_printf (GP_DEBUG_LOW, "core", "Following cameras were found:");
        CHECK_RESULT (count = gp_abilities_list_count (gal));
        if (count)
                gp_abilities_list_dump_libs (gal);
        else
                gp_debug_printf (GP_DEBUG_LOW, "core","\tNone");

        return (GP_OK);
}

int
gp_exit (void)
{
        CHECK_RESULT (gp_abilities_list_free (gal));
        gal = NULL;

        return (GP_OK);
}

int
gp_autodetect (CameraList *list)
{
	CHECK_INIT;

	return (gp_abilities_list_detect (gal, list));
}

int
gp_camera_count (void)
{
        CHECK_INIT;

        return (gp_abilities_list_count (gal));
}

int
gp_camera_name (int camera_number, const char **camera_name)
{
	CHECK_INIT;

        return (gp_abilities_list_get_model (gal, camera_number, camera_name));
}

int
gp_camera_abilities (int camera_number, CameraAbilities *abilities)
{
	CHECK_INIT;

        return (gp_abilities_list_get_abilities (gal, camera_number,abilities));
}

int
gp_camera_abilities_by_name (const char *camera_name,
                             CameraAbilities *abilities)
{
        int x;

	CHECK_INIT;

        CHECK_RESULT (x = gp_abilities_list_lookup_model (gal, camera_name));
        return (gp_camera_abilities (x, abilities));
}
