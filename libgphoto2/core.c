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

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include <gphoto2-abilities.h>
#include <gphoto2-setting.h>
#include <gphoto2-result.h>
#include <gphoto2-abilities-list.h>
#include <gphoto2-camera.h>

/* Debug level */
int             glob_debug = 0;

/* Camera List */
CameraAbilitiesList *glob_abilities_list;

static int have_initted = 0;

static int
is_library (char *library_filename)
{
	int ret = GP_OK;
        void *lh;
	c_id id;
       
	if ((lh = GP_SYSTEM_DLOPEN(library_filename)) == NULL)
                return (GP_ERROR);
	id = (c_id)GP_SYSTEM_DLSYM(lh, "camera_id");
	if (!id)
		ret = GP_ERROR;
	GP_SYSTEM_DLCLOSE(lh);

        return (ret);
}

static int
load_camera_list (char *library_filename)
{
	CameraText id;
	void *lh;
        c_abilities load_camera_abilities;
        c_id load_camera_id;
        int x, old_count;

        /* try to open the library */
        if ((lh = GP_SYSTEM_DLOPEN(library_filename))==NULL) {
                if (glob_debug)
                        perror("core:\tload_camera_list");
                return 0;
        }

        /* check to see if this library has been loaded */
	load_camera_id = (c_id)GP_SYSTEM_DLSYM(lh, "camera_id");
	load_camera_id(&id);
	gp_debug_printf(GP_DEBUG_LOW, "core", "\t library id: %s", id.text);

	for (x=0; x<glob_abilities_list->count; x++) {
		if (strcmp(glob_abilities_list->abilities[x]->id, id.text)==0) {
			GP_SYSTEM_DLCLOSE(lh);
			return (GP_ERROR);
		}
	}

	/* load in the camera_abilities function */
	load_camera_abilities = (c_abilities)GP_SYSTEM_DLSYM(lh, "camera_abilities");
	old_count = glob_abilities_list->count;

        if (load_camera_abilities(glob_abilities_list) != GP_OK) {
                GP_SYSTEM_DLCLOSE(lh);
                return 0;
        }

	/* Copy in the core-specific information */
	for (x=old_count; x<glob_abilities_list->count; x++) {
		strcpy(glob_abilities_list->abilities[x]->id, id.text);
		strcpy(glob_abilities_list->abilities[x]->library, library_filename);
	}

        GP_SYSTEM_DLCLOSE(lh);

	return x;
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
                /* try to open the library */
                if (is_library (buf) == GP_OK) {
                        gp_debug_printf(GP_DEBUG_LOW, "core", "yes");
                        load_camera_list (buf);
                   } else {
			gp_debug_printf(GP_DEBUG_LOW, "core", "no. reason: %s", GP_SYSTEM_DLERROR());
                }
           }
        } while (de);

        return (GP_OK);
}

static int
load_cameras (void)
{
	int x, y, z;
	CameraAbilities *t;

	/* Where should we search for camera libraries? */

	/* The installed camera library directory */
	load_cameras_search (CAMLIBS);

	/* Current directory */
	/* load_cameras_search("."); */

        /* Sort the camera list */
        for (x=0; x<glob_abilities_list->count-1; x++) {
                for (y=x+1; y<glob_abilities_list->count; y++) {
                        z = strcmp(glob_abilities_list->abilities[x]->model, 
				   glob_abilities_list->abilities[y]->model);
                        if (z > 0) {
			  t = glob_abilities_list->abilities[x];
			  glob_abilities_list->abilities[x] = glob_abilities_list->abilities[y];
			  glob_abilities_list->abilities[y] = t;
                        }
                }
        }

	return GP_OK;
}

