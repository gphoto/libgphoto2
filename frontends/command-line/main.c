/* $Id$
 *
 * Copyright (C) 2002 Lutz Müller <lutz@users.sourceforge.net>
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
#include "main.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <locale.h>

#ifdef HAVE_RL
#  include <readline/readline.h>
#endif

#ifdef HAVE_PTHREAD
#  include <pthread.h>
#endif

#ifndef DISABLE_DEBUGGING
/* we need these for timestamps of debugging messages */
#include <time.h>
#include <sys/time.h>
#endif

#ifndef WIN32
#  include <signal.h>
#endif

#include "actions.h"
#include "foreach.h"
#include "options.h"
#include "range.h"
#include "shell.h"

#ifdef HAVE_CDK
#  include "gphoto2-cmd-config.h"
#endif

#ifdef HAVE_AA
#include "gphoto2-cmd-capture.h"
#endif

#include "gphoto2-port-info-list.h"
#include "gphoto2-port-log.h"
#include "gphoto2-setting.h"

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif 
#else
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define _(String) (String)
#  define N_(String) (String)
#endif

#ifndef MAX
# define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
# define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#define CR(result) {int r = (result); if (r < 0) return (r);}

#define GP_USB_HOTPLUG_SCRIPT "usbcam"
#define GP_USB_HOTPLUG_MATCH_VENDOR_ID          0x0001
#define GP_USB_HOTPLUG_MATCH_PRODUCT_ID         0x0002

#define GP_USB_HOTPLUG_MATCH_DEV_CLASS          0x0080
#define GP_USB_HOTPLUG_MATCH_DEV_SUBCLASS       0x0100
#define GP_USB_HOTPLUG_MATCH_DEV_PROTOCOL       0x0200

/* Takes the current globals, and sets up the gPhoto lib with them */
static int set_globals (void);

/* Command-line option
   -----------------------------------------------------------------------
   this is funky and may look a little scary, but sounded cool to do.
   it makes sense since this is just a wrapper for a functional/flow-based
   library.
   AFTER NOTE: whoah. gnome does something like this. cool deal :)

   When the program starts it calls (in order):
        1) verify_options() to make sure all options are valid,
           and that any options needing an argument have one (or if they
           need an argument, they don't have one).
        2) (optional) option_is_present() to see if a particular option was
           typed in the command-line. This can be used to set up something
           before the next step.
        3) execute_options() to actually parse the command-line and execute
           the command-line options in order.

   This might sound a little complex, but makes adding options REALLY easy,
   and it is very flexible.

   How to add a command-line option:
*/

/* 1) Add a forward-declaration here                                    */
/*    ----------------------------------------------------------------- */
/*    Use the OPTION_CALLBACK(function) macro.                          */

OPTION_CALLBACK(abilities);
#ifdef HAVE_CDK
OPTION_CALLBACK(config);
#endif
#ifdef HAVE_EXIF
OPTION_CALLBACK(show_exif);
#endif
OPTION_CALLBACK(show_info);
OPTION_CALLBACK(help);
OPTION_CALLBACK(version);
OPTION_CALLBACK(shell);
OPTION_CALLBACK(list_cameras);
OPTION_CALLBACK(print_usb_usermap);
OPTION_CALLBACK(usb_usermap_script);
OPTION_CALLBACK(auto_detect);
OPTION_CALLBACK(list_ports);
OPTION_CALLBACK(filename);
OPTION_CALLBACK(port);
OPTION_CALLBACK(speed);
OPTION_CALLBACK(usbid);
OPTION_CALLBACK(model);
OPTION_CALLBACK(quiet);
#ifndef DISABLE_DEBUGGING
OPTION_CALLBACK(debug);
#endif
OPTION_CALLBACK(use_folder);
OPTION_CALLBACK(recurse);
OPTION_CALLBACK(no_recurse);
OPTION_CALLBACK(use_stdout);
OPTION_CALLBACK(use_stdout_size);
OPTION_CALLBACK(list_folders);
OPTION_CALLBACK(list_files);
OPTION_CALLBACK(num_files);
OPTION_CALLBACK(get_file);
OPTION_CALLBACK(get_all_files);
OPTION_CALLBACK(get_thumbnail);
OPTION_CALLBACK(get_all_thumbnails);
OPTION_CALLBACK(get_raw_data);
OPTION_CALLBACK(get_all_raw_data);
OPTION_CALLBACK(get_audio_data);
OPTION_CALLBACK(get_all_audio_data);
OPTION_CALLBACK(delete_file);
OPTION_CALLBACK(delete_all_files);
OPTION_CALLBACK(upload_file);
OPTION_CALLBACK(capture_image);
OPTION_CALLBACK(capture_preview);
OPTION_CALLBACK(capture_movie);
OPTION_CALLBACK(capture_sound);
OPTION_CALLBACK(summary);
OPTION_CALLBACK(manual);
OPTION_CALLBACK(about);
OPTION_CALLBACK(make_dir);
OPTION_CALLBACK(remove_dir);
OPTION_CALLBACK(authors);
OPTION_CALLBACK(license);

/* 2) Add an entry in the option table                          */
/*    ----------------------------------------------------------------- */
/*    Format for option is:                                             */
/*     {"short", "long", "argument", "description", callback_function, required}, */
/*    if it is just a flag, set the argument to "".                     */
/*    Order is important! Options are exec'd in the order in the table! */

