/* $Id$ */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#ifndef WIN32
#include <signal.h>
#endif

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "main.h"

#include "actions.h"
#include "foreach.h"
#include "interface.h"
#include "options.h"
#include "range.h"
#include "shell.h"
#include "test.h"

#ifdef ENABLE_NLS
#  include <libintl.h>
#endif

/* Initializes the globals */
int  init_globals();

/* Takes the current globals, and sets up the gPhoto lib with them */
int  set_globals();

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
OPTION_CALLBACK(help);
OPTION_CALLBACK(test);
OPTION_CALLBACK(shell);
OPTION_CALLBACK(list_cameras);
OPTION_CALLBACK(list_ports);
OPTION_CALLBACK(filename);
OPTION_CALLBACK(port);
OPTION_CALLBACK(speed);
OPTION_CALLBACK(model);
OPTION_CALLBACK(quiet);
OPTION_CALLBACK(debug);
OPTION_CALLBACK(use_folder);
OPTION_CALLBACK(recurse);
OPTION_CALLBACK(use_stdout);
OPTION_CALLBACK(use_stdout_size);
OPTION_CALLBACK(list_folders);
OPTION_CALLBACK(list_files);
OPTION_CALLBACK(num_pictures);
OPTION_CALLBACK(get_picture);
OPTION_CALLBACK(get_all_pictures);
OPTION_CALLBACK(get_thumbnail);
OPTION_CALLBACK(get_all_thumbnails);
OPTION_CALLBACK(delete_picture);
OPTION_CALLBACK(delete_all_pictures);
OPTION_CALLBACK(upload_picture);
OPTION_CALLBACK(capture_image);
OPTION_CALLBACK(capture_preview);
OPTION_CALLBACK(capture_movie);
OPTION_CALLBACK(capture_sound);
OPTION_CALLBACK(summary);
OPTION_CALLBACK(manual);
OPTION_CALLBACK(about);

/* 2) Add an entry in the option table                          */
/*    ----------------------------------------------------------------- */
/*    Format for option is:                                             */
/*     {"short", "long", "argument", "description", callback_function, required}, */
/*    if it is just a flag, set the argument to "".                     */
/*    Order is important! Options are exec'd in the order in the table! */

Option option[] = {

/* Settings needed for formatting output */
{"",  "debug",          "",             "Turn on debugging",            debug,          0},
{"q", "quiet",          "",             "Quiet output (default=verbose)",quiet,         0},

/* Display and die actions */

{"h", "help",           "",             "Displays this help screen",    help,           0},
{"",  "verify",         "",             "Verifies gPhoto installation", test,           0},
{"",  "list-cameras",   "",             "List supported camera models", list_cameras,   0},
{"",  "list-ports",     "",             "List supported port devices",  list_ports,     0},
{"",  "stdout",         "",             "Send file to stdout",          use_stdout,     0},
{"",  "stdout-size",    "",             "Print filesize before data",   use_stdout_size,0},

/* Settings needed for camera functions */
{"" , "port",           "path",         "Specify port device",          port,           0},
{"" , "speed",          "speed",        "Specify serial transfer speed",speed,          0},
{"" , "camera",         "model",        "Specify camera model",         model,          0},
{"" , "filename",       "filename",     "Specify a filename",           filename,       0},

/* Actions that depend on settings */
{"a", "abilities",      "",             "Display camera abilities",     abilities,      0},
{"f", "folder",         "folder",       "Specify camera folder (default=\"/\")",use_folder,0},
{"R", "recurse",        "",             "Recursively descend through folders",recurse,  0},
{"l", "list-folders",   "",             "List folders in folder",       list_folders,   0},
{"L", "list-files",     "",             "List files in folder",         list_files,     0},
{"n", "num-pictures",   "",             "Display number of pictures",   num_pictures,   0},
{"p", "get-picture",    "range",        "Get pictures given in range",  get_picture,    0},
{"P", "get-all-pictures","",            "Get all pictures from folder", get_all_pictures,0},
{"t", "get-thumbnail",  "range",        "Get thumbnails given in range",get_thumbnail,  0},
{"T", "get-all-thumbnails","",          "Get all thumbnails from folder",get_all_thumbnails,0},
{"d", "delete-picture", "range",        "Delete pictures given in range", delete_picture, 0},
{"D", "delete-all-pictures","",         "Delete all pictures in folder",delete_all_pictures,0},
{"u", "upload-picture", "filename",     "Upload a picture to camera",   upload_picture, 0},
{"" , "capture-preview","",             "Capture a quick preview",      capture_preview, 0},
{"" , "capture-image",  "resolution",   "Capture an image",             capture_image,  0},
{"" , "capture-movie",  "resolution:duration", "Capture a movie ",             capture_movie,  0},
{"" , "capture-sound",  "resolution:duration", "Capture an audio clip",        capture_sound,  0},
{"",  "summary",        "",             "Summary of camera status",     summary,        0},
{"",  "manual",         "",             "Camera driver manual",         manual,         0},
{"",  "about",          "",             "About the camera driver",      about,          0},
{"",  "shell",          "",             "gPhoto shell",                 shell,          0},

/* End of list                  */
{"" , "",               "",             "",                             NULL,           0}
};

