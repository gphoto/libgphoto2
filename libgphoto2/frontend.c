/* frontend.c
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

#include "gphoto2-frontend.h"

#include "gphoto2-result.h"

/* Front-end function pointers */
CameraMessage  gp_fe_message  = NULL;
CameraConfirm  gp_fe_confirm  = NULL;

int
gp_frontend_register (void *dummy1, void *dummy2,
		      CameraMessage message, CameraConfirm confirm,
		      void *dummy3)
{
	gp_fe_message  = message;
	gp_fe_confirm  = confirm;

	return (GP_OK);
}

int
gp_frontend_message (Camera *camera, char *message)
{
	if (gp_fe_message)
		gp_fe_message(camera, message);
        return(GP_OK);
}

int
gp_frontend_confirm (Camera *camera, char *message)
{
	if (!gp_fe_confirm)
		/* return YES. dangerous? */
		return 1;
        return(gp_fe_confirm(camera, message));
}