Option option[] = {

/* Settings needed for formatting output */
#ifndef DISABLE_DEBUGGING
{"",  "debug", "", N_("Turn on debugging"),              debug,         0},
#endif
{"q", "quiet", "", N_("Quiet output (default=verbose)"), quiet,         0},

/* Display and die actions */

{"v", "version",     "", N_("Display version and exit"),     version,        0},
{"h", "help",        "", N_("Displays this help screen"),    help,           0},
{"",  "list-cameras","", N_("List supported camera models"), list_cameras,   0},
{"",  "usb-usermap-script", "name", N_("usb.usermap script (dflt=\"usbcam\")"), usb_usermap_script, 0},
{"",  "print-usb-usermap",  "",     N_("Create output for usb.usermap"),        print_usb_usermap,  0},
{"",  "list-ports",  "", N_("List supported port devices"),  list_ports,     0},
{"",  "stdout",      "", N_("Send file to stdout"),          use_stdout,     0},
{"",  "stdout-size", "", N_("Print filesize before data"),   use_stdout_size,0},
{"",  "auto-detect", "", N_("List auto-detected cameras"),   auto_detect,    0},

/* Settings needed for camera functions */
{"" , "port",     "path",     N_("Specify port device"),            port,      0},
{"" , "speed",    "speed",    N_("Specify serial transfer speed"),  speed,     0},
{"" , "camera",   "model",    N_("Specify camera model"),           model,     0},
{"" , "filename", "filename", N_("Specify a filename"),             filename, 0},
{"" , "usbid",    "usbid",    N_("(expert only) Override USB IDs"), usbid,     0},

/* Actions that depend on settings */
{"a", "abilities", "",       N_("Display camera abilities"), abilities,    0},
{"f", "folder",    "folder", N_("Specify camera folder (default=\"/\")"),use_folder,0},
{"R", "recurse", "",  N_("Recursion (default for download)"), recurse, 0},
{"", "no-recurse", "",  N_("No recursion (default for deletion)"), no_recurse, 0},
{"l", "list-folders",   "", N_("List folders in folder"), list_folders,   0},
{"L", "list-files",     "", N_("List files in folder"),   list_files,     0},
{"m", "mkdir", N_("name"),  N_("Create a directory"),     make_dir,       0},
{"r", "rmdir", N_("name"),  N_("Remove a directory"),     remove_dir,     0},
{"n", "num-files", "",           N_("Display number of files"),     num_files,   0},
{"p", "get-file", "range",       N_("Get files given in range"),    get_file,    0},
{"P", "get-all-files","",        N_("Get all files from folder"),   get_all_files,0},
{"t", "get-thumbnail",  "range",  N_("Get thumbnails given in range"),  get_thumbnail,  0},
{"T", "get-all-thumbnails","",    N_("Get all thumbnails from folder"), get_all_thumbnails,0},
{"r", "get-raw-data", "range",    N_("Get raw data given in range"),    get_raw_data, 0},
{"", "get-all-raw-data", "",      N_("Get all raw data from folder"),   get_all_raw_data, 0},
{"", "get-audio-data", "range",   N_("Get audio data given in range"),  get_audio_data, 0},
{"", "get-all-audio-data", "",    N_("Get all audio data from folder"), get_all_audio_data, 0},
{"d", "delete-file", "range",  N_("Delete files given in range"), delete_file, 0},
{"D", "delete-all-files","",     N_("Delete all files in folder"), delete_all_files,0},
{"u", "upload-file", "filename", N_("Upload a file to camera"),    upload_file, 0},
#ifdef HAVE_CDK
{"" , "config",         "",  N_("Configure"),               config,          0},
#endif
{"" , "capture-preview", "", N_("Capture a quick preview"), capture_preview, 0},
{"" , "capture-image",  "",  N_("Capture an image"),        capture_image,   0},
{"" , "capture-movie",  "",  N_("Capture a movie "),        capture_movie,   0},
{"" , "capture-sound",  "",  N_("Capture an audio clip"),   capture_sound,   0},
#ifdef HAVE_EXIF
{"", "show-exif", "range",   N_("Show EXIF information"), show_exif, 0},
#endif
{"", "show-info", "range",   N_("Show info"), show_info, 0},
{"",  "summary",        "",  N_("Summary of camera status"), summary, 0},
{"",  "manual",         "",  N_("Camera driver manual"),     manual,  0},
{"",  "about",          "",  N_("About the camera driver"),  about,   0},
{"",  "shell",          "",  N_("gPhoto shell"),             shell,   0},
{"",  "authors",        "",  N_("Authors"),                  authors, 0},
{"",  "license",        "",  N_("License"),                  license, 0},

/* End of list                  */
{"" , "", "", "", NULL, 0}
};

/* 3) Add any necessary global variables                                */
/*    ----------------------------------------------------------------- */
/*    Most flags will set options, like choosing a port, camera model,  */
/*    etc...                                                            */

int  glob_option_count;
char glob_port[128];
static char glob_model[64];
char glob_folder[1024];
char glob_usb_usermap_script[1024];
char glob_owd[1024];
char glob_cwd[1024];
int  glob_speed;

int glob_usbid[5];

Camera    *glob_camera  = NULL;
GPContext *glob_context = NULL;

static ForEachParams fparams = {NULL, NULL, NULL, FOR_EACH_FLAGS_RECURSE};
static ActionParams  aparams;

int  glob_quiet=0;
static char glob_filename[128];
int  glob_stdout=0;
int  glob_stdout_size=0;
char glob_cancel = 0;
static int glob_cols = 80;

/* 4) Finally, add your callback function.                              */
/*    ----------------------------------------------------------------- */
/*    The callback function is passed "char *arg" to the argument of    */
/*    command-line option. It must return GP_OK or GP_ERROR.            */
/*    Again, use the OPTION_CALLBACK(function) macro.                   */

OPTION_CALLBACK(license)
{
	FILE *f;
	char buf[1024];
	size_t size;

	f = fopen (DOC_DIR "/COPYING", "rb");
	if (!f) {
		gp_context_error (glob_context, _("Could not open "
			"'" DOC_DIR "/COPYING'. Please verify your "
			"installation."));
		return (GP_ERROR_FILE_NOT_FOUND);
	}

	while ((size = fread (buf, 1, 1024, f)))
		fwrite (buf, 1, size, stdout);

	fclose (f);
	return (GP_OK);
}

OPTION_CALLBACK(authors)
{
	FILE *f;
	char buf[1024];
	size_t size;

	f = fopen (DOC_DIR "/AUTHORS", "rb");
	if (!f) {
		gp_context_error (glob_context, _("Could not open " 
			"'" DOC_DIR "/COPYING'. Please verify your "
			"installation."));
		return (GP_ERROR_FILE_NOT_FOUND);
	}

	while ((size = fread (buf, 1, 1024, f)))
		fwrite (buf, 1, size, stdout);

	fclose (f);
	return (GP_OK);
}

OPTION_CALLBACK(help)
{
        usage ();
        exit (EXIT_SUCCESS);

        return GP_OK;
}

OPTION_CALLBACK(version)
{
        print_version ();
        exit (EXIT_SUCCESS);

        return GP_OK;
}

OPTION_CALLBACK(use_stdout) {

        glob_quiet  = 1; /* implied */
        glob_stdout = 1;

        return GP_OK;
}

OPTION_CALLBACK(use_stdout_size)
{
        glob_stdout_size = 1;
        use_stdout(arg);

        return GP_OK;
}

OPTION_CALLBACK (auto_detect)
{
        int x, count;
        CameraList list;
	CameraAbilitiesList *al;
	GPPortInfoList *il;
        const char *name, *value;

	gp_abilities_list_new (&al);
	gp_abilities_list_load (al, glob_context);
	gp_port_info_list_new (&il);
	gp_port_info_list_load (il);
	gp_abilities_list_detect (al, il, &list, glob_context);
	gp_abilities_list_free (al);
	gp_port_info_list_free (il);

        CR (count = gp_list_count (&list));

        printf(_("%-30s %-16s\n"), _("Model"), _("Port"));
        printf(_("----------------------------------------------------------\n"));
        for (x = 0; x < count; x++) {
                CR (gp_list_get_name  (&list, x, &name));
                CR (gp_list_get_value (&list, x, &value));
                printf(_("%-30s %-16s\n"), name, value);
        }

        return GP_OK;
}

