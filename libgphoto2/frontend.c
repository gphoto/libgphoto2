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
CameraStatus   gp_fe_status   = NULL;
CameraProgress gp_fe_progress = NULL;
CameraMessage  gp_fe_message  = NULL;
CameraConfirm  gp_fe_confirm  = NULL;
CameraPrompt   gp_fe_prompt   = NULL;

int gp_frontend_register (CameraStatus status, CameraProgress progress,
			  CameraMessage message, CameraConfirm confirm,
			  CameraPrompt prompt)
{
	gp_fe_status   = status;
	gp_fe_progress = progress;
	gp_fe_message  = message;
	gp_fe_confirm  = confirm;
	gp_fe_prompt   = prompt;

	return (GP_OK);
}

int gp_frontend_status (Camera *camera, char *status)
{
	if (gp_fe_status)
		gp_fe_status(camera, status);
        return(GP_OK);
}

int gp_frontend_progress (Camera *camera, CameraFile *file, float percentage)
{
	if (gp_fe_progress)
		gp_fe_progress(camera, file, percentage);

        return(GP_OK);
}

int gp_frontend_message (Camera *camera, char *message)
{
	if (gp_fe_message)
		gp_fe_message(camera, message);
        return(GP_OK);
}

int gp_frontend_confirm (Camera *camera, char *message)
{
	if (!gp_fe_confirm)
		/* return YES. dangerous? */
		return 1;
        return(gp_fe_confirm(camera, message));
}

int gp_frontend_prompt_clean_all (CameraWidget *widget)
{
	CameraWidget *child;
        int x;

	if (!widget)
		return (GP_ERROR_BAD_PARAMETERS);

	gp_widget_set_changed (widget, 0);
 
        for (x = 0; x < gp_widget_count_children (widget); x++) {
		child = NULL;
		gp_widget_get_child (widget, x, &child);
                gp_frontend_prompt_clean_all (child);
	}
 
        return (GP_OK);
}

int gp_frontend_prompt (Camera *camera, CameraWidget *window)
{
	gp_frontend_prompt_clean_all(window);

	if (!gp_fe_prompt)
		/* return OK */
		return GP_PROMPT_OK;
        return(gp_fe_prompt(camera, window));
}
