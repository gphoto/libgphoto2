#include <stdlib.h>
#include <stdio.h>
#include <gphoto2.h>
#include <gpio/gpio.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "main.h"
#include "test.h"
#include "interface.h"

/* Command-line option functions */
void usage();
int  verify_options (int argc, char **argv);

/* Standardized output print functions */
void debug_print (char *message, char *str);
void error_print (char *message);

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

/* 1) Add a forward-declaration here 					*/
/*    ----------------------------------------------------------------- */
/*    Use the OPTION_CALLBACK(function) macro.				*/

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
OPTION_CALLBACK(list_folders);
OPTION_CALLBACK(use_folder);
OPTION_CALLBACK(num_pictures);
OPTION_CALLBACK(get_picture);
OPTION_CALLBACK(get_thumbnail);
OPTION_CALLBACK(delete_picture);
OPTION_CALLBACK(upload_picture);
OPTION_CALLBACK(summary);
OPTION_CALLBACK(manual);
OPTION_CALLBACK(about);

/* 2) Add an entry in the option table 				*/
/*    ----------------------------------------------------------------- */
/*    Format for option is:						*/
/*     {"short", "long", "argument", "description", callback_function, required}, */
/*    if it is just a flag, set the argument to "".			*/
/*    Order is important! Options are exec'd in the order in the table! */

Option option[] = {

/* Settings needed for formatting output */
{"",  "debug",		"",		"Turn on debugging",		debug,		0},
{"q", "quiet",		"",		"Quiet output (default=verbose)",quiet,		0},

/* Display and die actions */
{"h", "help",		"",		"Displays this help screen",	help,		0},
{"",  "verify",		"",		"Verifies gPhoto installation",	test,		0},
{"D", "daemon",		"",		"gPhoto daemon (stdin/stdout)", daemonmode,	0},
{"",  "list-cameras",	"",		"List supported camera models",	list_cameras,	0},
{"",  "list-ports",	"",		"List supported port devices",	list_ports,	0},

/* Settings needed for camera functions */
{"" , "port",		"path",		"Specify port device",		port,		0},
{"" , "speed",		"speed",	"Specify serial transfer speed",speed,		0},
{"" , "camera",		"model",	"Specify camera model",		model,		0},
{"" , "filename",	"filename",	"Specify a filename",		filename,	0},

/* Actions that depend on settings */
{"",  "list-folders",	"",		"List all folders on the camera",list_folders,	0},
{"a", "abilities",	"",		"Display camera abilities",	abilities, 	0},
{"f", "folder",		"folder",	"Specify camera folder (default=\"/\")",use_folder,0},
{"n", "num-pictures",	"",		"Display number of pictures",	num_pictures,	0},
{"p", "get-picture",	"#", 		"Get picture # from camera", 	get_picture,	0},
{"t", "get-thumbnail",	"#", 		"Get thumbnail # from camera",	get_thumbnail,	0},
{"d", "delete-picture",	"#",		"Delete picture # from camera", delete_picture, 0},
{"u", "upload-picture",	"filename",	"Upload a picture to camera", 	upload_picture, 0},
{"",  "summary",	"",		"Summary of camera status",	summary,	0},
{"",  "manual",		"",		"Camera driver manual",		manual,		0},
{"",  "about",		"",		"About the camera driver",	about,		0},

/* End of list 			*/
{"" , "", 		"",		"",				NULL,		0}
};

/* 3) Add any necessary global variables				*/
/*    ----------------------------------------------------------------- */
/*    Most flags will set options, like choosing a port, camera model,  */
/*    etc...								*/

int option_count; /* total number of command-line options */

char glob_port[128];
char glob_model[64];
char glob_folder[128];
int  glob_speed;

CameraAbilities glob_abilities;

int  glob_debug;
int  glob_daemon=0;
int  glob_quiet=0;
int  glob_filename_override=0;
char glob_filename[128];

/* 4) Finally, add your callback function.				*/
/*    ----------------------------------------------------------------- */
/*    The callback function is passed "char *arg" to the argument of    */
/*    command-line option. It must return GP_OK or GP_ERROR.		*/
/*    Again, use the OPTION_CALLBACK(function) macro.			*/
/*    glob_debug is set if the user types in the "-d/--debug" flag. Use */
/*    debug_print(char *message, char *arg) to display debug output.Use */
/*    error_print(char *message) to display error output.		*/