OPTION_CALLBACK (abilities)
{
        int x = 0;
	CameraAbilitiesList *al;
        CameraAbilities abilities;

        if (!strlen (glob_model)) {
		cli_error_print (_("You need to specify a camera model"));
                return (GP_ERROR_BAD_PARAMETERS);
        }

	CR (gp_abilities_list_new (&al));
	CR (gp_abilities_list_load (al, glob_context));
	CR (x = gp_abilities_list_lookup_model (al, glob_model));
	CR (gp_abilities_list_get_abilities (al, x, &abilities));
	CR (gp_abilities_list_free (al));

        /* Output a parsing friendly abilities table. Split on ":" */
        printf(_("Abilities for camera             : %s\n"),
                abilities.model);
        printf(_("Serial port support              : %s\n"),
		(abilities.port & GP_PORT_SERIAL)? _("yes"):_("no"));
        printf(_("USB support                      : %s\n"),
		(abilities.port & GP_PORT_USB)? _("yes"):_("no"));
        if (abilities.speed[0] != 0) {
        printf(_("Transfer speeds supported        :\n"));
		x = 0;
                do {
        printf(_("                                 : %i\n"), abilities.speed[x]);
                        x++;
                } while (abilities.speed[x]!=0);
        }
        printf(_("Capture choices                  :\n"));
        if (abilities.operations & GP_OPERATION_CAPTURE_IMAGE)
            printf(_("                                 : Image\n"));
        if (abilities.operations & GP_OPERATION_CAPTURE_VIDEO)
            printf(_("                                 : Video\n"));
        if (abilities.operations & GP_OPERATION_CAPTURE_AUDIO)
            printf(_("                                 : Audio\n"));
        if (abilities.operations & GP_OPERATION_CAPTURE_PREVIEW)
            printf(_("                                 : Preview\n"));
        printf(_("Configuration support            : %s\n"),
                abilities.operations & GP_OPERATION_CONFIG? _("yes"):_("no"));
        printf(_("Delete files on camera support   : %s\n"),
                abilities.file_operations & GP_FILE_OPERATION_DELETE? _("yes"):_("no"));
        printf(_("File preview (thumbnail) support : %s\n"),
                abilities.file_operations & GP_FILE_OPERATION_PREVIEW? _("yes"):_("no"));
        printf(_("File upload support              : %s\n"),
                abilities.folder_operations & GP_FOLDER_OPERATION_PUT_FILE? _("yes"):_("no"));

        return (GP_OK);
}


OPTION_CALLBACK(list_cameras) {

        int x, n;
	CameraAbilitiesList *al;
        CameraAbilities a;

	CR (gp_abilities_list_new (&al));
	CR (gp_abilities_list_load (al, glob_context));
        CR (n = gp_abilities_list_count (al));

        if (glob_quiet)
                printf ("%i\n", n);
	else {
                printf (_("Number of supported cameras: %i\n"), n);
	   	printf (_("Supported cameras:\n"));
	}

        for (x = 0; x < n; x++) {
		CR (gp_abilities_list_get_abilities (al, x, &a));

                if (glob_quiet)
                        printf("%s\n", a.model);
                else
                        switch (a.status) {
                        case GP_DRIVER_STATUS_TESTING:
                                printf (_("\t\"%s\" (TESTING)\n"), a.model);
                                break;
                        case GP_DRIVER_STATUS_EXPERIMENTAL:
                                printf (_("\t\"%s\" (EXPERIMENTAL)\n"), a.model);
                                break;
                        default:
                        case GP_DRIVER_STATUS_PRODUCTION:
                                printf (_("\t\"%s\"\n"), a.model);
                                break;
                        }
        }
	CR (gp_abilities_list_free (al));

        return (GP_OK);
}

/* print_usb_usermap
 *
 * Print out lines that can be included into usb.usermap 
 * - for all cams supported by our instance of libgphoto2.
 *
 * written after list_cameras 
 */

OPTION_CALLBACK(print_usb_usermap) {

	int x, n;
	CameraAbilitiesList *al;
        CameraAbilities a;

	CR (gp_abilities_list_new (&al));
	CR (gp_abilities_list_load (al, glob_context));
	CR (n = gp_abilities_list_count (al));

        for (x = 0; x < n; x++) {
		CR (gp_abilities_list_get_abilities (al, x, &a));
		if (!(a.port & GP_PORT_USB))
		    continue;
		if (a.usb_vendor && a.usb_product) {
			printf ("%-20s "
				"0x%04x      0x%04x   0x%04x    0x0000       "
				"0x0000       0x00         0x00            "
				"0x00            0x00            0x00               "
				"0x00               0x00000000\n",
				glob_usb_usermap_script,
				GP_USB_HOTPLUG_MATCH_VENDOR_ID | GP_USB_HOTPLUG_MATCH_PRODUCT_ID,
				a.usb_vendor, a.usb_product);
		}
		if (a.usb_class) {
		    	int flags;
			int class,subclass,proto;

			class = a.usb_class;
			subclass = a.usb_subclass;
			proto = a.usb_protocol;
			flags = GP_USB_HOTPLUG_MATCH_DEV_CLASS;
			if (subclass != -1)
			    flags |= GP_USB_HOTPLUG_MATCH_DEV_SUBCLASS;
			else
			    subclass = 0;
			if (proto != -1)
			    flags |= GP_USB_HOTPLUG_MATCH_DEV_PROTOCOL;
			else
			    proto = 0;
			printf ("%-20s "
				"0x%04x      0x0000   0x0000    0x0000       "
				"0x0000       0x%02x         0x%02x            "
				"0x%02x        0x00            0x00               "
				"0x00               0x00000000\n",
				glob_usb_usermap_script,
				flags,
				class, subclass, proto);
		}
        }
	CR (gp_abilities_list_free (al));

        return (GP_OK);
}

OPTION_CALLBACK(usb_usermap_script)
{
	strncpy (glob_usb_usermap_script, arg,
		 sizeof (glob_usb_usermap_script) - 1);
	glob_usb_usermap_script[sizeof (glob_usb_usermap_script) - 1] = 0;

	return (GP_OK);
}

OPTION_CALLBACK(list_ports)
{
	GPPortInfoList *list;
	GPPortInfo info;
        int x, count, result;

	CR (gp_port_info_list_new (&list));
	result = gp_port_info_list_load (list);
	if (result < 0) {
		gp_port_info_list_free (list);
		return (result);
	}
	count = gp_port_info_list_count (list);
	if (count < 0) {
		gp_port_info_list_free (list);
		return (count);
	}

        if (!glob_quiet) {
          printf(_("Devices found: %i\n"), count);
          printf(_("Path                             Description\n"
                 "--------------------------------------------------------------\n"));
        } else
          printf("%i\n", count);

	/* Now list the ports */
	for (x = 0; x < count; x++) {
		result = gp_port_info_list_get_info (list, x, &info);
		if (result < 0) {
			gp_port_info_list_free (list);
			return (result);
		}
                printf ("%-32s %-32s\n", info.path, info.name);
        }

        return (GP_OK);
}

OPTION_CALLBACK(filename)
{
        gp_log (GP_LOG_DEBUG, "main", "Setting filename to %s...", arg);

        strncpy (glob_filename, arg, sizeof (glob_filename) - 1);

        return (GP_OK);
}

