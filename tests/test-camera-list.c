/* test-gphoto2.c
 *
 * Copyright © 2001 Lutz Müller <lutz@users.sf.net>
 * Copyright © 2005 Hans Ulrich Niedermann <gp@n-dimensional.de>
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
#include "config.h"

#include <stdio.h>
#include <gphoto2-camera.h>

#define CHECK(f) {int res = f; if (res < 0) {printf ("ERROR: %s\n", gp_result_as_string (res)); return (1);}}

int
main (int argc, char *argv [])
{
	CameraAbilitiesList *al;
	int i;
	int count;

	CHECK (gp_abilities_list_new (&al));
	CHECK (gp_abilities_list_load (al, NULL));

	count = gp_abilities_list_count (al);
	if (count < 0) {
		printf("gp_abilities_list_count error: %d\n", count);
		return(1);
	} else if (count == 0) {
		printf("no camera drivers (camlibs) found in camlib dir\n");
		return(1);
	}

	printf("No.|%-20s|%s\n",
	       "driver (camlib)",
	       "camera model");
	printf("---+%-20s+%s\n",
	       "--------------------",
	       "-------------------------------------------");
	for (i = 0; i < count; i++) {
		CameraAbilities abilities;
		CHECK (gp_abilities_list_get_abilities (al, i, &abilities));
		printf("%3d|%-20s|%s\n", i+1, 
		       abilities.id,
		       abilities.model);
	}
	
	CHECK (gp_abilities_list_free (al));
	return (0);
}

/*
 * Local variables:
 * mode:c
 * c-basic-offset:8
 * End:
 */