OPTION_CALLBACK(help) {

	debug_print("Displaying usage", "");
	
	usage();
	exit(EXIT_SUCCESS);
}

OPTION_CALLBACK(test) {

	debug_print("Testing gPhoto Installation", "");

	test_gphoto();
	exit(EXIT_SUCCESS);
}

OPTION_CALLBACK(abilities) {

	int x=0;
	char buf[32], msg[1024];

	if (strlen(glob_model)==0) {
		sprintf(msg, "Must specify a camera model using \"%scamera model\"",LONG_OPTION);
		error_print(msg);
		return (GP_ERROR);
	}

	if (gp_camera_abilities_by_name(glob_model, &glob_abilities)==GP_ERROR) {
		sprintf(msg, "Could not find camera \"%s\".\nUse \"--list-cameras\" to see available camera models", glob_model);
		error_print(msg);
		return (GP_ERROR);
	}

	/* Output a parsing friendly abilities table. Split on ":" */

	printf("Abilities for camera:                 : %s\n", 
		glob_abilities.model);
        printf("Serial port support                   : %s\n",
                glob_abilities.serial == 0? "no":"yes");
        printf("Parallel port support                 : %s\n",
                glob_abilities.parallel == 0? "no":"yes");
        printf("USB support                           : %s\n",
                glob_abilities.usb == 0? "no":"yes");
        printf("IEEE1394 support                      : %s\n",
                glob_abilities.ieee1394 == 0? "no":"yes");

	if (glob_abilities.speed[0] != 0) {
	printf("Transfer speeds supported             :\n");
		do {	
	printf("                                      : %i\n", glob_abilities.speed[x]);
			x++;
		} while (glob_abilities.speed[x]!=0);
	}
	printf("Capture from computer support         : %s\n", 
		glob_abilities.capture == 0? "no":"yes");
	printf("Configuration support                 : %s\n", 
		glob_abilities.config == 0? "no":"yes");
	printf("Delete files on camera support        : %s\n", 
		glob_abilities.file_delete == 0? "no":"yes");
	printf("File preview (thumbnail) support      : %s\n", 
		glob_abilities.file_preview == 0? "no":"yes");
	printf("File upload support                   : %s\n", 
		glob_abilities.file_put == 0? "no":"yes");

	return (GP_OK);
}