OPTION_CALLBACK(port)
{
        gp_log (GP_LOG_DEBUG, "main", "Setting port to '%s'...", arg);

	strncpy (glob_port, arg, sizeof (glob_port) - 1);
	glob_port[sizeof (glob_port) - 1] = 0;

	if (strchr(glob_port, ':') == NULL) {
		/* User didn't specify the port type; try to guess it */

		gp_log (GP_LOG_DEBUG, "main", _("Ports must look like "
		  "'serial:/dev/ttyS0' or 'usb:', but '%s' is missing a colon "
		  "so I am going to guess what you mean."),
		glob_port);

		if (strcmp(arg, "usb") == 0) {
			/* User forgot the colon; add it for him */
			strcpy (glob_port, "usb:");

		} else if (strncmp(arg, "/dev/", 5) == 0) {
			/* Probably a serial port like /dev/ttyS0 */
			strcpy (glob_port, "serial:");
			strncat (glob_port, arg, sizeof(glob_port)-7);

		} else if (strncmp(arg, "/proc/", 6) == 0) {
			/* Probably a USB port like /proc/bus/usb/001 */
			strcpy (glob_port, "usb:");
			strncat (glob_port, arg, sizeof(glob_port)-4);
		}
		gp_log (GP_LOG_DEBUG, "main", "Guessed port name. Using port "
			"'%s' from now on.", glob_port);
	}

        return (GP_OK);
}

OPTION_CALLBACK(speed)
{
	gp_log (GP_LOG_DEBUG, "main", "Setting speed to '%s'...", arg);

        glob_speed = atoi (arg);

        return (GP_OK);
}

OPTION_CALLBACK(model)
{
        gp_log (GP_LOG_DEBUG, "main", "Setting camera model to '%s'...", arg);

        strncpy (glob_model, arg, sizeof (glob_model) - 1);
        glob_model[sizeof (glob_model) - 1] = 0;

        return (GP_OK);
}

OPTION_CALLBACK(usbid)
{
	gp_log (GP_LOG_DEBUG, "main", "Overriding USB IDs to '%s'...", arg);

	if (sscanf (arg, "0x%x:0x%x=0x%x:0x%x", &glob_usbid[0], &glob_usbid[1],
		    &glob_usbid[2], &glob_usbid[3]) != 4) {
		printf (_("Use the following syntax a:b=c:d to treat any "
			  "USB device detected as a:b as c:d instead. "
			  "a b c d should be hexadecimal numbers beginning "
			  "with '0x'.\n"));
		return (GP_ERROR_BAD_PARAMETERS);
	}
	glob_usbid[4] = 1;
	return (GP_OK);
}

#ifndef DISABLE_DEBUGGING
/* time zero for debug log time stamps */
struct timeval glob_tv_zero = { 0, 0 };

static void
debug_func (GPLogLevel level, const char *domain, const char *format,
	    va_list args, void *data)
{
	struct timeval tv;
	gettimeofday (&tv,NULL);
	fprintf (stderr, "%li.%06li %s(%i): ", 
		 tv.tv_sec - glob_tv_zero.tv_sec, 
		 (1000000 + tv.tv_usec - glob_tv_zero.tv_usec) % 1000000,
		 domain, level);
	vfprintf (stderr, format, args);
	fprintf (stderr, "\n");
}

OPTION_CALLBACK (debug)
{
	gp_log_add_func (GP_LOG_ALL, debug_func, NULL);
	gp_log (GP_LOG_DEBUG, "main", _("ALWAYS INCLUDE THE FOLLOWING LINE "
		"WHEN SENDING DEBUG MESSAGES TO THE MAILING LIST:"));
	gp_log (GP_LOG_DEBUG, "main", PACKAGE " " VERSION);

	return (GP_OK);
}
#endif

OPTION_CALLBACK(quiet)
{
        glob_quiet=1;

        return (GP_OK);
}

OPTION_CALLBACK(shell)
{
        CR (set_globals ());
	CR (shell_prompt (glob_camera));

	return (GP_OK);
}

OPTION_CALLBACK (use_folder)
{
        gp_log (GP_LOG_DEBUG, "main", "Setting folder to '%s'...", arg);

        strncpy (glob_folder, arg, sizeof (glob_folder) - 1);
        glob_folder[sizeof (glob_folder) - 1] = 0;

        return (GP_OK);
}

OPTION_CALLBACK (recurse)
{
	fparams.flags |= FOR_EACH_FLAGS_RECURSE;

        return (GP_OK);
}

OPTION_CALLBACK (no_recurse)
{
	fparams.flags &= ~FOR_EACH_FLAGS_RECURSE;

        return (GP_OK);
}

OPTION_CALLBACK (list_folders)
{
        CR (set_globals ());
	CR (for_each_folder (&fparams, list_folders_action));

	return (GP_OK);
}

#ifdef HAVE_CDK
OPTION_CALLBACK(config)
{
	CR (set_globals ());

	CR (gp_cmd_config (glob_camera, glob_context));

	return (GP_OK);
}
#endif

OPTION_CALLBACK(show_info)
{
	CR (set_globals ());

	/*
	 * If the user specified the file directly (and not a range),
	 * directly dump info.
	 */
	if (strchr (arg, '.'))
		return (print_info_action (&aparams, arg));

	return (for_each_file_in_range (&fparams, print_info_action,
					arg));
}

#ifdef HAVE_EXIF
OPTION_CALLBACK(show_exif)
{
	CR (set_globals ());

	/*
	 * If the user specified the file directly (and not a range),
	 * directly dump EXIF information.
	 */
	if (strchr (arg, '.'))
		return (print_exif_action (&aparams, arg));

	return (for_each_file_in_range (&fparams, print_exif_action,
					arg));
}
#endif

OPTION_CALLBACK (list_files)
{
        CR (set_globals ());
        CR (for_each_folder (&fparams, list_files_action));

	return (GP_OK);
}

OPTION_CALLBACK (num_files)
{
        int count;
        CameraList list;

        CR (set_globals ());
        CR (gp_camera_folder_list_files (glob_camera, glob_folder,
                                                   &list, glob_context));
        CR (count = gp_list_count (&list));

        if (glob_quiet)
                printf ("%i\n", count);
           else
                printf (_("Number of files in folder %s: %i\n"),
                        glob_folder, count);

        return (GP_OK);
}

static int
save_camera_file_to_file (CameraFile *file, CameraFileType type)
{
        char ofile[1024], ofolder[1024], buf[1024], c[1024];
        int result;
        char *f;
        const char *name;
	unsigned int num = 1;

        /* Determine the folder and filename */
        strcpy (ofile, "");
        strcpy (ofolder, "");

        if (strcmp (glob_filename, "")) {
                if (strchr (glob_filename, '/')) {              /* Check if they specified an output dir */
                        f = strrchr(glob_filename, '/');
                        strcpy (buf, f+1);                       /* Get the filename */
                        sprintf (ofile, buf, num++);
                        *f = 0;
                        strcpy (ofolder, glob_filename);      /* Get the folder */
                        strcat (ofolder, "/");
                        *f = '/';
                } else {                                        /* If not, subst and set */
                        sprintf (ofile, glob_filename, num++);
                }
        } else {
                gp_file_get_name (file, &name);
		strcat (ofile, name);
		gp_log (GP_LOG_DEBUG, "gphoto2", _("Using filename '%s'..."),
			ofile);
        }

        switch (type) {
        case GP_FILE_TYPE_PREVIEW:
                snprintf (buf, sizeof (buf), "%sthumb_%s", ofolder, ofile);
                break;
        case GP_FILE_TYPE_NORMAL:
		snprintf (buf, sizeof (buf), "%s%s", ofolder, ofile);
                break;
        case GP_FILE_TYPE_RAW:
                snprintf (buf, sizeof (buf), "%sraw_%s", ofolder, ofile);
                break;
	case GP_FILE_TYPE_AUDIO:
		snprintf (buf, sizeof (buf), "%saudio_%s", ofolder, ofile);
		break;
        default:
                return (GP_ERROR_NOT_SUPPORTED);
        }
        if (!glob_quiet) {
                while (GP_SYSTEM_IS_FILE(buf)) {
			if (glob_quiet)
				break;

			do {
				putchar('\007');
				printf(_("File %s exists. Overwrite? [y|n] "),buf);
				fflush(stdout);
				fgets(c, 1023, stdin);
			} while ((c[0]!='y')&&(c[0]!='Y')&&
				 (c[0]!='n')&&(c[0]!='N'));

			if ((c[0]=='y') || (c[0]=='Y'))
				break;


			do { 
				printf(_("Specify new filename? [y|n] "));
				fflush(stdout); 
				fgets(c, 1023, stdin);
			} while ((c[0]!='y')&&(c[0]!='Y')&&
				 (c[0]!='n')&&(c[0]!='N'));

			if (!((c[0]=='y') || (c[0]=='Y')))
				return (GP_OK);

			printf(_("Enter new filename: "));
			fflush(stdout);
			fgets(buf, 1023, stdin);
			buf[strlen(buf)-1]=0;
                }
                printf(_("Saving file as %s\n"), buf);
        }

        if ((result = gp_file_save(file, buf)) != GP_OK)
                cli_error_print(_("Can not save file as %s"), buf);

        return (result);
}

