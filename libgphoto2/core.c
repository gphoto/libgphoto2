#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include <gphoto2.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
#  ifdef gettext_noop
#      define N_(String) gettext_noop (String)
#  else
#      define N_(String) (String)
#  endif
#else
#  define _(String) (String)
#  define N_(String) (String)
#endif

#include "core.h"
#include "library.h"
#include "settings.h"
#include "util.h"

/* Debug level */
int             glob_debug = 0;

/* Camera List */
CameraAbilitiesList *glob_abilities_list;

/* Currently loaded settings */
int             glob_setting_count = 0;
Setting         glob_setting[512];

/* Sessions */
int             glob_session_camera = 0;
int             glob_session_file   = 0;

/* Front-end function pointers */
CameraStatus   gp_fe_status   = NULL;
CameraProgress gp_fe_progress = NULL;
CameraMessage  gp_fe_message  = NULL;
CameraConfirm  gp_fe_confirm  = NULL;
CameraPrompt   gp_fe_prompt   = NULL;

static char *result_string[] = {
        /* GP_ERROR_BAD_PARAMETERS      -100 */ N_("Bad parameters"),
        /* GP_ERROR_IO                  -101 */ N_("I/O problem"),
        /* GP_ERROR_CORRUPTED_DATA      -102 */ N_("Corrupted data"),
        /* GP_ERROR_FILE_EXISTS         -103 */ N_("File exists"),
        /* GP_ERROR_NO_MEMORY           -104 */ N_("Insufficient memory"),
        /* GP_ERROR_MODEL_NOT_FOUND     -105 */ N_("Unknown model"),
        /* GP_ERROR_NOT_SUPPORTED       -106 */ N_("Unsupported operation"),
        /* GP_ERROR_DIRECTORY_NOT_FOUND -107 */ N_("Directory not found"),
        /* GP_ERROR_FILE_NOT_FOUND      -108 */ N_("File not found"),
	/* GP_ERROR_DIRECTORY_EXISTS    -109 */ N_("Directory exists"),
        /* GP_ERROR_NO_CAMERA_FOUND     -110 */ N_("No cameras were detected")
};

static int have_initted = 0;

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
        glob_setting_count   = 0;

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
        load_cameras();

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

int gp_exit ()
{
        gp_abilities_list_free (glob_abilities_list);

        return (GP_OK);
}

int gp_autodetect (CameraList *list)
{
    int x=0;
    int status;
    int found=0;
    int vendor, product;
    gp_port *dev;

    status = gp_port_new(&dev, GP_PORT_USB);
    if (status != GP_OK)
        return status;

    for (x=0; x<glob_abilities_list->count; x++) {
        vendor = glob_abilities_list->abilities[x]->usb_vendor;
        product = glob_abilities_list->abilities[x]->usb_product;
        if (vendor) {
            if (gp_port_usb_find_device(dev, vendor, product)==GP_OK) {
                gp_list_append(list, glob_abilities_list->abilities[x]->model);
                strcpy(list->entry[list->count-1].value, "usb:");
                found = 1;
            }
        }
    }

    gp_port_free(dev);

    if (found)
        return GP_OK;
    return GP_ERROR_NO_CAMERA_FOUND;
}


void gp_debug_printf (int level, char *id, char *format, ...)
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

char *gp_result_as_string (int result)
{
        /* Really an error? */
        if (result > 0)
                return _("Unknown error");

        /* IOlib error? Pass through. */
        if ((result <= 0) && (result >= -99))
            return gp_port_result_as_string(result);

        /* Camlib error? You should have called gp_camera_result_as_string... */
        if (result <= -1000)
                return _("Unknown camera library error");

        /* Do we have an error description? */
        if ((-result - 100) < (int) (sizeof (result_string) / 
				     sizeof (*result_string)))
                return _(result_string[-result - 100]);

        return _("Unknown error");
}