OPTION_CALLBACK(list_cameras) {

	int x, n;
	char buf[64];

	debug_print("Listing Cameras", "");

	if ((n = gp_camera_count())==GP_ERROR)
		error_print("Could not retrieve the number of supported cameras!");
	if (glob_quiet)
		printf("%i\n", n);
	   else
		printf("Number of supported cameras: %i\nSupported cameras:\n", n);

	for (x=0; x<n; x++) {
		if (gp_camera_name(x, buf)==GP_ERROR)
			error_print("Could not retrieve the name of camerglob_abilities.");
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
		error_print("Could not get number of ports");
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

	debug_print("Setting filename to %s", arg);

	strcpy(glob_filename, arg);
	glob_filename_override = 1;

	return (GP_OK);
}

OPTION_CALLBACK(port) {

	debug_print("Setting port to %s", arg);

	strcpy(glob_port, arg);

	return (GP_OK);
}

OPTION_CALLBACK(speed) {

	debug_print("Setting speed to %s", arg);

	glob_speed = atoi(arg);

	return (GP_OK);
}

OPTION_CALLBACK(model) {

	debug_print("Setting camera model to %s", arg);

	strcpy(glob_model, arg);

	return (GP_OK);
}

OPTION_CALLBACK(debug) {

	glob_debug = 1;
	debug_print("Turning on debug mode", "");

	return (GP_OK);
}

OPTION_CALLBACK(quiet) {

	debug_print("Setting to quiet mode", "");

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

void list_folders_rec (char *path) {
	/* List all the folders on the camera recursively. */

	CameraFolderInfo fl[512];
	char buf[1024];
	int count, x=0;

	if (strcmp("<photos>", path)==0)
		return;	

	count = gp_folder_list(path, fl);
	while (x<count) {
		if (strcmp(fl[x].name, "<photos>")!=0) {
			if (strcmp(path, "/")==0)
				sprintf(buf, "/%s", fl[x].name);
			   else
				sprintf(buf, "%s/%s", path, fl[x].name);
			printf("%s\n", buf);
			list_folders_rec(buf);
		} else  {
//			printf("%s has files!\n", path);
		}
		x++;
	}

	return;
}

OPTION_CALLBACK(list_folders) {

	int count;

	if (set_globals() == GP_ERROR)
		return (GP_ERROR);
	if (!glob_quiet)
		printf("Folders on camera:\n");
	printf("/\n");

	list_folders_rec("/");

	return (GP_OK);
}


OPTION_CALLBACK(use_folder) {

	debug_print("Setting folder to %s", arg);

	strcpy(glob_folder, arg);

	return (GP_OK);
}

OPTION_CALLBACK(num_pictures) {

	int count;

	debug_print("Counting pictures", "");

	if (set_globals() == GP_ERROR)
		return (GP_ERROR);

	count = gp_file_count();
	
	if (count == GP_ERROR) {
		error_print("Could not get number of pictures on camera");
		return (GP_ERROR);
	}

	if (glob_quiet)
		printf("%i\n", count);
	   else
		printf("Number of pictures in folder %s: %i\n", glob_folder, count);

	return (GP_OK);
}

int get_picture_common(int num, int thumbnail) {

	CameraFile *f;
	int count=0;
	char filename[1024], msg[1024];

	if (thumbnail)
		debug_print("Getting thumbnail", "");
	   else
		debug_print("Getting picture", "");

	if (set_globals() == GP_ERROR)
		return (GP_ERROR);

	count = gp_file_count();

	if (count == GP_ERROR) {
		error_print("Could not get number of pictures on camera");
		return (GP_ERROR);
	}

	if (num > count) {
		error_print("Picture number is too large");
		return (GP_ERROR);
	}

	f = gp_file_new();

	if (thumbnail) {
		if (!glob_abilities.file_preview) {
			error_print("Camera can not provide thumbnails");
			gp_file_free(f);
			return (GP_ERROR);
		}
		gp_file_get_preview(num-1, f);
	 } else
		gp_file_get(num-1, f);

	if ((glob_filename_override)&&(strlen(glob_filename)>0))
		sprintf(filename, glob_filename, num);
	   else if (strlen(f->name)>0)
		strcpy(filename, f->name);
	   else {
		sprintf(msg, "Filename not found. Use \"%sfilename\" to specify a filename", LONG_OPTION);
		error_print(msg);
		gp_file_free(f);
		return (GP_ERROR);
	}

	if (!glob_quiet)
		printf("Saving image #%i as %s\n", num, filename);
	if (gp_file_save(f, filename)==GP_ERROR) {
		sprintf(msg, "Can not save image as ", filename);
		error_print(msg);
	}
	gp_file_free(f);

	return (GP_OK);
}

OPTION_CALLBACK(get_picture) {

	return (get_picture_common(atoi(arg), 0));
}

OPTION_CALLBACK(get_thumbnail) {

	if (!glob_abilities.file_preview) {
		error_print("Camera doesn't support picture thumbnails");
		return (GP_ERROR);
	}

	return (get_picture_common(atoi(arg), 1));
}

OPTION_CALLBACK(delete_picture) {

	int num = atoi(arg);
	int count;

	debug_print("Deleting picture", "");

	if (set_globals() == GP_ERROR)
		return (GP_ERROR);

	if (!glob_abilities.file_delete) {
		error_print("Camera can not delete pictures");
		return (GP_ERROR);
	}

	count = gp_file_count();

	if (count == GP_ERROR) {
		error_print("Could not get number of pictures on camera");
		return (GP_ERROR);
	}

	if (num > count) {
		error_print("Picture number is too large.\nRemember that numbering begins at zero (0)");
		return (GP_ERROR);
	}	

	if (gp_file_delete(num-1)==GP_ERROR) {
		error_print("Could not delete the picture");
		return (GP_ERROR);		
	}
	
	return (GP_OK);
}

OPTION_CALLBACK(upload_picture) {

	CameraFile *f;
	char buf[1024];

	debug_print("Uploading picture", "");

	if (set_globals() == GP_ERROR)
		return (GP_ERROR);

	if (!glob_abilities.file_put) {
		error_print("Camera doesn't support uploading pictures");
		return (GP_ERROR);
	}

	f = gp_file_new();
	if (gp_file_open(f, arg)==GP_ERROR) {
		sprintf(buf, "Could not open file %s", arg);
		error_print(buf);
		return (GP_ERROR);		
	}
	
	if (gp_file_put(f)==GP_ERROR) {
		error_print("Could not upload the picture");
		return (GP_ERROR);
	}

	gp_file_free(f);

	return (GP_OK);
}

OPTION_CALLBACK(summary) {

	char buf[1024*32];

	if (set_globals() == GP_ERROR)
		return (GP_ERROR);

	if (gp_summary(buf)==GP_ERROR) {
		error_print("Could not get camera summary");
		return (GP_ERROR);
	}
	printf("Camera Summary:\n%s\n", buf);

	return (GP_OK);
}
OPTION_CALLBACK(manual) {

	char buf[1024*32];

	if (set_globals() == GP_ERROR)
		return (GP_ERROR);

	if (gp_manual(buf)==GP_ERROR) {
		error_print("Could not get camera manual");
		return (GP_ERROR);
	}
	printf("Camera Manual:\n%s\n", buf);

	return (GP_OK);
}

OPTION_CALLBACK(about) {

	char buf[1024*32];

	if (set_globals() == GP_ERROR)
		return (GP_ERROR);

	if (gp_about(buf)==GP_ERROR) {
		error_print("Could not get camera manual");
		return (GP_ERROR);
	}
	printf("About the library:\n%s\n", buf);

	return (GP_OK);
}


int set_globals () {
	/* takes all the settings and sets up the gphoto lib */
	CameraPortSettings s;
	char buf[1024];

	if (strlen(glob_model) == 0) {
		sprintf(buf, "Must specify a camera model using \"%scamera model\"",LONG_OPTION);
		error_print(buf);
		return (GP_ERROR);
	}

	if ((strlen(glob_port) == 0)&&(strcmp(glob_model, "Directory Browse")!=0)) {
		sprintf(buf, "Must specify a camera port using \"%sport path\"",LONG_OPTION);
		error_print(buf);
		return (GP_ERROR);
	}

	strcpy(s.path, glob_port);
	s.speed = glob_speed;

	if (gp_camera_abilities_by_name(glob_model, &glob_abilities)==GP_ERROR) {
		sprintf(buf, "Could not find camera \"%s\".\nUse \"--list-cameras\" to see available camera models", glob_model);
		error_print(buf);
		return (GP_ERROR);
	}

	if (gp_camera_set_by_name(glob_model, &s)==GP_ERROR) {
		sprintf(buf, "Can not initialize camera \"%s\"",glob_model);
		error_print(buf);
		return (GP_ERROR);
	}

	if (gp_folder_set(glob_folder)==GP_ERROR) {
		sprintf(buf, "Can not find folder \"%s\"",glob_folder);
		error_print(buf);
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

	glob_speed = 0;
	glob_debug = 0;
	glob_quiet = 0;
	glob_filename_override = 0;
}

/* Command-line option functions					*/
/* ------------------------------------------------------------------	*/

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
		debug_print("checking \"%s\": \n", argv[x]);
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
			sprintf(msg, "Bad option \"%s\": ", argv[x]);
			error_print(msg);
			if (missing_arg) {
				sprintf(buf, "    Missing argument. You must specify the \"%s\"",
					option[which].argument);
				error_print(msg);
			}   else
				error_print("    unknown option");
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
					// error_print("Option \"%s\" did not execute properly.",op);
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
	"\ngPhoto2 (v%s)- Cross-platform digital camera library.\n"
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

void debug_print(char *message, char *str) {

	if (glob_debug) {
		printf("cli: ");
		printf(message, str);
		printf("\n");
	}
}

void error_print(char *message) {
	
	printf("ERROR: %s\n", message);
}

/* ------------------------------------------------------------------	*/

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
printf("gPhoto2 for OS/2 requires you to set the enviroment value CAMLIBS 
to the location of the camera libraries.
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
//		if (!glob_quiet)
//			usage();
		exit(EXIT_FAILURE);
	}

	/* Exit gPhoto core */
	gp_exit();

	return (EXIT_SUCCESS);
}