/* 3) Add any necessary global variables                                */
/*    ----------------------------------------------------------------- */
/*    Most flags will set options, like choosing a port, camera model,  */
/*    etc...                                                            */

int  glob_option_count;
char glob_port[128];
char glob_model[64];
char glob_folder[1024];
char glob_owd[1024];
char glob_cwd[1024];
int  glob_speed;
int  glob_num=1;

Camera         *glob_camera=NULL;

extern int  glob_debug;
int  glob_shell=0;
int  glob_quiet=0;
int  glob_filename_override=0;
int  glob_recurse=0;
char glob_filename[128];
int  glob_stdout=0;
int  glob_stdout_size=0;

/* 4) Finally, add your callback function.                              */
/*    ----------------------------------------------------------------- */
/*    The callback function is passed "char *arg" to the argument of    */
/*    command-line option. It must return GP_OK or GP_ERROR.            */
/*    Again, use the OPTION_CALLBACK(function) macro.                   */
/*    glob_debug is set if the user types in the "--debug" flag. Use    */
/*    cli_debug_print(char *format, ...) to display debug output. Use   */
/*    cli_error_print(char *format, ...) to display error output.       */


OPTION_CALLBACK(help) {

        cli_debug_print("Displaying usage");

        usage();
        exit(EXIT_SUCCESS);
        return GP_OK;
}

OPTION_CALLBACK(test) {

        cli_debug_print("Testing gPhoto Installation");
        test_gphoto();
        exit(EXIT_SUCCESS);
        return GP_OK;
}

OPTION_CALLBACK(use_stdout) {

        glob_quiet  = 1; /* implied */
        glob_stdout = 1;

        return GP_OK;
}

OPTION_CALLBACK(use_stdout_size) {

        glob_stdout_size = 1;
        use_stdout(arg);

        return GP_OK;
}