int
save_file_to_file (Camera *camera, GPContext *context, const char *folder,
		   const char *filename, CameraFileType type)
{
        int res;

        CameraFile *file;

        CR (gp_file_new (&file));
        CR (gp_camera_file_get (camera, folder, filename, type,
                                          file, context));

        if (glob_stdout) {
                const char *data;
                long int size;

                CR (gp_file_get_data_and_size (file, &data, &size));

                if (glob_stdout_size)
                        printf ("%li\n", size);
                fwrite (data, sizeof(char), size, stdout);
                gp_file_unref (file);
                return (GP_OK);
        }

        res = save_camera_file_to_file (file, type);

        gp_file_unref (file);

        return (res);
}


/*
  get_file_common() - parse range, download specified files, or their
        thumbnails according to thumbnail argument, and save to files.
*/

int
get_file_common (char *arg, CameraFileType type )
{
        gp_log (GP_LOG_DEBUG, "main", "Getting '%s'...", arg);

        CR (set_globals ());

	/*
	 * If the user specified the file directly (and not a number),
	 * get that file.
	 */
        if (strchr (arg, '.'))
                return (save_file_to_file (glob_camera, glob_context,
					   glob_folder, arg, type));

        switch (type) {
        case GP_FILE_TYPE_PREVIEW:
		CR (for_each_file_in_range (&fparams, save_thumbnail_action,
					    arg));
		break;
        case GP_FILE_TYPE_NORMAL:
                CR (for_each_file_in_range (&fparams, save_file_action, arg));
		break;
        case GP_FILE_TYPE_RAW:
                CR (for_each_file_in_range (&fparams, save_raw_action, arg));
		break;
	case GP_FILE_TYPE_AUDIO:
		CR (for_each_file_in_range (&fparams, save_audio_action, arg));
		break;
	case GP_FILE_TYPE_EXIF:
		CR (for_each_file_in_range (&fparams, save_exif_action, arg));
		break;
        default:
                return (GP_ERROR_NOT_SUPPORTED);
        }

	return (GP_OK);
}

OPTION_CALLBACK (get_file)
{
        return get_file_common (arg, GP_FILE_TYPE_NORMAL);
}

OPTION_CALLBACK (get_all_files)
{
        CR (set_globals ());
        CR (for_each_file (&fparams, save_file_action));

	return (GP_OK);
}

OPTION_CALLBACK (get_thumbnail)
{
        return (get_file_common (arg, GP_FILE_TYPE_PREVIEW));
}

OPTION_CALLBACK(get_all_thumbnails)
{
        CR (set_globals ());
        CR (for_each_file (&fparams, save_thumbnail_action));

	return (GP_OK);
}

OPTION_CALLBACK (get_raw_data)
{
        return (get_file_common (arg, GP_FILE_TYPE_RAW));
}

OPTION_CALLBACK (get_all_raw_data)
{
        CR (set_globals ());
        CR (for_each_file (&fparams, save_raw_action));

	return (GP_OK);
}

OPTION_CALLBACK (get_audio_data)
{
	return (get_file_common (arg, GP_FILE_TYPE_AUDIO));
}

OPTION_CALLBACK (get_all_audio_data)
{
	CR (set_globals ());
	CR (for_each_file (&fparams, save_audio_action));

	return (GP_OK);
}

OPTION_CALLBACK (delete_file)
{
	CR (set_globals ());

	if (strchr (arg, '.'))
		return (delete_file_action (&aparams, arg));

	return (for_each_file_in_range (&fparams, delete_file_action,
					arg));
}

OPTION_CALLBACK (delete_all_files)
{
	CR (set_globals ());
	CR (for_each_folder (&fparams, delete_all_action));

	return (GP_OK);
}

OPTION_CALLBACK (upload_file)
{
        CameraFile *file;
        int res;

        gp_log (GP_LOG_DEBUG, "main", "Uploading file...");

        CR (set_globals ());

        CR (gp_file_new (&file));
	res = gp_file_open (file, arg);
	if (res < 0) {
		gp_file_unref (file);
		return (res);
	}

	/* Check if the user specified a filename */
	if (strcmp (glob_filename, "")) {
		res = gp_file_set_name (file, glob_filename);
		if (res < 0) {
			gp_file_unref (file);
			return (res);
		}
	}

        res = gp_camera_folder_put_file (glob_camera, glob_folder, file,
					 glob_context);
        gp_file_unref (file);

        return (res);
}

OPTION_CALLBACK (make_dir)
{
	CR (set_globals ());

	CR (gp_camera_folder_make_dir (glob_camera, glob_folder, arg, 
						 glob_context));

	return (GP_OK);
}

OPTION_CALLBACK (remove_dir)
{
	CR (set_globals ());

	CR (gp_camera_folder_remove_dir (glob_camera, glob_folder, arg,
						   glob_context));

	return (GP_OK);
}

static int
capture_generic (CameraCaptureType type, char *name)
{
        CameraFilePath path;
        char *pathsep;
        int result;

        CR (set_globals ());

	result =  gp_camera_capture (glob_camera, type, &path, glob_context);
	if (result != GP_OK) {
		cli_error_print("Could not capture.");
		return (result);
	}

	if (strcmp(path.folder, "/") == 0)
		pathsep = "";
	else
		pathsep = "/";

	if (glob_quiet)
		printf ("%s%s%s\n", path.folder, pathsep, path.name);
	else
		printf (_("New file is in location %s%s%s on the camera\n"),
			path.folder, pathsep, path.name);

        return (GP_OK);
}


OPTION_CALLBACK (capture_image)
{
	return (capture_generic (GP_CAPTURE_IMAGE, arg));
}

OPTION_CALLBACK (capture_movie)
{
        return (capture_generic (GP_CAPTURE_MOVIE, arg));
}

OPTION_CALLBACK (capture_sound)
{
        return (capture_generic (GP_CAPTURE_SOUND, arg));
}

