/* $Id$ */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <gphoto2.h>
#include <gpio.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "main.h"
#include "test.h"
#include "interface.h"

#define MAX_IMAGE_NUMBER		1024

/* Command-line option functions */
void usage();
int  verify_options (int argc, char **argv);

/* Standardized output print functions */
void cli_debug_print(char *format, ...);
void cli_error_print(char *format, ...);

/* Initializes the globals */
int  init_globals();

/* Takes the current globals, and sets up the gPhoto lib with them */
int  set_globals();


/* Global variables */

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

/* 1) Add a forward-declaration here                                    */
/*    ----------------------------------------------------------------- */
/*    Use the OPTION_CALLBACK(function) macro.                          */

OPTION_CALLBACK(abilities);
OPTION_CALLBACK(help);
OPTION_CALLBACK(test);
OPTION_CALLBACK(daemonmode);
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
{"S", "server mode",    "",             "gPhoto server (stdin/stdout)", daemonmode,     0},
{"",  "list-cameras",   "",             "List supported camera models", list_cameras,   0},
{"",  "list-ports",     "",             "List supported port devices",  list_ports,     0},


/* Settings needed for camera functions */
{"" , "port",           "path",         "Specify port device",          port,           0},
{"" , "speed",          "speed",        "Specify serial transfer speed",speed,          0},
{"" , "camera",         "model",        "Specify camera model",         model,          0},
{"" , "filename",       "filename",     "Specify a filename",           filename,       0},

/* Actions that depend on settings */
{"a", "abilities",	"",		"Display camera abilities",	abilities, 	0},
{"f", "folder",		"folder",	"Specify camera folder (default=\"/\")",use_folder,0},
{"R", "recurse",	"",		"Recursively descend through folders",recurse,	0},
{"l", "list-folders",	"",		"List folders in folder",	list_folders,	0},
{"L", "list-files",	"",		"List files in folder",		list_files,	0},
{"n", "num-pictures",	"",		"Display number of pictures",	num_pictures,	0},
{"p", "get-picture",	"range", 	"Get pictures given in range", 	get_picture,	0},
{"P", "get-all-pictures","", 		"Get all pictures from folder", get_all_pictures,0},
{"t", "get-thumbnail",	"range", 	"Get thumbnails given in range",get_thumbnail,	0},
{"T", "get-all-thumbnails","", 		"Get all thumbnails from folder",get_all_thumbnails,0},
{"d", "delete-picture",	"range",	"Delete pictures given in range", delete_picture, 0},
{"D", "delete-all-pictures","",		"Delete all pictures in folder",delete_all_pictures,0},
{"u", "upload-picture",	"filename",	"Upload a picture to camera", 	upload_picture, 0},
{"",  "summary",	"",		"Summary of camera status",	summary,	0},
{"",  "manual",		"",		"Camera driver manual",		manual,		0},
{"",  "about",		"",		"About the camera driver",	about,		0},

/* End of list                  */
{"" , "",               "",             "",                             NULL,           0}
};

/* 3) Add any necessary global variables                                */
/*    ----------------------------------------------------------------- */
/*    Most flags will set options, like choosing a port, camera model,  */
/*    etc...                                                            */

int option_count; /* total number of command-line options */

char glob_port[128];
char glob_model[64];
char glob_folder[128];
int  glob_speed;
int  glob_num=1;

Camera         *glob_camera=NULL;
CameraAbilities glob_abilities;

int  glob_debug;
int  glob_daemon=0;
int  glob_quiet=0;
int  glob_filename_override=0;
int  glob_recurse=0;
char glob_filename[128];

/* 4) Finally, add your callback function.                              */
/*    ----------------------------------------------------------------- */
/*    The callback function is passed "char *arg" to the argument of    */
/*    command-line option. It must return GP_OK or GP_ERROR.		*/
/*    Again, use the OPTION_CALLBACK(function) macro.			*/
/*    glob_debug is set if the user types in the "--debug" flag. Use 	*/
/*    cli_debug_print(char *format, ...) to display debug output. Use 	*/
/*    cli_error_print(char *format, ...) to display error output.		*/