OPTION_CALLBACK(abilities) {

        int x=0;
        int result;
        CameraAbilities abilities;

        if (strlen(glob_model)==0) {
                cli_error_print("Must specify a camera model using \"%scamera model\"",LONG_OPTION);
                return (GP_ERROR);
        }

        if ((result = gp_camera_abilities_by_name(glob_model, &abilities)) != GP_OK) {
                cli_error_print("Could not find camera \"%s\".\nUse \"--list-cameras\" to see available camera models", glob_model);
                return (result);
        }

        /* Output a parsing friendly abilities table. Split on ":" */

        printf("Abilities for camera:            : %s\n",
                abilities.model);
        printf("Serial port support              : %s\n",
                SERIAL_SUPPORTED(abilities.port)? "yes":"no");
        printf("Parallel port support            : %s\n",
                PARALLEL_SUPPORTED(abilities.port)? "yes":"no");
        printf("USB support                      : %s\n",
                USB_SUPPORTED(abilities.port)? "yes":"no");
        printf("IEEE1394 support                 : %s\n",
                IEEE1394_SUPPORTED(abilities.port)? "yes":"no");
        printf("Network support                  : %s\n",
                NETWORK_SUPPORTED(abilities.port)? "yes":"no");

        if (abilities.speed[0] != 0) {
        printf("Transfer speeds supported        :\n");
                do {
        printf("                                 : %i\n", abilities.speed[x]);
                        x++;
                } while (abilities.speed[x]!=0);
        }
        printf("Capture choices                  :\n");
        if (abilities.operations & GP_OPERATION_CAPTURE_IMAGE)
            printf("                                 : Image\n");
        if (abilities.operations & GP_OPERATION_CAPTURE_VIDEO)
            printf("                                 : Video\n");
        if (abilities.operations & GP_OPERATION_CAPTURE_AUDIO)
            printf("                                 : Audio\n");
	if (abilities.operations & GP_OPERATION_CAPTURE_PREVIEW)
	    printf("                                 : Preview\n");
        printf("Configuration support            : %s\n",
                abilities.operations & GP_OPERATION_CONFIG? "yes":"no");
        printf("Delete files on camera support   : %s\n",
                abilities.file_operations & GP_FILE_OPERATION_DELETE? "yes":"no");
        printf("File preview (thumbnail) support : %s\n",
                abilities.file_operations & GP_FILE_OPERATION_PREVIEW? "yes":"no");
        printf("File upload support              : %s\n",
                abilities.folder_operations & GP_FOLDER_OPERATION_PUT_FILE? "yes":"no");

        return (GP_OK);
}


OPTION_CALLBACK(list_cameras) {

        int x, n;
        char buf[64];

        cli_debug_print("Listing Cameras");

        if ((n = gp_camera_count()) < 0)
                cli_error_print("Could not retrieve the number of supported cameras!");
        if (glob_quiet)
                printf("%i\n", n);
           else
                printf("Number of supported cameras: %i\nSupported cameras:\n", n);

        for (x=0; x<n; x++) {
                if (gp_camera_name(x, buf) != GP_OK)
                        cli_error_print("Could not retrieve the name of camera!");
                if (glob_quiet)
                        printf("%s\n", buf);
                   else
                        printf("\t\"%s\"\n", buf);
        }

        return (GP_OK);
}

OPTION_CALLBACK(list_ports) {

        gp_port_info info;
        int x, count;

        if ((count = gp_port_count_get()) < 0) {
                cli_error_print("Could not get number of ports");
                return (count);
        }
        if (!glob_quiet) {
          printf("Devices found: %i\n", count);
          printf("Path                             Description\n"
                 "--------------------------------------------------------------\n");
        } else
          printf("%i\n", count);
        for(x=0; x<count; x++) {
                gp_port_info_get(x, &info);
                printf("%-32s %-32s\n",info.path,info.name);
        }

        return (GP_OK);
}

OPTION_CALLBACK(filename) {

        cli_debug_print("Setting filename to %s", arg);

        strcpy(glob_filename, arg);
        glob_filename_override = 1;

        return (GP_OK);
}

OPTION_CALLBACK(port) {

        cli_debug_print("Setting port to %s", arg);

        strcpy(glob_port, arg);

        return (GP_OK);
}

OPTION_CALLBACK(speed) {

        cli_debug_print("Setting speed to %s", arg);

        glob_speed = atoi(arg);

        return (GP_OK);
}

OPTION_CALLBACK(model) {

        cli_debug_print("Setting camera model to %s", arg);

        strcpy(glob_model, arg);

        return (GP_OK);
}

OPTION_CALLBACK(debug) {

        glob_debug = 1;
        cli_debug_print("Turning on debug mode");

        return (GP_OK);
}

OPTION_CALLBACK(quiet) {

        cli_debug_print("Setting to quiet mode");

        glob_quiet=1;

        return (GP_OK);
}

OPTION_CALLBACK(shell) {

        int result;

        cli_debug_print("Entering shell mode");

        if ((result = set_globals()) != GP_OK)
                return (result);

        glob_shell = 1;

        return (shell_prompt());

}

OPTION_CALLBACK(use_folder) {

        cli_debug_print("Setting folder to %s", arg);

        strcpy(glob_folder, arg);

        return (GP_OK);
}