int gp_init (int debug)
{
        char buf[1024];
        int x, ret;

	gp_debug_printf (GP_DEBUG_LOW, "core", "Initializing GPhoto with "
			 "debug level %i...", debug);

	glob_debug = debug;

        if (have_initted)
		return (GP_OK);

        /* Initialize the globals */
        glob_abilities_list = gp_abilities_list_new();

        /* Make sure the directories are created */
	gp_debug_printf (GP_DEBUG_LOW, "core", "Creating $HOME/.gphoto");
#ifdef WIN32
        GetWindowsDirectory (buf, 1024);
        strcat (buf, "\\gphoto");
#else
        sprintf (buf, "%s/.gphoto", getenv ("HOME"));
#endif
        (void)GP_SYSTEM_MKDIR (buf);

        gp_debug_printf (GP_DEBUG_LOW, "core", "Initializing IO library...");
        ret = gp_port_init (debug);
	if (ret != GP_OK)
		return (ret);

        /* Load settings */
        gp_debug_printf (GP_DEBUG_LOW, "core", "Trying to load settings...");
        load_settings();

        gp_debug_printf (GP_DEBUG_LOW, "core", "Trying to load libraries...");
        load_cameras ();

        gp_debug_printf (GP_DEBUG_LOW, "core", "Following cameras were found:");
        for (x = 0; x < glob_abilities_list->count; x++)
                gp_debug_printf (GP_DEBUG_LOW, "core", "\t\"%s\" uses %s",
                        glob_abilities_list->abilities[x]->model,
                        glob_abilities_list->abilities[x]->library);
        if (glob_abilities_list->count == 0)
                gp_debug_printf (GP_DEBUG_LOW, "core","\tNone");

        have_initted = 1;
        return (GP_OK);
}

int
gp_is_initialized (void)
{
        return have_initted;
}

int
gp_exit (void)
{
        gp_abilities_list_free (glob_abilities_list);

        return (GP_OK);
}

int
gp_autodetect (CameraList *list)
{
	int x = 0, result;
	int vendor, product;
	gp_port *dev;
	
	if (!list)
		return (GP_ERROR_BAD_PARAMETERS);
	
	/* Be nice to frontend-writers... */
	if (!gp_is_initialized ())
		if ((result = gp_init (GP_DEBUG_NONE)) != GP_OK)
			return (result);

	if ((result = gp_port_new (&dev, GP_PORT_USB)) != GP_OK)
		return (result);

	list->count = 0;
	for (x = 0; x < glob_abilities_list->count; x++) {
		vendor = glob_abilities_list->abilities[x]->usb_vendor;
		product = glob_abilities_list->abilities[x]->usb_product;
		if (vendor) {
			if (gp_port_usb_find_device (dev, vendor, product)
					== GP_OK) {
				gp_list_append (list, glob_abilities_list->abilities[x]->model);
				strcpy (list->entry[list->count-1].value, "usb:");
			}
		}
	}
	
	gp_port_free (dev);
	
	if (list->count)
		return GP_OK;
	
	return GP_ERROR_NO_CAMERA_FOUND;
}


void
gp_debug_printf (int level, char *id, char *format, ...)
{
        va_list arg;

        if (glob_debug == GP_DEBUG_NONE)
                return;

        if (glob_debug >= level) {
                fprintf (stderr, "%s: ", id);
                va_start (arg, format);
                vfprintf (stderr, format, arg);
                va_end (arg);
                fprintf (stderr, "\n");
        }
}

int
gp_camera_count (void)
{
	int result;
	
	/* Be nice to frontend-writers... */
	if (!gp_is_initialized ())
		if ((result = gp_init (GP_DEBUG_NONE)) != GP_OK)
			return (result);

	return (glob_abilities_list->count);
}

int
gp_camera_name (int camera_number, char *camera_name)
{
	if (glob_abilities_list == NULL)
		return (GP_ERROR_MODEL_NOT_FOUND);

	if (camera_number > glob_abilities_list->count)
		return (GP_ERROR_MODEL_NOT_FOUND);

	strcpy (camera_name,
			glob_abilities_list->abilities[camera_number]->model);
	return (GP_OK);
}

int
gp_camera_abilities (int camera_number, CameraAbilities *abilities)
{
	memcpy (abilities, glob_abilities_list->abilities[camera_number],
		sizeof(CameraAbilities));
	
	if (glob_debug)
		gp_abilities_dump (abilities);

	return (GP_OK);
}

int
gp_camera_abilities_by_name (char *camera_name, CameraAbilities *abilities)
{
	int x;
	
	for (x = 0; x < glob_abilities_list->count; x++) {
		if (strcmp (glob_abilities_list->abilities[x]->model,
					camera_name) == 0)
			return (gp_camera_abilities(x, abilities));
	}

	return (GP_ERROR_MODEL_NOT_FOUND);
}