OPTION_CALLBACK (capture_preview)
{
	CameraFile *file;
	int result;

	CR (set_globals ());

	CR (gp_file_new (&file));
#ifdef HAVE_AA
	result = gp_cmd_capture_preview (glob_camera, file, glob_context);
#else
	result = gp_camera_capture_preview (glob_camera, file, glob_context);
#endif
	if (result < 0) {
		gp_file_unref (file);
		return (result);
	}

	result = save_camera_file_to_file (file, GP_FILE_TYPE_NORMAL);
	if (result < 0) {
		gp_file_unref (file);
		return (result);
	}

	gp_file_unref (file);

	return (GP_OK);
}

OPTION_CALLBACK(summary)
{
        CameraText buf;

        CR (set_globals ());
        CR (gp_camera_get_summary (glob_camera, &buf, glob_context));

        printf(_("Camera Summary:\n%s\n"), buf.text);

        return (GP_OK);
}

OPTION_CALLBACK (manual)
{
        CameraText buf;

        CR (set_globals ());
        CR (gp_camera_get_manual (glob_camera, &buf, glob_context));

        printf (_("Camera Manual:\n%s\n"), buf.text);

        return (GP_OK);
}

OPTION_CALLBACK (about)
{
        CameraText buf;

        CR (set_globals ());
        CR (gp_camera_get_about (glob_camera, &buf, glob_context));

        printf (_("About the library:\n%s\n"), buf.text);

        return (GP_OK);
}

/* Set/init global variables                                    */
/* ------------------------------------------------------------ */

#ifdef HAVE_PTHREAD

typedef struct _ThreadData ThreadData;
struct _ThreadData {
	Camera *camera;
	unsigned int timeout;
	CameraTimeoutFunc func;
};

static void
thread_cleanup_func (void *data)
{
	ThreadData *td = data;

	free (td);
}

static void *
thread_func (void *data)
{
	ThreadData *td = data;
	time_t t, last;

	pthread_cleanup_push (thread_cleanup_func, td);

	last = time (NULL);
	while (1) {
		t = time (NULL);
		if (t - last > td->timeout) {
			td->func (td->camera, NULL);
			last = t;
		}
		pthread_testcancel ();
	}

	pthread_cleanup_pop (1);
}

static unsigned int
start_timeout_func (Camera *camera, unsigned int timeout,
		    CameraTimeoutFunc func, void *data)
{
	pthread_t tid;
	ThreadData *td;

	td = malloc (sizeof (ThreadData));
	if (!td)
		return 0;
	memset (td, 0, sizeof (ThreadData));
	td->camera = camera;
	td->timeout = timeout;
	td->func = func;

	pthread_create (&tid, NULL, thread_func, td);

	return (tid);
}

static void
stop_timeout_func (Camera *camera, unsigned int id, void *data)
{
	pthread_t tid = id;

	pthread_cancel (tid);
	pthread_join (tid, NULL);
}

#endif

static int
set_globals (void)
{
	CameraList list;
	CameraAbilitiesList *al, *al_mod;
	GPPortInfoList *il;
	GPPortInfo info;
	CameraAbilities abilities;
	int model = -1, port = -1, count, x;
	const char *name, *value;
	static int initialized = 0;
	ForEachFlags flags;

	if (initialized)
		return (GP_OK);
	initialized = 1;

        /* takes all the settings and sets up the gphoto lib */

        CR (gp_camera_new (&glob_camera));
#ifdef HAVE_PTHREAD
	gp_camera_set_timeout_funcs (glob_camera, start_timeout_func,
				     stop_timeout_func, NULL);
#endif

	CR (gp_abilities_list_new (&al));
	CR (gp_abilities_list_load (al, glob_context));

	/* Eventually override the abilities list (option usbid) */
	if (glob_usbid[4]) {
		count = 0;
		CR (gp_abilities_list_new (&al_mod));
		for (x = 0; x < gp_abilities_list_count (al); x++) {
			gp_abilities_list_get_abilities (al, x, &abilities);
			if (abilities.usb_vendor==glob_usbid[2]
					&& abilities.usb_product==glob_usbid[3]) {
				gp_log (GP_LOG_DEBUG, "main",
					"Overriding USB vendor/product id "
					"0x%x/0x%x with 0x%x/0x%x",
					abilities.usb_vendor,
					abilities.usb_product,
					glob_usbid[0], glob_usbid[1]); 
				abilities.usb_vendor  = glob_usbid[0];
				abilities.usb_product = glob_usbid[1];
				count++;
			}
			gp_abilities_list_append (al_mod, abilities);
		}

		if (count) {
			gp_log (GP_LOG_DEBUG, "main",
				"%d override(s) done.", count);
			gp_abilities_list_free (al);
			al = al_mod;
			al_mod = NULL;
		} else
			gp_abilities_list_free (al_mod);
	}

	CR (gp_port_info_list_new (&il));
	CR (gp_port_info_list_load (il));

	/* Model? */
	if (!strcmp ("", glob_model)) {
		CR (gp_abilities_list_detect (al, il, &list,
							glob_context));
		count = gp_list_count (&list);
		CR (count);
		if (count == 1) {

			/* Exactly one camera detected */
			CR (gp_list_get_name (&list, 0, &name));
			CR (gp_list_get_value (&list, 0, &value));
			model = gp_abilities_list_lookup_model (al, name);
			CR (model);
			port = gp_port_info_list_lookup_path (il, value);
			CR (port);

		} else if (!count) {

			/* No camera detected. Have a look at the settings */
			gp_setting_get ("gphoto2", "model", glob_model);
			model = gp_abilities_list_lookup_model (al, glob_model);
			if (model < 0) {
				cli_error_print (_("Please specify a model."));
				return (GP_ERROR_MODEL_NOT_FOUND);
			}
		} else {

			/* More than one camera detected */
			/*FIXME: Let the user choose from the list!*/
			CR (gp_list_get_name (&list, 0, &name));
			CR (gp_list_get_value (&list, 0, &value));
			model = gp_abilities_list_lookup_model (al, name);
			CR (model);
			port = gp_port_info_list_lookup_path (il, value);
			CR (port);
		}
	} else {
		model = gp_abilities_list_lookup_model (al, glob_model);
		CR (model);
	}
	CR (gp_abilities_list_get_abilities (al, model, &abilities));
	CR (gp_camera_set_abilities (glob_camera, abilities));
	gp_setting_set ("gphoto2", "model", abilities.model);

	/* Port? */
	if (strcmp (glob_model, "Directory Browse") && (port < 0)) {
		if (strcmp ("", glob_port)) {
			port = gp_port_info_list_lookup_path (il, glob_port);
			switch (port) {
			case GP_ERROR_UNKNOWN_PORT:
				cli_error_print (_("The port you specified "
					"('%s') can not be found. Please "
					"specify one of the ports found by "
					"'gphoto2 --list-ports' and make "
					"sure the spelling is correct "
					"(i.e. with prefix 'serial:' or "
					"'usb:')."), glob_port);
			default:
				break;
			}
			CR (port);
		} else {

			/* Let's have a look at the settings */
			gp_setting_get ("gphoto2", "port", glob_port);
			port = gp_port_info_list_lookup_path (il, glob_port);
			if (port < 0) {
				cli_error_print (_("Please specify a port."));
				return (GP_ERROR_UNKNOWN_PORT);
			}
		}
	}

	/* We do not set the port in case of "Directory Browse" */
	if (strcmp (glob_model, "Directory Browse")) {
		CR (gp_port_info_list_get_info (il, port, &info));
		CR (gp_camera_set_port_info (glob_camera, info));
		gp_setting_set ("gphoto2", "port", info.path);
	}

	gp_abilities_list_free (al);
	gp_port_info_list_free (il);

	/* 
	 * Setting of speed only makes sense for serial ports. gphoto2 
	 * knows that and will complain if we try to set the speed for
	 * ports other than serial ones. Because we are paranoid here and
	 * check every single error message returned by gphoto2, we need
	 * to make sure that we have a serial port.
	 */
	if (!strncmp (glob_port, "serial:", 7))
	        CR (gp_camera_set_port_speed (glob_camera, glob_speed));

	/* fparams.flags could have been modified by the --norecurse option */
	flags = fparams.flags;
	memset (&fparams, 0, sizeof (ForEachParams));
	fparams.flags   = flags;
	memset (&aparams,  0, sizeof (ActionParams));
	fparams.camera  = aparams.camera  = glob_camera;
	fparams.context = aparams.context = glob_context;
	fparams.folder  = aparams.folder  = glob_folder;

	return (GP_OK);
}