OPTION_CALLBACK(help) {

	cli_debug_print("Displaying usage");
	
	usage();
	exit(EXIT_SUCCESS);
}

OPTION_CALLBACK(test) {

	cli_debug_print("Testing gPhoto Installation");

        test_gphoto();
        exit(EXIT_SUCCESS);
}

OPTION_CALLBACK(abilities) {

	int x=0;

	if (strlen(glob_model)==0) {
		cli_error_print("Must specify a camera model using \"%scamera model\"",LONG_OPTION);
		return (GP_ERROR);
	}

	if (gp_camera_abilities_by_name(glob_model, &glob_abilities)==GP_ERROR) {
		cli_error_print("Could not find camera \"%s\".\nUse \"--list-cameras\" to see available camera models", glob_model);
		return (GP_ERROR);
	}

        /* Output a parsing friendly abilities table. Split on ":" */

        printf("Abilities for camera:            : %s\n",
                glob_abilities.model);
        printf("Serial port support              : %s\n",
                SERIAL_SUPPORTED(glob_abilities.port)? "yes":"no");
        printf("Parallel port support            : %s\n",
                PARALLEL_SUPPORTED(glob_abilities.port)? "yes":"no");
        printf("USB support                      : %s\n",
                USB_SUPPORTED(glob_abilities.port)? "yes":"no");
        printf("IEEE1394 support                 : %s\n",
                IEEE1394_SUPPORTED(glob_abilities.port)? "yes":"no");
        printf("Network support                  : %s\n",
                NETWORK_SUPPORTED(glob_abilities.port)? "yes":"no");

        if (glob_abilities.speed[0] != 0) {
        printf("Transfer speeds supported        :\n");
                do {
        printf("                                 : %i\n", glob_abilities.speed[x]);
                        x++;
                } while (glob_abilities.speed[x]!=0);
        }
        printf("Capture from computer support    : %s\n",
                glob_abilities.capture == 0? "no":"yes");
        printf("Configuration support            : %s\n",
                glob_abilities.config == 0? "no":"yes");
        printf("Delete files on camera support   : %s\n",
                glob_abilities.file_delete == 0? "no":"yes");
        printf("File preview (thumbnail) support : %s\n",
                glob_abilities.file_preview == 0? "no":"yes");
        printf("File upload support              : %s\n",
                glob_abilities.file_put == 0? "no":"yes");

        return (GP_OK);
}


OPTION_CALLBACK(list_cameras) {

        int x, n;
        char buf[64];

	cli_debug_print("Listing Cameras");

        if ((n = gp_camera_count())==GP_ERROR)
                cli_error_print("Could not retrieve the number of supported cameras!");
        if (glob_quiet)
                printf("%i\n", n);
           else
                printf("Number of supported cameras: %i\nSupported cameras:\n", n);

        for (x=0; x<n; x++) {
                if (gp_camera_name(x, buf)==GP_ERROR)
                        cli_error_print("Could not retrieve the name of camerglob_abilities.");
                if (glob_quiet)
                        printf("%s\n", buf);
                   else
                        printf("\t\"%s\"\n", buf);
        }

        return (GP_OK);
}