OPTION_CALLBACK(recurse) {

        cli_debug_print("Setting recursive mode");

        glob_recurse = 1;

        return (GP_OK);
}

OPTION_CALLBACK(list_folders) {

        int result;

        if ((result = set_globals()) != GP_OK)
                return (result);

        if (!glob_quiet)
                printf("Subfolders of \"%s\":\n", glob_folder);

        return for_each_subfolder(glob_folder, print_folder, NULL, glob_recurse);
}

OPTION_CALLBACK(list_files) {

        int result;

        if ((result = set_globals()) != GP_OK)
                return (result);

        if ((result = print_files (glob_folder, NULL, 0)) != GP_OK)
                return (result);

        if (!glob_recurse)
                return (GP_OK);

        return for_each_subfolder(glob_folder, print_files, NULL, glob_recurse);
}

OPTION_CALLBACK(num_pictures) {

        CameraList list;
        int res;

        cli_debug_print ("Counting pictures");

	res = set_globals();
	if (res != GP_OK)
		return (res);

	res = gp_camera_folder_list_files (glob_camera, glob_folder, &list);
	if (res != GP_OK)
		return (res);

        if (glob_quiet)
                printf ("%i\n", gp_list_count(&list));
           else
                printf ("Number of pictures in folder %s: %i\n", 
			glob_folder, gp_list_count(&list));

        return (GP_OK);
}

int save_camera_file_to_file (CameraFile *file, int thumbnail) {

        char out_filename[1024], out_folder[1024], buf[1024], msg[1024];
        int result;
        char *f;
        int resp1, resp2;

        /* Determine the folder and filename */
        strcpy(out_filename, "");
        strcpy(out_folder, "");

        if (glob_filename_override) {
                if (strchr(glob_filename, '/')) {               /* Check if they specified an output dir */
                        f = strrchr(glob_filename, '/');
                        strcpy(buf, f+1);                       /* Get the filename */
                        sprintf(out_filename, buf, glob_num++);
                        *f = 0;
                        strcpy(out_folder, glob_filename);      /* Get the folder */
                        strcat(out_folder, "/");
                        *f = '/';
                } else {                                        /* If not, subst and set */
                        sprintf(out_filename, glob_filename, glob_num++);
                }
        } else {
                strcat(out_filename, file->name);
        }

        if (thumbnail)
                sprintf(buf, "%sthumb_%s", out_folder, out_filename);
           else
                sprintf(buf, "%s%s", out_folder, out_filename);

        if (!glob_quiet) {
                while (GP_SYSTEM_IS_FILE(buf)) {
                        sprintf(msg, "File %s exists. Overwrite?", buf);
                        resp1 = gp_frontend_confirm(glob_camera, msg);
                        if ((resp1==GP_CONFIRM_NO)||(resp1==GP_CONFIRM_NOTOALL)) {
                                resp2 = gp_frontend_confirm(glob_camera, "Specify new filename?");
                                if ((resp2==GP_CONFIRM_NO)||(resp2==GP_CONFIRM_NOTOALL)) {
                                        gp_file_free(file);
                                        return (GP_OK);
                                }
                                printf("Enter new filename: ");
                                fflush(stdout);
                                fgets(buf, 1023, stdin);
                                buf[strlen(buf)-1]=0;
                        } else {
                                break;
                        }
                }
                printf("Saving %s as %s\n", thumbnail ? "preview" : "image", buf);
        }
        if ((result = gp_file_save(file, buf)) != GP_OK)
                cli_error_print("Can not save %s as %s", thumbnail ? "preview" : "image", buf);

        return (result);
}


