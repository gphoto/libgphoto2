#include <stdlib.h>
#include <stdio.h>
#include <gphoto2.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "main.h"
#include "test.h"
#include "interface.h"

/* Command-line options
   -----------------------------------------------------------------------
   (this is funky, but sounded cool to do. it makes sense since this is 
   just a wrapper for a functional/flow-based library)

   - Add command-line options in the following table. 
   - Don't forget to update options_count!
   - Order is important! Options are parsed in the order in the table.

   When main() starts up, it starts parsing the command-line arguments.
   For each argument, it will look up the entry in the following table. if it
   finds it, it will call the callback function.
   Then, it will remove that argument from the command-line and continue.

   How to add an option:

/* 1) Add a forward-declaration here 					*/
/*    ----------------------------------------------------------------- */

OPTION_CALLBACK(filename);
OPTION_CALLBACK(get_picture);
OPTION_CALLBACK(get_thumbnail);

/* 2) Add an entry in the options table 				*/
/*    ----------------------------------------------------------------- */
/*    Format for options is:						*/
/*     {'short', "long", "argument", "description", callback_function}, */
/*    if it is just a flag, set the argument to ""			*/

Option options[] = {
{'f', "filename",	"<filename>",	"Specify a filename",		filename},
{'p', "get-picture",	"#", 		"Get picture # from camera", 	get_picture},
{'t', "get-thumbnail",	"#", 		"Get thumbnail # from camera",	get_thumbnail},
};

/* 3) Increase options_count by the number of entries you added.	*/
/*    ----------------------------------------------------------------- */
/*    (Value should be the total number of entries in the options table.*/

int options_count = 3;

/* 4) Add any necessary global variables				*/
/*    ----------------------------------------------------------------- */

char glob_filename[128];


/* 4) Finally, add your callback function.				*/
/*    ----------------------------------------------------------------- */
/*    The callback function is passed "char *arg" to the argument of    */
/*    command-line option.						*/

OPTION_CALLBACK(filename) {

	printf ("filename callback\n");

	strcpy(glob_filename, arg);
}


OPTION_CALLBACK(get_picture) {

	printf ("get_picture callback\n");
}

OPTION_CALLBACK(get_thumbnail) {

	printf ("get_thumbnail callback\n");
}

void usage () {

	int x=0;
	char buf[128];

	/* Standard licensing stuff */
	printf(
	"gPhoto2 (v%s)- Cross-platform digital camera library.\n"
	"Copyright (C) 2000 Scott Fritzinger\n"
	"gPhoto is licensed under the Lesser GNU Public License (LGPL).\n"
	"Usage:\n", VERSION
	);

	/* Run through options and print them out */
	while (x < options_count) {
		sprintf(buf, " -%c, --%s %s", 
			options[x].short_id, options[x].long_id, options[x].argument);
		printf("%-38s %s\n", buf, options[x].description);
		x++;
	}

}

int main (int argc, char **argv) {

	usage();
}