static void
ctx_status_func (GPContext *context, const char *format, va_list args,
		 void *data)
{
	vfprintf (stderr, format, args);
	fprintf  (stderr, "\n");
	fflush   (stderr);
}

static void
ctx_error_func (GPContext *context, const char *format, va_list args,
		void *data)
{
	fprintf  (stderr, "\n");
	fprintf  (stderr, _("*** Error ***              \n"));
	vfprintf (stderr, format, args);
	fprintf  (stderr, "\n");
	fflush   (stderr);
}


#define MAX_PROGRESS_STATES 16
#define MAX_MSG_LEN 1024

static struct {
	char message[MAX_MSG_LEN + 1];
	float target;
	unsigned long int count;
	struct {
		float  current;
		time_t time, left;
	} last;
} progress_states[MAX_PROGRESS_STATES];

static unsigned int
ctx_progress_start_func (GPContext *context, float target, 
			 const char *format, va_list args, void *data)
{
	unsigned int id, len;
	static unsigned char initialized = 0;

	if (!initialized) {
		memset (progress_states, 0, sizeof (progress_states));
		initialized = 1;
	}

	/*
	 * If the message is too long, we will shorten it. If we have less
	 * than 4 cols available, we won't display any message.
	 */
	len = (glob_cols * 0.5 < 4) ? 0 : MIN (glob_cols * 0.5, MAX_MSG_LEN);

	/* Remember message and target. */
	for (id = 0; id < MAX_PROGRESS_STATES; id++)
		if (!progress_states[id].target)
			break;
	if (id == MAX_PROGRESS_STATES)
		id--;
	progress_states[id].target = target;
	vsnprintf (progress_states[id].message, len + 1, format, args);
	progress_states[id].count = 0;
	progress_states[id].last.time = time (NULL);
	progress_states[id].last.current = progress_states[id].last.left = 0.;

	/* If message is too long, shorten it. */
	if (progress_states[id].message[len - 1]) {
		progress_states[id].message[len - 1] = '\0';
		progress_states[id].message[len - 2] = '.';
		progress_states[id].message[len - 3] = '.';
		progress_states[id].message[len - 4] = '.';
	}

	return (id);
}

static void
ctx_progress_update_func (GPContext *context, unsigned int id,
			  float current, void *data)
{
	static const char spinner[] = "\\|/-";
	unsigned int i, width, pos;
	float rate;
	char remaining[10], buf[4];
	time_t sec = 0;

	/* Guard against buggy camera drivers */
	if (id >= MAX_PROGRESS_STATES || id < 0)
		return;

	/* How much time until completion? */
	if ((time (NULL) - progress_states[id].last.time > 0) &&
	    (current - progress_states[id].last.current > 0)) {
		rate = (time (NULL) - progress_states[id].last.time) /
		       (current - progress_states[id].last.current);
		sec = (MAX (0, progress_states[id].target - current)) * rate;
		if (progress_states[id].last.left) {
			sec += progress_states[id].last.left;
			sec /= 2.;
		}
		progress_states[id].last.time = time (NULL);
		progress_states[id].last.current = current;
		progress_states[id].last.left = sec;
	} else
		sec = progress_states[id].last.left;
	memset (remaining, 0, sizeof (remaining));
	if ((int) sec >= 3600) {
		snprintf (buf, sizeof (buf), "%2ih", (int) sec / 3600);
		sec -= ((int) (sec / 3600) * 3600);
		strncat (remaining, buf, sizeof (remaining) - 1);
	}
	if ((int) sec >= 60) {
		snprintf (buf, sizeof (buf), "%2im", (int) sec / 60);
		sec -= ((int) (sec / 60) * 60);
		strncat (remaining, buf, sizeof (remaining) - 1);
	}
	if ((int) sec) {
		snprintf (buf, sizeof (buf), "%2is", (int) sec);
		strncat (remaining, buf, sizeof (remaining) - 1);
	}

	/* Determine the width of the progress bar and the current position */
	width = MAX (0, (int) glob_cols -
				strlen (progress_states[id].message) - 20);
	pos = MIN (width, (MIN (current / progress_states[id].target, 100.) * width) + 0.5);

	/* Print the progress bar */
	fprintf (stdout, "%s |", progress_states[id].message);
	for (i = 0; i < width; i++)
		fprintf (stdout, (i < pos) ? "-" : " ");
	if (pos == width)
		fputc ('|', stdout);
	else
		fputc (spinner[progress_states[id].count & 0x03], stdout);
	progress_states[id].count++;

	fprintf (stdout, " %5.1f%% %9.9s\r",
		 current / progress_states[id].target * 100., remaining);
	fflush (stdout);
}

static void
ctx_progress_stop_func (GPContext *context, unsigned int id, void *data)
{
	unsigned int i;

	/* Guard against buggy camera drivers */
	if (id >= MAX_PROGRESS_STATES || id < 0)
		return;

	/* Clear the progress bar. */
	for (i = 0; i < glob_cols; i++)
		fprintf (stdout, " ");
	fprintf (stdout, "\r");
	fflush (stdout);

	progress_states[id].target = 0.;
}

static GPContextFeedback
ctx_cancel_func (GPContext *context, void *data)
{
	if (glob_cancel) {
		return (GP_CONTEXT_FEEDBACK_CANCEL);
		glob_cancel = 0;
	} else
		return (GP_CONTEXT_FEEDBACK_OK);
}

static void
ctx_message_func (GPContext *context, const char *format, va_list args,
		  void *data)
{
	int c;

	vprintf (format, args);
	fprintf (stdout, "\n");

	/* Only ask for confirmation if the user can give it. */
	if (isatty (STDOUT_FILENO) && isatty (STDIN_FILENO)) {
		fprintf (stdout, _("Press any key to continue.\n"));
		fflush (stdout);
		c = fgetc (stdin);
	} else
		fflush (stdout);
}

