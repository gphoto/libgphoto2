#include <config.h>
#include <stdio.h>
#include <string.h>

#include "main.h"
#include "globals.h"
#include "options.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

int option_is_present (char *op, int argc, char **argv) {
        /* checks to see if op is in the command-line. it will */
        /* check for both short and long option-formats for op */

        int x, found=0;
        char s[5], l[20];

        /* look for short/long options and fill them in */
        for (x=0; x<glob_option_count; x++) {
                if ((strcmp(op, option[x].short_id)==0)||
                    (strcmp(op, option[x].long_id)==0)) {
                        sprintf(s, "%s%s", SHORT_OPTION, option[x].short_id);
                        sprintf(l, "%s%s", LONG_OPTION, option[x].long_id);
                        found=1;
                }
        }

        /* Strictly require an option in the option table */
        if (!found)
                return (GP_ERROR_BAD_PARAMETERS);

        /* look through argv, if a match is found, return */
        for (x=1; x<argc; x++)
                if ((strcmp(s, argv[x])==0)||(strcmp(l, argv[x])==0))
                        return (GP_OK);

        return (GP_ERROR_BAD_PARAMETERS);
}

int verify_options (int argc, char **argv) {
        /* This function makes sure that all command-line options are
           valid and have the correct number of arguments */

        int x, y, match, missing_arg, which;
        char s[5], l[24];

        which = 0;

	for (x=1; x<argc; x++) {
		cli_debug_print("checking \"%s\": \n", argv[x]);
		match = 0;
		missing_arg = 0;
		for (y=0; y<glob_option_count; y++) {
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
			return (GP_ERROR_BAD_PARAMETERS);
		}
	}

	/* Make sure required options are present */
	for (x=0; x<glob_option_count; x++) {
	   if (option[x].required) {
		if (option_is_present(option[x].short_id, argc, argv) != GP_OK) {
			printf("Option %s%s is required.\n",
			 strlen(option[x].short_id)>0? SHORT_OPTION:LONG_OPTION,
			 strlen(option[x].short_id)>0? option[x].short_id:option[x].long_id);
			return (GP_ERROR_BAD_PARAMETERS);
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
        for (x=0; x<glob_option_count; x++) {
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
                                   if (ret != GP_OK) {
                                        // cli_error_print("Option \"%s\" did not execute properly.",op);
                                        return (ret);
                                   }
                                }
                        }
                }
        }

        return (GP_OK);
}

void usage () {

        int x=0;
        char buf[128], s[5], l[24], a[16];

	/* Standard licensing stuff */
	printf(
	"\ngPhoto (v%s) - Cross-platform digital camera library.\n"
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
	while (x < glob_option_count) {
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
		sprintf(buf, " %-4s %s %s", s, l, a);
		printf("%-38s %s\n", buf, option[x].description);
		x++;
	}
	printf( "------------------------------------------------------------------------\n"
	        "[Use double-quotes around arguments]     [Picture numbers begin one (1)]\n");
}
