#include <gpio.h>
#include <gphoto2.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "globals.h"
#include "library.h"
#include "settings.h"
#include "util.h"

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
        int x;
 
	widget->changed = 0;
 
        for (x=0; x<widget->children_count; x++)
                gp_frontend_prompt_clean_all(widget->children[x]);
 
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