static int
init_globals (void)
{
        glob_option_count = 0;

	memset (glob_model, 0, sizeof (glob_model));
	memset (glob_filename, 0, sizeof (glob_filename));

        strcpy (glob_port, "");
        strcpy (glob_folder, "/");
	strcpy (glob_usb_usermap_script, "usbcam");
        if (!getcwd (glob_owd, 1023))
                strcpy (glob_owd, "./");
        strcpy (glob_cwd, glob_owd);

        glob_camera = NULL;
        glob_speed = 0;
        glob_quiet = 0;

	glob_context = gp_context_new ();
	gp_context_set_cancel_func    (glob_context, ctx_cancel_func,   NULL);
	gp_context_set_error_func     (glob_context, ctx_error_func,    NULL);
	gp_context_set_status_func    (glob_context, ctx_status_func,   NULL);
	gp_context_set_message_func   (glob_context, ctx_message_func,  NULL);

	/* Report progress only if user will see it. */
	if (isatty (STDOUT_FILENO))
		gp_context_set_progress_funcs (glob_context,
			ctx_progress_start_func, ctx_progress_update_func,
			ctx_progress_stop_func, NULL);

#ifndef DISABLE_DEBUGGING
	/* now is time zero for debug log time stamps */
	gettimeofday (&glob_tv_zero, NULL);
	/* here we could output the current time if we wanted to */
#endif

	return GP_OK;
}

/* Misc functions                                                       */
/* ------------------------------------------------------------------   */

void
cli_error_print (char *format, ...)
{
        va_list         pvar;

        fprintf(stderr, _("ERROR: "));
        va_start(pvar, format);
        vfprintf(stderr, format, pvar);
        va_end(pvar);
        fprintf(stderr, "\n");
}

static void
signal_resize (int signo)
{
	const char *columns;

	columns = getenv ("COLUMNS");
	if (columns && atoi (columns))
		glob_cols = atoi (columns);
}

static void
signal_exit (int signo)
{
	/* If we already were told to cancel, abort. */
	if (glob_cancel) {
		if (!glob_quiet)
			printf (_("\nAborting...\n"));
		if (glob_camera)
			gp_camera_unref (glob_camera);
		if (glob_context)
			gp_context_unref (glob_context);
		if (!glob_quiet)
			printf (_("Aborted.\n"));
		exit (EXIT_FAILURE);
	}

        if (!glob_quiet)
                printf (_("\nCancelling...\n"));

	glob_cancel = 1;

#if 0
        if (strlen (glob_owd) > 0)
                chdir (glob_owd);

        exit (EXIT_SUCCESS);
#endif
}

/* Main :)                                                              */
/* -------------------------------------------------------------------- */

int
main (int argc, char **argv)
{
        int result;

	/* For translation */
	setlocale (LC_ALL, "");
        bindtextdomain (PACKAGE, GPHOTO2_LOCALEDIR);
        textdomain (PACKAGE);

	/* Figure out the width of the terminal and watch out for changes */
	signal_resize (0);
	signal (SIGWINCH, signal_resize);

        /* Initialize the globals */
        init_globals ();

        signal (SIGINT, signal_exit);

        /* Count the number of command-line options we have */
        while ((strlen(option[glob_option_count].short_id) > 0) ||
               (strlen(option[glob_option_count].long_id)  > 0)) {
                glob_option_count++;
        }

        /* Peek ahead: Turn on quiet if they gave the flag */
        if (option_is_present ("q", argc, argv) == GP_OK)
                glob_quiet = 1;

#ifdef OS2 /*check if environment is set otherwise quit*/
        if(CAMLIBS==NULL)
        {
printf(_("gPhoto2 for OS/2 requires you to set the enviroment value CAMLIBS \
to the location of the camera libraries. \
e.g. SET CAMLIBS=C:\\GPHOTO2\\CAM\n"));
                exit(EXIT_FAILURE);
        }
#endif

#ifdef OS2 /*check if environment is set otherwise quit*/
        if(IOLIBS==NULL)
        {
printf(_("gPhoto2 for OS/2 requires you to set the enviroment value IOLIBS \
to the location of the io libraries. \
e.g. SET IOLIBS=C:\\GPHOTO2\\IOLIB\n"));
                exit(EXIT_FAILURE);
        }
#endif

        /* Peek ahead: Make sure we were called correctly */
        if ((argc == 1) || (verify_options (argc, argv) != GP_OK)) {
                if (!glob_quiet)
                        usage ();
                exit (EXIT_FAILURE);
        }

	/*
	 * Recursion is too dangerous for deletion. Only turn it on if
	 * explicitely specified.
	 */
	if ((option_is_present ("delete-all-files", argc, argv) == GP_OK) ||
	    (option_is_present ("D", argc, argv) == GP_OK)) {
		if (option_is_present ("recurse", argc, argv) == GP_OK)
			fparams.flags |= FOR_EACH_FLAGS_RECURSE;
		else
			fparams.flags &= ~FOR_EACH_FLAGS_RECURSE;
	}

	/*
	 * If we delete single files, do it in the reverse order. It should
	 * work the other way, too, but one never knows...
	 */
	if ((option_is_present ("delete-file", argc, argv) == GP_OK) ||
	    (option_is_present ("d", argc, argv) == GP_OK))
		fparams.flags |= FOR_EACH_FLAGS_REVERSE;
	else
		fparams.flags &= ~FOR_EACH_FLAGS_REVERSE;

	/* Now actually do something. */
	result = execute_options (argc, argv);
        if (result < 0) {
		switch (result) {
		case GP_ERROR_CANCEL:
			printf (_("Operation cancelled.\n"));
			break;
		default:
			fprintf (stderr, _("*** Error ('%s') ***       \n\n"),
				 gp_result_as_string (result));
#ifndef DISABLE_DEBUGGING
			if (option_is_present ("debug", argc, argv) != GP_OK) {
				int n;
				printf (_(
	"For debugging messages, please use the --debug option.\n"
	"Debugging messages may help finding a solution to your problem.\n"
	"If you intend to send any error or debug messages to the gphoto\n"
	"developer mailing list <gphoto-devel@gphoto.org>, please run\n"
	"gphoto2 as follows:\n\n"));

				/*
				 * print the exact command line to assist
				 * l^Husers
				 */
				printf ("    env LANG=C gphoto2 --debug");
				for (n = 1; n < argc; n++) {
					if (argv[n][0] == '-')
						printf(" %s",argv[n]);
					else
						printf(" \"%s\"",argv[n]);
				}
				printf ("\n\n");
				printf ("Please make sure there is sufficient quoting "
					"around the arguments.\n\n");
			}
#endif

			/* Clean up and exit */
			if (glob_camera) {
				gp_camera_unref (glob_camera);
				glob_camera = NULL;
			}
			if (glob_context) {
				gp_context_unref (glob_context);
				glob_context = NULL;
			}
			exit (EXIT_FAILURE);
		}
	}

	/* Clean up */
	if (glob_camera) {
		gp_camera_unref (glob_camera);
		glob_camera = NULL;
	}
	if (glob_context) {
		gp_context_unref (glob_context);
		glob_context = NULL;
	}

        return (EXIT_SUCCESS);
}