int save_picture_to_file(char *folder, char *filename, int thumbnail) {

        int res;

        CameraFile *file;

        file = gp_file_new();

        if (thumbnail) {
		res = gp_camera_file_get_preview (glob_camera, folder, 
						  filename, file);
		if (res != GP_OK) {
                        cli_error_print ("Can not get preview for %s.", 
					 filename);
                        return (res);
                }
         } else {
	 	res = gp_camera_file_get_file (glob_camera, folder, filename, 
					       file);
		if (res != GP_OK) {
                        cli_error_print("Can not get picture %s", filename);
                        return (res);
                }
        }

        if (glob_stdout) {
                if (glob_stdout_size)
                        printf ("%li\n", file->size);
                fwrite (file->data, sizeof(char), file->size, stdout);
                gp_file_free (file);
                return (GP_OK);
        }

        res = save_camera_file_to_file (file, thumbnail);

        gp_file_free (file);

        return (res);
}


/*
  get_picture_common() - parse range, download specified images, or their
        thumbnails according to thumbnail argument, and save to files.
*/

int get_picture_common(char *arg, int thumbnail) {

        int result;

        if (thumbnail)
                cli_debug_print("Getting thumbnail(s) %s", arg);
        else
                cli_debug_print("Getting picture(s) %s", arg);

        if ((result = set_globals()) != GP_OK)
                return (result);

        if (thumbnail && !glob_camera->abilities->file_operations & GP_FILE_OPERATION_PREVIEW) {
                cli_error_print("Camera doesn't support picture thumbnails");
                return (GP_ERROR_NOT_SUPPORTED);
        }

        if (strchr(arg, '.'))
                return (save_picture_to_file(glob_folder, arg, thumbnail));

        if (thumbnail)
                return for_each_image_in_range(glob_folder, arg, save_thumbnail_action, 0);
        else
                return for_each_image_in_range(glob_folder, arg, save_picture_action, 0);
}

OPTION_CALLBACK(get_picture) {

        return get_picture_common(arg, 0);
}

OPTION_CALLBACK(get_all_pictures) {

        int result;

        cli_debug_print("Getting all pictures");

        if ((result = set_globals()) != GP_OK)
                return (result);

        return for_each_image(glob_folder, save_picture_action, 0);
}

OPTION_CALLBACK(get_thumbnail) {

        return (get_picture_common(arg, 1));
}

OPTION_CALLBACK(get_all_thumbnails) {

        int result;

        cli_debug_print("Getting all thumbnails");

        if ((result = set_globals()) != GP_OK)
                return (result);

        if (!(glob_camera->abilities->file_operations & GP_FILE_OPERATION_PREVIEW)) {
                cli_error_print("Camera doesn't support picture thumbnails");
                return (GP_ERROR_NOT_SUPPORTED);
        }

        return for_each_image(glob_folder, save_thumbnail_action, 0);
}

OPTION_CALLBACK(delete_picture) {

        int result;

        cli_debug_print("Deleting picture(s) %s", arg);

        if ((result = set_globals()) != GP_OK)
                return (result);

        if (!(glob_camera->abilities->file_operations & GP_FILE_OPERATION_DELETE)) {
                cli_error_print("Camera can not delete pictures");
                return (GP_ERROR_NOT_SUPPORTED);
        }

        return for_each_image_in_range(glob_folder, arg, delete_picture_action, 1);
}

OPTION_CALLBACK(delete_all_pictures) {

        int result;

        cli_debug_print("Deleting all pictures");

        if ((result = set_globals()) != GP_OK)
                return (result);

        /* If the camera supports deleting all the files */
        if (glob_camera->abilities->folder_operations & GP_FOLDER_OPERATION_DELETE_ALL) {
            result = gp_camera_folder_delete_all (glob_camera, glob_folder);
            /* If that went OK, return. Otherwise, do it 1 by 1 */
            if (result == GP_OK)
                return GP_OK;
        }


        if (!(glob_camera->abilities->file_operations & GP_FILE_OPERATION_DELETE)) {
                cli_error_print("Camera can not delete pictures");
                return (GP_ERROR_NOT_SUPPORTED);
        }

        return for_each_image(glob_folder, delete_picture_action, 1);
}

OPTION_CALLBACK(upload_picture) {

        CameraFile *file;
        int res;

        file = gp_file_new();

        cli_debug_print("Uploading picture");

	res = set_globals();
	if (res != GP_OK)
		return (res);

        if (!(glob_camera->abilities->folder_operations & 
	      GP_FOLDER_OPERATION_PUT_FILE))
                return (GP_ERROR_NOT_SUPPORTED);

	res = gp_file_open(file, arg);
	if (res != GP_OK) {
                cli_error_print("Could not open file %s", arg);
                return (res);
        }

	res = gp_camera_folder_put_file (glob_camera, glob_folder, file);

        gp_file_free(file);

        return (res);
}

