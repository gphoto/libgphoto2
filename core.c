/* core.c
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

#include "gphoto2-core.h"
#include "gphoto2-library.h"

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
#define CHECK_INIT           {if (!gp_is_initialized ()) CHECK_RESULT (gp_init ());}

/* Camera List */
CameraAbilitiesList *gal;

static int have_initted = 0;

static int
is_library (char *library_filename)
{
        int ret = GP_OK;
        void *lh;
        CameraLibraryIdFunc id;

        lh = GP_SYSTEM_DLOPEN (library_filename);
        if (!lh)
                return (GP_ERROR);

        id = GP_SYSTEM_DLSYM (lh, "camera_id");
        GP_SYSTEM_DLCLOSE (lh);
        if (!id)
                ret = GP_ERROR;

        return (ret);
}

static int
load_camera_list (char *library_filename)
{
        CameraText id;
        void *lh;
        CameraLibraryAbilitiesFunc load_camera_abilities;
        CameraLibraryIdFunc load_camera_id;
        int x, old_count, new_count, result;

	/* Try to open the library */
	lh = GP_SYSTEM_DLOPEN(library_filename);
	if (!lh)
		return (GP_ERROR);

        /* Make sure the camera hasn't been loaded yet */
        load_camera_id = GP_SYSTEM_DLSYM (lh, "camera_id");
        load_camera_id(&id);
        gp_debug_printf(GP_DEBUG_LOW, "core", "\t library id: %s", id.text);
        result = gp_abilities_list_lookup_id (gal, id.text);
        if (result >= 0) {
                GP_SYSTEM_DLCLOSE (lh);
                return (GP_ERROR);
        }

        /* load in the camera_abilities function */
        load_camera_abilities = GP_SYSTEM_DLSYM (lh, "camera_abilities");
        old_count = gp_abilities_list_count (gal);
        if (old_count < 0) {
                GP_SYSTEM_DLCLOSE (lh);
                return (old_count);
        }

        result = load_camera_abilities (gal);
        GP_SYSTEM_DLCLOSE (lh);
        if (result != GP_OK)
                return result;

        new_count = gp_abilities_list_count (gal);
        if (new_count < 0)
                return (new_count);

        /* Copy in the core-specific information */
        for (x = old_count; x < new_count; x++) {
                gp_abilities_list_set_id (gal, x, id.text);
                gp_abilities_list_set_library (gal, x,
                                               library_filename);
        }

        return (GP_OK);
}

static int
load_cameras_search (char *directory)
{
        GP_SYSTEM_DIR d;
        GP_SYSTEM_DIRENT de;
        char buf[1024];

        gp_debug_printf(GP_DEBUG_LOW, "core","Trying to load camera libraries in:");
        gp_debug_printf(GP_DEBUG_LOW, "core","\t%s", directory);

        /* Look for available camera libraries */
        d = GP_SYSTEM_OPENDIR(directory);
        if (!d) {
                gp_debug_printf(GP_DEBUG_LOW, "core", "couldn't open %s", directory);
                return GP_ERROR_DIRECTORY_NOT_FOUND;
        }

        do {
           /* Read each entry */
           de = GP_SYSTEM_READDIR(d);
           if (de) {
                sprintf(buf, "%s%c%s", directory, GP_SYSTEM_DIR_DELIM, GP_SYSTEM_FILENAME(de));
                gp_debug_printf(GP_DEBUG_LOW, "core", "\tis %s a library? ", buf);

                /* Don't try to open ".*" */
                if (*buf && (buf[0] == '.'))
                        continue;

                /* Try to open the library */
                if (is_library (buf) == GP_OK) {
                        gp_debug_printf(GP_DEBUG_LOW, "core", "yes");
                        load_camera_list (buf);
                   } else {
                        gp_debug_printf(GP_DEBUG_LOW, "core", "no. reason: %s", GP_SYSTEM_DLERROR ());
                }
           }
        } while (de);

        return (GP_OK);
}

static int
load_cameras (void)
{
        /* Where should we search for camera libraries? */

        /* The installed camera library directory */
        CHECK_RESULT (load_cameras_search (CAMLIBS));

        /* Current directory */
        /* load_cameras_search("."); */

        /* Sort the camera list */
        CHECK_RESULT (gp_abilities_list_sort (gal));

        return GP_OK;
}

static int
gp_init (void)
{
        char buf[1024];
        int count;

        gp_debug_printf (GP_DEBUG_LOW, "core", "Initializing GPhoto...");

        if (have_initted)
                return (GP_OK);

        /* Initialize the globals */
        CHECK_RESULT (gp_abilities_list_new (&gal));

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

        gp_debug_printf (GP_DEBUG_LOW, "core", "Trying to load libraries...");
        CHECK_RESULT (load_cameras ());

        gp_debug_printf (GP_DEBUG_LOW, "core", "Following cameras were found:");
        CHECK_RESULT (count = gp_abilities_list_count (gal));
        if (count)
                gp_abilities_list_dump_libs (gal);
        else
                gp_debug_printf (GP_DEBUG_LOW, "core","\tNone");

        have_initted = 1;
        return (GP_OK);
}

static int
gp_is_initialized (void)
{
        return have_initted;
}

int
gp_exit (void)
{
        CHECK_RESULT (gp_abilities_list_free (gal));
        gal = NULL;

	have_initted = 0;

        return (GP_OK);
}

int
gp_autodetect (CameraList *list)
{
        int x, count, v, p;
        gp_port *dev;
        const char *m;

        CHECK_NULL (list);
        CHECK_INIT;

        list->count = 0;

        CHECK_RESULT (count = gp_abilities_list_count (gal));

        if (gp_port_new (&dev, GP_PORT_USB) != GP_OK)
                return (GP_ERROR_NO_CAMERA_FOUND);


        /* GP_OK == 0 */
	gp_debug_printf (GP_DEBUG_LOW, "core", "Auto-detecting cameras...");
        for (x = 0; x < count; x++) {
		if ((gp_abilities_list_get_vendor_and_product (gal, x, &v, &p)
							      == GP_OK) &&
		    (gp_port_usb_find_device (dev, v, p)      == GP_OK) &&
		    (gp_abilities_list_get_model (gal, x, &m) == GP_OK)) {
			gp_debug_printf (GP_DEBUG_LOW, "core",
					 "Found '%s' (%i,%i)", m, v, p);
	                gp_list_append (list, m, "usb:");
		}
	}

        gp_port_free (dev);

        if (list->count)
                return GP_OK;

        return GP_ERROR_NO_CAMERA_FOUND;
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
