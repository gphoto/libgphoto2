/* test-gphoto2.c
 *
 * Copyright 2001 Lutz Mueller <lutz@users.sf.net>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */
#include "config.h"

#include <stdio.h>
#ifdef HAVE_MCHECK_H
#include <mcheck.h>
#endif

#include <gphoto2/gphoto2-camera.h>

#define CHECK(f) {int res = f; if (res < 0) {printf ("ERROR: %s\n", gp_result_as_string (res)); return (1);}}

int
main ()
{
	CameraText text;
	Camera *camera;
	CameraAbilitiesList *al;
	CameraAbilities abilities;
	int m;

#ifdef HAVE_MCHECK_H
	mtrace();
#endif

	/*
	 * You'll probably want to access your camera. You will first have
	 * to create a camera (that is, allocating the memory).
	 */
	printf ("Creating camera...\n");
	CHECK (gp_camera_new (&camera));

	/*
	 * Before you initialize the camera, set the model so that
	 * gphoto2 knows which library to use.
	 */
	printf ("Setting model...\n");
	CHECK (gp_abilities_list_new (&al));
	CHECK (gp_abilities_list_load (al, NULL));
	CHECK (m = gp_abilities_list_lookup_model (al, "Directory Browse"));
	CHECK (gp_abilities_list_get_abilities (al, m, &abilities));
	CHECK (gp_abilities_list_free (al));
	CHECK (gp_camera_set_abilities (camera, abilities));

	/*
	 * Now, initialize the camera (establish a connection).
	 */
	printf ("Initializing camera...\n");
	CHECK (gp_camera_init (camera, NULL));

	/*
	 * At this point, you can do whatever you want.
	 * You could get files, capture images...
	 */
	printf ("Getting information about the driver...\n");
	CHECK (gp_camera_get_about (camera, &text, NULL));
	printf ("%s\n", text.text);

	/*
	 * Don't forget to clean up when you don't need the camera any
	 * more. Please use unref instead of destroy - you'll never know
	 * if some part of the program still needs the camera.
	 */
	printf ("Unrefing camera...\n");
	CHECK (gp_camera_unref (camera));

#ifdef HAVE_MCHECK_H
	muntrace();
#endif

	return (0);
}

/*
 * Local variables:
 *  compile-command: "gcc -pedantic -Wstrict-prototypes -O2 -g test-gphoto2.c -o test-gphoto2 `gphoto2-config --cflags --libs` && export MALLOC_TRACE=test-gphoto2.log && ./test-gphoto2 && mtrace ./test-gphoto2 test-gphoto2.log"
 * End:
 */