int capture_generic (int type, char *name) {

        CameraFilePath path;
        CameraFile* file;
        int result;

        if ((result = set_globals()) != GP_OK)
                return (result);

        if (type == GP_OPERATION_CAPTURE_PREVIEW) {
		file = gp_file_new ();
            	if ((result = gp_camera_capture_preview(glob_camera, file)) != GP_OK) {
                	cli_error_print("Could not capture the preview.");
                	return (result);
        	}
        	save_camera_file_to_file(file, 0);
        	gp_file_free(file);
        } else {
		result =  gp_camera_capture (glob_camera, type, &path);
		if (result != GP_OK) {
        		cli_error_print("Could not capture.");
        		return (result);
        	}

        	if (glob_quiet)
        		printf ("%s/%s\n", path.folder, path.name);
        	else
        		printf ("New file is in location %s/%s on the camera\n", 
				path.folder, path.name);
        }

        return (GP_OK);
}


OPTION_CALLBACK(capture_image) {

        return (capture_generic (GP_OPERATION_CAPTURE_IMAGE, arg));
}

OPTION_CALLBACK(capture_movie) {

        return (capture_generic (GP_OPERATION_CAPTURE_VIDEO, arg));
}

OPTION_CALLBACK(capture_sound) {

        return (capture_generic (GP_OPERATION_CAPTURE_AUDIO, arg));
}

OPTION_CALLBACK(capture_preview) {

        return (capture_generic (GP_OPERATION_CAPTURE_PREVIEW, arg));
}

OPTION_CALLBACK(summary) {

        CameraText buf;
        int res;

	res = set_globals ();
	if (res != GP_OK)
		return (res);

	res = gp_camera_get_summary (glob_camera, &buf);
	if (res != GP_OK)
		return (res);

        printf("Camera Summary:\n%s\n", buf.text);


        return (GP_OK);
}

OPTION_CALLBACK(manual) {

        int res;
        CameraText buf;

	res = set_globals ();
	if (res != GP_OK)
		return (res);

	res = gp_camera_get_manual (glob_camera, &buf);
	if (res != GP_OK)
		return (res);

        printf ("Camera Manual:\n%s\n", buf.text);

        return (GP_OK);
}

OPTION_CALLBACK(about) {

        int res;
        CameraText buf;

	res = set_globals ();
	if (res != GP_OK)
		return (res);

	res = gp_camera_get_about (glob_camera, &buf);
	if (res != GP_OK)
		return (res);

        printf ("About the library:\n%s\n", buf.text);

        return (GP_OK);
}

/* Set/init global variables                                    */
/* ------------------------------------------------------------ */

int set_globals () {

        int result;

        /* takes all the settings and sets up the gphoto lib */

//      if (strlen(glob_model) == 0) {
//              cli_error_print("Must specify a camera model using \"%scamera model\"",LONG_OPTION);
//              return (GP_ERROR);
//      }

//      if ((strlen(glob_port) == 0)&&(strcmp(glob_model, "Directory Browse")!=0)) {
//              cli_error_print("Must specify a camera port using \"%sport path\"",LONG_OPTION);
//              return (GP_ERROR);
//      }

        if ((result = gp_camera_new(&glob_camera)) != GP_OK) {
                cli_error_print("Can not create camera data");
                return (result);
        }

        strcpy (glob_camera->model, glob_model);
        strcpy (glob_camera->port->path, glob_port);
        glob_camera->port->speed = glob_speed;

        if ((result = gp_camera_init(glob_camera)) != GP_OK) {
                cli_error_print("Can not initialize the camera \"%s\"",glob_model);
                return (result);
        }

        return (GP_OK);
}

