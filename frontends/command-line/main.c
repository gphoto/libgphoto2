#include <stdlib.h>
#include <stdio.h>
#include <gphoto2.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "main.h"
#include "test.h"
#include "interface.h"

void usage();

/* Command-line option
   -----------------------------------------------------------------------
   (this is funky, but sounded cool to do. it makes sense since this is 
   just a wrapper for a functional/flow-based library)

   - Add command-line option in the following table. 
   - Don't forget to update option_count!
   - Order is important! option are parsed in the order in the table.

   When main() starts up, it starts parsing the command-line arguments.
   For each argument, it will look up the entry in the following table. if it
   finds it, it will call the callback function.
   Then, it will remove that argument from the command-line and continue.

   How to add an option:

/* 1) Add a forward-declaration here 					*/
/*    ----------------------------------------------------------------- */

OPTION_CALLBACK(help);
OPTION_CALLBACK(test);
OPTION_CALLBACK(filename);
OPTION_CALLBACK(get_picture);
OPTION_CALLBACK(get_thumbnail);

/* 2) Add an entry in the option table 				*/
/*    ----------------------------------------------------------------- */
/*    Format for option is:						*/
/*     {'short', "long", "argument", "description", callback_function}, */
/*    if it is just a flag, set the argument to ""			*/

Option option[] = {
{"h", "help",		"",		"Displays this help screen",	help},
{"",  "test",		"",		"Verifies gPhoto installation",	test},
{"f", "filename",	"<filename>",	"Specify a filename",		filename},
{"p", "get-picture",	"#", 		"Get picture # from camera", 	get_picture},
{"t", "get-thumbnail",	"#", 		"Get thumbnail # from camera",	get_thumbnail},
};

/* 3) Increase option_count by the number of entries you added.	*/
/*    ----------------------------------------------------------------- */
/*    (Value should be the total number of entries in the option table.*/

int option_count = 5;

/* 4) Add any necessary global variables				*/
/*    ----------------------------------------------------------------- */

int  glob_filename_override=0;
char glob_filename[128];


/* 4) Finally, add your callback function.				*/
/*    ----------------------------------------------------------------- */
/*    The callback function is passed "char *arg" to the argument of    */
/*    command-line option.						*/

OPTION_CALLBACK(help) {

	printf("help callback\n");

	usage();
}

OPTION_CALLBACK(test) {
	printf("test callback\n");

	test_gphoto();

	exit(0);
}

OPTION_CALLBACK(filename) {

	printf ("filename: %s\n", arg);

	strcpy(glob_filename, arg);
}


OPTION_CALLBACK(get_picture) {

	printf ("get_picture: %s\n", arg);
}

OPTION_CALLBACK(get_thumbnail) {

	printf ("get_thumbnail: %s\n", arg);
}

int verify_options (int argc, char **argv) {
	/* This function makes sure that all command-line options are
	   valid and have the correct number of arguments */

	int x, y, match, missing_arg, which;
	char s[5], l[24];

	which = 0;

	for (x=1; x<argc; x++) {
// printf("checking \"%s\": \n", argv[x]);
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
			printf("\n** Bad option \"%s\": ", argv[x]);
			if (missing_arg)
				printf("\n\tMissing argument. You must specify \"%s\"",
					option[which].argument);
			    else
				printf("unknown option");
			printf(". **\n\n");
			return (GP_ERROR);
		}
	}
	return (GP_OK);
}

void usage () {

	int x=0;
	char buf[128], s[5], l[24], a[16];

	/* Standard licensing stuff */
	printf(
	"gPhoto2 (v%s)- Cross-platform digital camera library.\n"
	"Copyright (C) 2000 Scott Fritzinger\n"
	"gPhoto is licensed under the Lesser GNU Public License (LGPL).\n"
	"Usage:\n", VERSION
	);

	/* Run through option and print them out */
	while (x < option_count) {
		if (strlen(option[x].short_id) > 0)
			sprintf(s, "%s%s,", SHORT_OPTION, option[x].short_id);
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

	exit(0);
}

int main (int argc, char **argv) {

	int x, y;
	char s[5], l[24];

	if ((argc == 1)||(verify_options(argc, argv)==GP_ERROR))
		usage();

	for (x=0; x<option_count; x++) {
		sprintf(s, "%s%s", SHORT_OPTION, option[x].short_id);
		sprintf(l, "%s%s", LONG_OPTION, option[x].long_id);
		for (y=1; y<argc; y++) {
			if ((strcmp(argv[y],s)==0)||(strcmp(argv[y],l)==0)) {
				if (strlen(option[x].argument) > 0) {
					(*option[x].execute)(argv[++y]);
				}  else
					(*option[x].execute)(NULL);
			}
		}
	}
}
