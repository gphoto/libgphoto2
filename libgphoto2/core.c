#include <stdarg.h>
#include <stdio.h>

#include <gpio.h>
#include <gphoto2.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "core.h"
#include "library.h"
#include "settings.h"
#include "util.h"

/* Debug level */
int             glob_debug=0;

/* Camera List */
CameraAbilitiesList *glob_abilities_list;

/* Currently loaded settings */
int             glob_setting_count = 0;
Setting         glob_setting[512];

/* Sessions */
int		glob_session_camera = 0;
int		glob_session_file   = 0;

/* Front-end function pointers */
CameraStatus   gp_fe_status = NULL;
CameraProgress gp_fe_progress = NULL;
CameraMessage  gp_fe_message = NULL;
CameraConfirm  gp_fe_confirm = NULL;


int gp_init (int debug)
{
        char buf[1024];
        int x;

        glob_debug = debug;

        /* Initialize the globals */
        glob_abilities_list = gp_abilities_list_new();
        glob_setting_count   = 0;

	gp_debug_printf(GP_DEBUG_LOW, "core", "Debug level %i", debug);
        gp_debug_printf(GP_DEBUG_LOW, "core", "Creating $HOME/.gphoto");

        /* Make sure the directories are created */
#ifdef WIN32
	GetWindowsDirectory(buf, 1024);
	strcat(buf, "\\gphoto");
	(void)_mkdir(buf);
#else
        sprintf(buf, "%s/.gphoto", getenv("HOME"));
        (void)mkdir(buf, 0700);
#endif
	gp_debug_printf(GP_DEBUG_LOW, "core", "Initializing gpio");

        if (gpio_init(debug) == GPIO_ERROR) {
                gp_camera_message(NULL, "ERROR: Can not initialize libgpio");
                return (GP_ERROR);
        }

	gp_debug_printf(GP_DEBUG_LOW, "core", "Trying to load settings");
        /* Load settings */
        load_settings();

	gp_debug_printf(GP_DEBUG_LOW, "core", "Trying to load libraries");
        load_cameras();

        if (glob_debug) {
                gp_debug_printf(GP_DEBUG_LOW, "core", "List of cameras found");
                for (x=0; x<glob_abilities_list->count; x++)
			gp_debug_printf(GP_DEBUG_LOW, "core", "\t\"%s\" uses %s",
                                glob_abilities_list->abilities[x]->model,
                                glob_abilities_list->abilities[x]->library);
                if (glob_abilities_list->count == 0)
                        gp_debug_printf(GP_DEBUG_LOW, "core","\tNone");
                gp_debug_printf(GP_DEBUG_LOW, "core", "Initializing the gPhoto IO library (libgpio)");
        }

        return (GP_OK);
}

int gp_exit ()
{
        gp_abilities_list_free(glob_abilities_list);

        return (GP_OK);
}

void gp_debug_printf(int level, char *id, char *format, ...)
{
	va_list arg;

	if (glob_debug == GP_DEBUG_NONE)
		return;

        if (level >= glob_debug) {
		fprintf(stderr, "%s: ", id);
		va_start(arg, format);
		vfprintf(stderr, format, arg);
		va_end(arg);
		fprintf(stderr, "\n");
	}
}

int gp_frontend_register(CameraStatus status, CameraProgress progress, 
			 CameraMessage message, CameraConfirm confirm) {

	gp_fe_status   = status;
	gp_fe_progress = progress;
	gp_fe_message  = message;
	gp_fe_confirm  = confirm;

	return (GP_OK);
}