int init_globals () {

        glob_option_count = 0;

        strcpy(glob_model, "");
        strcpy(glob_port, "");
        strcpy(glob_filename, "gphoto");
        strcpy(glob_folder, "/");
        if (!getcwd(glob_owd, 1023))
                strcpy(glob_owd, "./");
        strcpy(glob_cwd, glob_owd);

        glob_camera = NULL;
        glob_speed = 0;
        glob_debug = 0;
        glob_quiet = 0;
        glob_filename_override = 0;
        glob_recurse = 0;
        return GP_OK;
}

/* Misc functions                                                       */
/* ------------------------------------------------------------------   */

void cli_debug_print(char *format, ...) {

        va_list         pvar;

        if (glob_debug) {
                fprintf(stderr, "cli: ");
                va_start(pvar, format);
                vfprintf(stderr, format, pvar);
                va_end(pvar);
                fprintf(stderr, "\n");
        }
}

void cli_error_print(char *format, ...) {

        va_list         pvar;

        fprintf(stderr, "ERROR: ");
        va_start(pvar, format);
        vfprintf(stderr, format, pvar);
        va_end(pvar);
        fprintf(stderr, "\n");
}

void signal_exit (int signo) {

        if (!glob_quiet)
                printf("\nExiting gPhoto...\n");

        /* Exit gPhoto core */
        if (glob_camera)
               gp_camera_free(glob_camera);
        gp_exit();

        if (strlen(glob_owd)>0)
                chdir(glob_owd);

        exit(EXIT_SUCCESS);

}

/* Main :)                                                              */
/* -------------------------------------------------------------------- */

int main (int argc, char **argv) {

        int result;

#ifdef ENABLE_NLS
	bindtextdomain (PACKAGE, GPHOTO2_LOCALEDIR);
	textdomain (PACKAGE);
#endif

	/* Initialize the globals */
        init_globals();

        signal(SIGINT, signal_exit);

        /* Count the number of command-line options we have */
        while ((strlen(option[glob_option_count].short_id)>0) ||
               (strlen(option[glob_option_count].long_id)>0)) {
                glob_option_count++;
        }

        /* Peek ahead: Turn on quiet if they gave the flag */
        if (option_is_present("q", argc, argv)==GP_OK)
                glob_quiet=1;

        /* Peek ahead: Check to see if we need to turn on debugging output */
        if (option_is_present("debug", argc, argv)==GP_OK)
                glob_debug=1;

#ifdef OS2 /*check if environment is set otherwise quit*/
        if(CAMLIBS==NULL)
        {
printf("gPhoto2 for OS/2 requires you to set the enviroment value CAMLIBS \
to the location of the camera libraries. \
e.g. SET CAMLIBS=C:\\GPHOTO2\\CAM\n");
                exit(EXIT_FAILURE);
        }
#endif

#ifdef OS2 /*check if environment is set otherwise quit*/
        if(IOLIBS==NULL)
        {
printf("gPhoto2 for OS/2 requires you to set the enviroment value IOLIBS \
to the location of the io libraries. \
e.g. SET IOLIBS=C:\\GPHOTO2\\IOLIB\n");
                exit(EXIT_FAILURE);
        }
#endif

        /* Peek ahead: Make sure we were called correctly */
        if ((argc == 1)||(verify_options(argc, argv) != GP_OK)) {
                if (!glob_quiet)
                        usage();
                exit(EXIT_FAILURE);
        }

        /* Initialize gPhoto core */
        gp_init(glob_debug? GP_DEBUG_HIGH: GP_DEBUG_NONE);
        gp_frontend_register(gp_interface_status, gp_interface_progress,
                gp_interface_message, gp_interface_confirm, NULL);
        if ((result = execute_options(argc, argv)) != GP_OK) {
                printf ("gPhoto2 reported the error '%s'\n", 
			gp_camera_get_result_as_string (glob_camera, result));
                exit(EXIT_FAILURE);
        }

#ifdef OS2
       printf("\nErrors occuring beyond this point are 'expected' on OS/2\ninvestigation pending\n");
#endif

        /* Exit gPhoto core */
        if (glob_camera)
               gp_camera_free(glob_camera);
        gp_exit();
        return (EXIT_SUCCESS);
}