OPTION_CALLBACK(list_ports) {

        CameraPortInfo info;
        int x, count;

        if ((count = gp_port_count()) == GP_ERROR) {
                cli_error_print("Could not get number of ports");
                return (GP_ERROR);
        }
        if (!glob_quiet) {
          printf("Devices found: %i\n", count);
          printf("Path                             Description\n"
                 "--------------------------------------------------------------\n");
        } else
          printf("%i\n", count);
        for(x=0; x<count; x++) {
                gp_port_info(x, &info);
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

OPTION_CALLBACK(daemonmode) {

        char buf[1024];
        int n=0;

        glob_daemon = 1;

	while (1) {
		n = read(0, buf, 1024);
		gpfe_script(buf);
	}

	return (GP_OK);
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

/*
  folder_action - function type that performs some action on 'subfolder'. 
  	Invoked	by for_each_subfolder(). Should	return GP_OK or GP_ERROR.
*/

typedef int folder_action(char *subfolder);

/*
  image_action - function type that performs some action on num-th image
  	in the currently selected folder. Invoked by for_each_image(). 
	Image numbering is 0-based. Should return GP_OK or GP_ERROR.
*/

typedef int image_action(char *folder, char *filename);

/*
  for_each_subfolder() - call action() for every subfolder in 'folder' and, if
  	'recurse' is non-zero, in all its subfolders, recursively. If 'folder' 
	is NULL or an empty string the current folder set with -f/--folder is 
	assumed. It is also assumed that camera is already initialized, that 
	is, set_globals() was invoked. Returns GP_OK or GP_ERROR.	
*/

int for_each_subfolder(char *folder, folder_action action, int recurse) {
	
	CameraList		folderlist;
	CameraListEntry		*entry;

	int	count = 0;
	char	prefix[1024], subfolder[1024];
	int	i;
	
	prefix[0] = 0;
	subfolder[0] = 0;

	if (folder != NULL && *folder != '\0') 
		strcat(prefix, folder);
	else
		strcat(prefix, glob_folder);

	if (prefix[strlen(prefix) - 1] != '/')
			strcat(prefix, "/");
	
	/* Maybe we should rather trim last / instead of appending it? */
	gp_camera_folder_list(glob_camera, &folderlist, prefix);
			
	for (i = 0; i < gp_list_count(&folderlist); i++) {
		entry = gp_list_entry(&folderlist, i);
		sprintf(subfolder, "%s%s", prefix, entry->name);
		if (action(subfolder) == GP_ERROR) 
			return (GP_ERROR);
		if (recurse) 
			for_each_subfolder(subfolder, action, recurse);
	}

	return (GP_OK);
}

/*
  for_each_image() - call action() for every image in 'folder' and, if 'recurse'
  	is non-zero, in all its subfolders, recursively. If 'folder' is NULL or 
	an empty string the current folder set with -f/--folder is assumed. It 
	is also assumed that camera is already initialized, that is, 
	set_globals() was invoked. Returns GP_OK or GP_ERROR.
*/

int for_each_image(char *folder, image_action action, int recurse) {
	
	int	i;
	CameraList filelist;
	CameraListEntry *entry;

	gp_camera_file_list(glob_camera, &filelist, folder);

	for (i = 0; i < gp_list_count(&filelist); i++) {
		entry = gp_list_entry(&filelist, i);
		if (action(folder, entry->name) == GP_ERROR)
			return (GP_ERROR);
	}
	
	if (recurse) {
		
		int faction(char *subfolder) {
			
			return for_each_image(subfolder, action, 0);
		}
		
		return for_each_subfolder(folder, faction, recurse);
	}
			
	return (GP_OK);
}

OPTION_CALLBACK(list_folders) {

	int print_folder(char *subfolder) {
		/* print paths relative to glob_folder */
		if (strcmp(glob_folder, "/") != 0)
			printf("%s\n", subfolder + strlen(glob_folder) + 1);
		else
			printf("%s\n", subfolder + strlen(glob_folder));
			
		return (GP_OK);
	}
	
	if (set_globals() == GP_ERROR)
		return (GP_ERROR);

	if (!glob_quiet)
		printf("Subfolders of %s:\n", glob_folder);

	return for_each_subfolder(glob_folder, print_folder, glob_recurse); 
}

OPTION_CALLBACK(list_files) {

	int print_files(char *subfolder) {
		CameraList filelist;
		CameraListEntry *entry;
		int x;
		char buf[64];

		if (!glob_quiet)
			printf("Files in %s:\n", subfolder);

		gp_camera_file_list(glob_camera, &filelist, subfolder);

		for (x=0; x<gp_list_count(&filelist); x++) {
			entry = gp_list_entry(&filelist, x);
			sprintf(buf, "%i", x+1);
			printf("#%-05s %s\n", buf, entry->name);
		}
		return (GP_OK);
	}
	
	if (set_globals() == GP_ERROR)
		return (GP_ERROR);

	print_files(glob_folder);

	if (!glob_recurse)
		return (GP_OK);

	return for_each_subfolder(glob_folder, print_files, glob_recurse); 
}


OPTION_CALLBACK(num_pictures) {

	CameraList list;

	cli_debug_print("Counting pictures");

        if (set_globals() == GP_ERROR)
                return (GP_ERROR);

	if (gp_camera_file_list(glob_camera, &list, glob_folder)==GP_ERROR)
		return (GP_ERROR);
	
	if (glob_quiet)
		printf("%i\n", gp_list_count(&list));
	   else
		printf("Number of pictures in folder %s: %i\n", glob_folder, gp_list_count(&list));

	return (GP_OK);
}

/* 
  str_grab_nat() - Grab positive decimal integer (natural number) from str 
  	starting with pos byte. On return pos points to the first byte after 
	grabbed integer.
*/

int str_grab_nat(const char *str, int *pos) {
	
	int	result, old_pos = *pos;
	char 	*buf;

	if (!(buf = (char*)strdup(str))) /* we can not modify str */
		return -1;
	
	*pos += strspn(&buf[*pos], "0123456789");
	buf[*pos] = 0;
	result = atoi(&buf[old_pos]);
	free(buf);
	return result;
}

/*
  parse_range() - Intentionally, parse list of images given in range. Syntax
  	of range is:
		( m | m-n ) { , ( m | m-n ) }
	where m,n are decimal integers with 1 <= m <= n <= MAX_IMAGE_NUMBER.
	Ranges are XOR (exclusive or), so that 
		1-5,3,7
	is equivalent to
		1,2,4,5,7	
	Conversion from 1-based to 0-based numbering is performed, that is, 
	n-th image corresponds to (n-1)-th byte in index. The value of 
	index[n] is either 0 or 1. 0 means not selected, 1 selected. 
	index passed to parse_range() must be an array of MAX_IMAGE_NUMBER 
	char-s set to 0 (use memset() or bzero()).
*/
		
int parse_range(const char *range, char *index) {
	
	int	i = 0, l = -1, r = -1, j;
	
	do {
		switch (range[i]) {
			case '0'...'9':
				l = str_grab_nat(range, &i);
				if (l <= 0 || MAX_IMAGE_NUMBER < l)
					return (GP_ERROR); 
				break;
			
			case '-' :
				if (i == 0)
					return (GP_ERROR); /* '-' begins range */
				i++;		
				r = str_grab_nat(range, &i);		
				if (r <= 0 || MAX_IMAGE_NUMBER < r)
					return (GP_ERROR); 
				break;
				
			case '\0' :
				break; 
				
			case ',' :
				break; 
		
			default :
				return (GP_ERROR); /* unexpected character */
		}
	} while (range[i] != '\0' && range[i] != ',');	/* scan range till its end or ',' */
	
	if (i == 0)
		return (GP_ERROR); /* empty range or ',' at the begining */
	
	if (0 < r) { /* update range of bytes */ 
		if (r < l)
			return (GP_ERROR); /* decreasing ranges are not allowed */
		
		for (j = l; j <= r; j++)
			index[j - 1] ^= 1; /* convert to 0-based numbering */
	} 
	else  /* update single byte */
		index[l - 1] ^= 1; /* convert to 0-based numbering */

	if (range[i] == ',') 
		return parse_range(&range[i + 1], index); /* parse remainder */
	
	return (GP_OK);
}

/*
  for_each_image_in_range() - call action() for every image specified in 'range',
  	in the current folder set by -f/--folder. It is assumed that camera is 
	already initialized, that is, set_globals() was invoked. Returns GP_OK
	or GP_ERROR.
*/

int for_each_image_in_range(char *folder, char *range, image_action action) {
	
	char	index[MAX_IMAGE_NUMBER];
	int 	i, count = 0, max = 0;

	CameraList filelist;
	CameraListEntry *entry;

	memset(index, 0, MAX_IMAGE_NUMBER);
	
	if (parse_range(range, index) == GP_ERROR) {
		cli_error_print("Invalid range");
		return (GP_ERROR);
	}

	if (gp_camera_file_list(glob_camera, &filelist, glob_folder)==GP_ERROR)
		return (GP_ERROR);

	for (max = MAX_IMAGE_NUMBER - 1; !index[max]; max--) {}
	
	if (gp_list_count(&filelist) < max + 1) {
		cli_error_print("Picture number %i is too large. Available %i picture(s).", max + 1, count);
		return (GP_ERROR);
	}

	for (i = 0; i <= max; i++)
		if (index[i]) {
			entry = gp_list_entry(&filelist, i);
			if (action(folder, entry->name) == GP_ERROR)
				return (GP_ERROR);
		}
		
	return (GP_OK);
}

/*
  save_picture_to_file() - download (num+1)-th image, or its thumbnail, from 
  	the camera and save it into file. File name is given by --filename 
	option or set by library. num is 0-based.
*/

int save_picture_to_file(char *folder, char *filename, int thumbnail) {

	char out_filename[1024], buf[1024];
	CameraFile *file;

	file = gp_file_new();

	if (thumbnail) {
		if (gp_camera_file_get_preview(glob_camera, file, folder, filename) == GP_ERROR) {
			cli_error_print("Can not get preview for %s.", filename);
			return (GP_ERROR);
		}
	 } else {
		if (gp_camera_file_get(glob_camera, file, folder, filename) == GP_ERROR) {
			cli_error_print("Can not get picture %s", filename);
			return (GP_ERROR);
		}
	}
	if ((glob_filename_override)&&(strlen(glob_filename)>0))
		sprintf(out_filename, glob_filename, glob_num++);
	   else
		strcpy(out_filename, filename);

	if (thumbnail) {
		sprintf(buf, "thumb_%s", out_filename);
		strcpy(out_filename, buf);
	}

	if (!glob_quiet)
		printf("Saving %s as %s\n", thumbnail ? "preview" : "image", out_filename);
	if (gp_file_save(file, out_filename) == GP_ERROR) 
		cli_error_print("Can not save %s as %s", thumbnail ? "preview" : "image", out_filename);

	gp_file_free(file);

	return (GP_OK);
}

/*
  Commonly used image actions.
*/

int save_picture_action(char *folder, char *filename) {
		
	if (save_picture_to_file(folder, filename, 0) == GP_ERROR)
		return (GP_ERROR);		
}

int save_thumbnail_action(char *folder, char *filename) {
	
	if (save_picture_to_file(folder, filename, 1) == GP_ERROR)
		return (GP_ERROR);
}

int delete_picture_action(char *folder, char *filename) {
	
	if (gp_camera_file_delete(glob_camera, folder, filename) == GP_ERROR)
		return (GP_ERROR);		
}

/*
  get_picture_common() - parse range, download specified images, or their 
  	thumbnails according to thumbnail argument, and save to files.  
*/

int get_picture_common(char *range, int thumbnail) {

	if (thumbnail)
		cli_debug_print("Getting thumbnail(s) %s", range);
	else
		cli_debug_print("Getting picture(s) %s", range);
		
	if (set_globals() == GP_ERROR)
		return (GP_ERROR);

	if (thumbnail && !glob_abilities.file_preview) {
		cli_error_print("Camera doesn't support picture thumbnails");
		return (GP_ERROR);
	}

	if (thumbnail)
		return for_each_image_in_range(glob_folder, range, save_thumbnail_action);
	else
		return for_each_image_in_range(glob_folder, range, save_picture_action);
}

OPTION_CALLBACK(get_picture) {
	
	return get_picture_common(arg, 0);
}

OPTION_CALLBACK(get_all_pictures) {
	
	cli_debug_print("Getting all pictures");
				
	if (set_globals() == GP_ERROR)
		return (GP_ERROR);

	return for_each_image(glob_folder, save_picture_action, glob_recurse);
	/* NULL means current folder, set with -f/--folder */
}

OPTION_CALLBACK(get_thumbnail) {

	return (get_picture_common(arg, 1));
}

OPTION_CALLBACK(get_all_thumbnails) {

	cli_debug_print("Getting all thumbnails");
			
	if (set_globals() == GP_ERROR)
		return (GP_ERROR);

	if (!glob_abilities.file_preview) {
		cli_error_print("Camera doesn't support picture thumbnails");
		return (GP_ERROR);
	}
		
	return for_each_image(glob_folder, save_thumbnail_action, glob_recurse);
}

OPTION_CALLBACK(delete_picture) {

	cli_debug_print("Deleting picture(s) %s", arg);

	if (set_globals() == GP_ERROR)
		return (GP_ERROR);

	if (!glob_abilities.file_delete) {
		cli_error_print("Camera can not delete pictures");
		return (GP_ERROR);
	}

	return for_each_image_in_range(glob_folder, arg, delete_picture_action);
}

OPTION_CALLBACK(delete_all_pictures) {

	cli_debug_print("Deleting all pictures");

	if (set_globals() == GP_ERROR)
		return (GP_ERROR);

	if (!glob_abilities.file_delete) {
		cli_error_print("Camera can not delete pictures");
		return (GP_ERROR);
	}
		
	return for_each_image(glob_folder, delete_picture_action, glob_recurse);
}

OPTION_CALLBACK(upload_picture) {

	CameraFile *file;

	file = gp_file_new();

	cli_debug_print("Uploading picture");

        if (set_globals() == GP_ERROR)
                return (GP_ERROR);

	if (!glob_abilities.file_put) {
		cli_error_print("Camera doesn't support uploading pictures");
		return (GP_ERROR);
	}

	if (gp_file_open(file, arg)==GP_ERROR) {
		cli_error_print("Could not open file %s", arg);
		return (GP_ERROR);		
	}
	
	if (gp_camera_file_put(glob_camera, file, glob_folder)==GP_ERROR) {
		cli_error_print("Could not upload the picture");
		return (GP_ERROR);
	}

        gp_file_free(file);

        return (GP_OK);
}

OPTION_CALLBACK(summary) {

        CameraText buf;

        if (set_globals() == GP_ERROR)
                return (GP_ERROR);

        if (gp_camera_summary(glob_camera, &buf)==GP_ERROR) {
                cli_error_print("Could not get camera summary");
                return (GP_ERROR);
        }
        printf("Camera Summary:\n%s\n", buf.text);


        return (GP_OK);
}
OPTION_CALLBACK(manual) {

        CameraText buf;

        if (set_globals() == GP_ERROR)
                return (GP_ERROR);

        if (gp_camera_manual(glob_camera, &buf)==GP_ERROR) {
                cli_error_print("Could not get camera manual");
                return (GP_ERROR);
        }
        printf("Camera Manual:\n%s\n", buf.text);

        return (GP_OK);
}

OPTION_CALLBACK(about) {


        CameraText buf;


        if (set_globals() == GP_ERROR)
                return (GP_ERROR);

	if (gp_camera_about(glob_camera, &buf)==GP_ERROR) {
		cli_error_print("Could not get camera driver information");
		return (GP_ERROR);
	}
	printf("About the library:\n%s\n", buf.text);

        return (GP_OK);
}


int set_globals () {
	/* takes all the settings and sets up the gphoto lib */
	CameraPortInfo s;

	if (strlen(glob_model) == 0) {
		cli_error_print("Must specify a camera model using \"%scamera model\"",LONG_OPTION);
		return (GP_ERROR);
	}

	if ((strlen(glob_port) == 0)&&(strcmp(glob_model, "Directory Browse")!=0)) {
		cli_error_print("Must specify a camera port using \"%sport path\"",LONG_OPTION);
		return (GP_ERROR);
	}

	strcpy(s.path, glob_port);
	s.speed = glob_speed;

	if (gp_camera_abilities_by_name(glob_model, &glob_abilities)==GP_ERROR) {
		cli_error_print("Could not find camera \"%s\".\nUse \"--list-cameras\" to see available camera models", glob_model);
		return (GP_ERROR);
	}

	if (gp_camera_new_by_name(&glob_camera, glob_model, &s)==GP_ERROR) {
		cli_error_print("Can not initialize camera \"%s\"",glob_model);
		return (GP_ERROR);
	}

        return (GP_OK);
}

int init_globals () {

	option_count = 0;

	strcpy(glob_model, "");
	strcpy(glob_port, "");
	strcpy(glob_filename, "gphoto");
	strcpy(glob_folder, "/");

	glob_camera = NULL;
	glob_speed = 0;
	glob_debug = 0;
	glob_quiet = 0;
	glob_filename_override = 0;
	glob_recurse = 0;	
}

/* Command-line option functions                                        */
/* ------------------------------------------------------------------   */

int option_is_present (char *op, int argc, char **argv) {
        /* checks to see if op is in the command-line. it will */
        /* check for both short and long option-formats for op */

        int x, found=0;
        char s[5], l[20];

        /* look for short/long options and fill them in */
        for (x=0; x<option_count; x++) {
                if ((strcmp(op, option[x].short_id)==0)||
                    (strcmp(op, option[x].long_id)==0)) {
                        sprintf(s, "%s%s", SHORT_OPTION, option[x].short_id);
                        sprintf(l, "%s%s", LONG_OPTION, option[x].long_id);
                        found=1;
                }
        }

        /* Strictly require an option in the option table */
        if (!found)
                return (GP_ERROR);

        /* look through argv, if a match is found, return */
        for (x=1; x<argc; x++)
                if ((strcmp(s, argv[x])==0)||(strcmp(l, argv[x])==0))
                        return (GP_OK);

        return (GP_ERROR);
}

int verify_options (int argc, char **argv) {
        /* This function makes sure that all command-line options are
           valid and have the correct number of arguments */

        int x, y, match, missing_arg, which;
        char s[5], l[24], buf[32], msg[1024];

        which = 0;

	for (x=1; x<argc; x++) {
		cli_debug_print("checking \"%s\": \n", argv[x]);
		match = 0;
		missing_arg = 0;
		for (y=0; y<option_count; y++) {
			/* Check to see if the option matches */
			sprintf(s, "%s%s", SHORT_OPTION, option[y].short_id);
			sprintf(l, "%s%s", LONG_OPTION, option[y].long_id);
			if ((strcmp(s, argv[x])==0)||(strcmp(l, argv[x])==0)) {
				/* Check to see if the option requires an argument */
				if (strlen(option[y].argument)>0) {
					if (x+1 < argc) {
					   if (
				(strncmp(argv[x+1], SHORT_OPTION, strlen(SHORT_OPTION))!=0) &&
				(strncmp(argv[x+1], LONG_OPTION, strlen(LONG_OPTION))!=0)
					      ) {
						match=1;
						x++;
					   } else {
						which=y;
						missing_arg=1;
					   }
					} else {
					   missing_arg=1;
					   which=y;
					}
				}  else
					match=1;
			}
		}
		if (!match) {
			cli_error_print("Bad option \"%s\": ", argv[x]);
			if (missing_arg) {
				cli_error_print("    Missing argument. You must specify the \"%s\"",
					option[which].argument);
			}   else
				cli_error_print("    unknown option");
			return (GP_ERROR);
		}
	}

	/* Make sure required options are present */
	for (x=0; x<option_count; x++) {
	   if (option[x].required) {
		if (option_is_present(option[x].short_id, argc, argv)==GP_ERROR) {
			printf("Option %s%s is required.\n",
			 strlen(option[x].short_id)>0? SHORT_OPTION:LONG_OPTION,
			 strlen(option[x].short_id)>0? option[x].short_id:option[x].long_id);
			return (GP_ERROR);
		}
	   }
	}

        return (GP_OK);
}

int execute_options (int argc, char **argv) {

        int x, y, ret;
        char s[5], l[24];
        char *op;

        /* Execute the command-line options */
        for (x=0; x<option_count; x++) {
                sprintf(s, "%s%s", SHORT_OPTION, option[x].short_id);
                sprintf(l, "%s%s", LONG_OPTION, option[x].long_id);
                for (y=1; y<argc; y++) {
                        if ((strcmp(argv[y],s)==0)||(strcmp(argv[y],l)==0)) {
                                if (option[x].execute) {
                                   op = argv[y];
                                   if (strlen(option[x].argument) > 0) {
                                        ret=(*option[x].execute)(argv[++y]);
                                   }  else
                                        ret=(*option[x].execute)(NULL);
                                   if (ret == GP_ERROR) {
                                        // cli_error_print("Option \"%s\" did not execute properly.",op);
                                        return (GP_ERROR);
                                   }
                                }
                        }
                }
        }

        return (GP_OK);
}

void usage () {

        int x=0, y=0;
        char buf[128], s[5], l[24], a[16];

	/* Standard licensing stuff */
	printf(
	"\ngPhoto2 (v%s) - Cross-platform digital camera library.\n"
	"Copyright (C) 2000 Scott Fritzinger\n"
#ifdef OS2
        "OS/2 port by Bart van Leeuwen\n"
#endif
	"Licensed under the Library GNU Public License (LGPL).\n"
	"Usage:\n", VERSION
	);

	printf ("Short/long options (& argument)        description\n"
	        "------------------------------------------------------------------------\n");

	/* Run through option and print them out */
	while (x < option_count) {
		/* maybe sort these by short option? can't be an in-place sort.
		   would need to memcpy() to a new struct array. */
		if (strlen(option[x].short_id) > 0)
			sprintf(s, "%s%s ", SHORT_OPTION, option[x].short_id);
		   else
			sprintf(s, " ");

		if (strlen(option[x].long_id) > 0)
			sprintf(l, "%s%s", LONG_OPTION, option[x].long_id);
		   else
			sprintf(l, " ");

		if (strlen(option[x].argument) > 0)
			sprintf(a, "%s", option[x].argument);
		   else
			sprintf(a, " ");
		sprintf(buf, " %-04s %s %s", s, l, a);
		printf("%-38s %s\n", buf, option[x].description);
		x++;
	}
	printf( "------------------------------------------------------------------------\n"
	        "[Use double-quotes around arguments]     [Picture numbers begin one (1)]\n");
}

/* Misc functions							*/
/* ------------------------------------------------------------------	*/

void cli_debug_print(char *format, ...) {

	va_list		pvar;
	
	if (glob_debug) {
		fprintf(stderr, "cli: ");
		va_start(pvar, format);
		vfprintf(stderr, format, pvar);
		va_end(pvar);
		fprintf(stderr, "\n"); 
	}
}

void cli_error_print(char *format, ...) {

	va_list		pvar;

	fprintf(stderr, "ERROR: ");
	va_start(pvar, format);
	vfprintf(stderr, format, pvar);
	va_end(pvar);
	fprintf(stderr, "\n"); 
}

/* ------------------------------------------------------------------   */

int main (int argc, char **argv) {

        /* Initialize the globals */
        init_globals();

        /* Count the number of command-line options we have */
        while ((strlen(option[option_count].short_id)>0) ||
               (strlen(option[option_count].long_id)>0)) {
                option_count++;
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

        /* Peek ahead: Make sure we were called correctly */
        if ((argc == 1)||(verify_options(argc, argv)==GP_ERROR)) {
                if (!glob_quiet)
                        usage();
                exit(EXIT_FAILURE);
        }

        /* Initialize gPhoto core */
        gp_init(glob_debug);

        if (execute_options(argc, argv) == GP_ERROR) {
//              if (!glob_quiet)
//                      usage();
                exit(EXIT_FAILURE);
        }

        /* Exit gPhoto core */
       if (glob_camera)
               gp_camera_free(glob_camera);
        gp_exit();

	return (EXIT_SUCCESS);
}
