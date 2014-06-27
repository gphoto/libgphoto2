/* test-camera-list.c
 *
 * Copyright 2001 Lutz Mueller <lutz@users.sf.net>
 * Copyright 2005 Hans Ulrich Niedermann <gp@n-dimensional.de>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#include <gphoto2/gphoto2-port-log.h>
#include <gphoto2/gphoto2-camera.h>
#include <gphoto2/gphoto2-port-portability.h>

#define CHECK(f) \
	do { \
		int res = f; \
		if (res < 0) { \
			printf ("ERROR: %s\n", gp_result_as_string (res)); \
			return (1); \
		} \
	} while (0)


#ifdef __GNUC__
#define __unused__ __attribute__((unused))
#else
#define __unused__
#endif


/** boolean value */
static int do_debug = 0;


/** time zero for debug log time stamps */
struct timeval glob_tv_zero = { 0, 0 };


static void
debug_func (GPLogLevel level, const char *domain, const char *str,
	    void __unused__ *data)
{
	struct timeval tv;
	long sec, usec;

	gettimeofday (&tv, NULL);
	sec = tv.tv_sec  - glob_tv_zero.tv_sec;
	usec = tv.tv_usec - glob_tv_zero.tv_usec;
	if (usec < 0) {sec--; usec += 1000000L;}
	fprintf (stderr, "%li.%06li %s(%i): %s\n", sec, usec, domain, level, str);
}


static void
print_headline (void)
{
	printf("No.|%-20s|%-20s|%s\n",
	       "camlib",
	       "driver name",
	       "camera model");
}


static void
print_hline (void)
{
	printf("---+%-20s+%-20s+%s\n",
	       "--------------------",
	       "--------------------",
	       "-------------------------------------------");
}


/** C equivalent of basename(1) */
static const char *
path_basename (const char *pathname)
{
	char *result, *tmp;
	/* remove path part from camlib name */
	for (result=tmp=(char *)pathname; (*tmp!='\0'); tmp++) {
		if ((*tmp == gp_system_dir_delim) 
		    && (*(tmp+1) != '\0')) {
			result = tmp+1;
		}
	}
	return (const char *)result;
}


/** Define the different output formats.
 * These may be used for consistency checking and other stuff.
 */
typedef enum {
	/* Text table with headers. */
	FMT_HEADED_TEXT = 0,
	/* Text table without headers. */
	FMT_FLAT_TEXT,
	/* Text table just of camlibs without headers,
	 * which will contain duplicate lines! 
	 * Treat the output with | sort | uniq to get meaningful results.
	 */
	FMT_FLAT_CAMLIBS,
	/* Comma separated values without headers */
	FMT_CSV,
	/* Demo XML, don't use this for anything (yet) */
	FMT_XML,
	/* Just print the number of supported cameras */
	FMT_COUNT
} OutputFormat;

static OutputFormat format = FMT_HEADED_TEXT;


/* #define DEBUG_OUTPUT */


/** Parse command line and set global variables. */
static void
parse_command_line (const int argc, char *argv[])
{
	int i;
#ifdef DEBUG_OUTPUT
	fprintf(stderr, "parsing cmdline (%d, %p)\n", argc, argv);
	fprintf(stderr, "argv[0]=\"%s\"\n", argv[0]);
#endif
	for (i=1; i<argc; i++) {
#ifdef DEBUG_OUTPUT
		fprintf(stderr, "argv[%d]=\"%s\"\n", i, argv[i]);
#endif
		if (strcmp(argv[i], "--debug") == 0) {
			do_debug = 1;
		} else if (strcmp(argv[i], "--format=text") == 0) {
			format = FMT_HEADED_TEXT;
		} else if (strcmp(argv[i], "--format=flattext") == 0) {
			format = FMT_FLAT_TEXT;
		} else if (strcmp(argv[i], "--format=csv") == 0) {
			format = FMT_CSV;
		} else if (strcmp(argv[i], "--format=xml") == 0) {
			format = FMT_XML;
		} else if (strcmp(argv[i], "--format=count") == 0) {
			format = FMT_COUNT;
		} else if (strcmp(argv[i], "--format=camlibs") == 0) {
			format = FMT_FLAT_CAMLIBS;
		} else {
			const char * const bn = path_basename(argv[0]);
			printf("Unknown command line parameter %d: \"%s\"\n",
			       i, argv[i]);
			printf("Sorry, no more help to give but the "
			       "source code.\n");
			printf("%s: Aborting.\n", bn);
			exit(1);
		}
	}
}


/** 
 * Get list of supported cameras, walk through it and create some output.
 */
int
main (int argc, char *argv[])
{
	CameraAbilitiesList *al;
	int i;
	int count;
	const char *fmt_str = NULL;
	char lastmodel[200];

	parse_command_line (argc, argv);

	if (do_debug) {
		gettimeofday (&glob_tv_zero, NULL);
		CHECK (gp_log_add_func (GP_LOG_ALL, debug_func, NULL));
		
		gp_log (GP_LOG_DEBUG, "main", "test-camera-list start");
	}


	CHECK (gp_abilities_list_new (&al));
	CHECK (gp_abilities_list_load (al, NULL));

	count = gp_abilities_list_count (al);
	if (count < 0) {
		printf("gp_abilities_list_count error: %d\n", count);
		return(1);
	} else if (count == 0) {
		/* Copied from gphoto2-abilities-list.c gp_abilities_list_load() */
		const char *camlib_env = getenv(CAMLIBDIR_ENV);
		const char *camlibs = (camlib_env != NULL)?camlib_env:CAMLIBS;

		printf("No camera drivers (camlibs) found in camlib dir:\n"
		       "    CAMLIBS='%s', default='%s', used=%s\n",
		       camlib_env?camlib_env:"(null)", CAMLIBS,
		       (camlib_env!=NULL)?"CAMLIBS":"default");
		return(1);
	}


	/* Set output format for file body, 
	 * and print opening part of output file. */
	switch (format) {
	case FMT_CSV:
		fmt_str = "%d,%s,%s,%s\n";
		break;
	case FMT_FLAT_TEXT:
		fmt_str = "%3d|%-20s|%-20s|%s\n";
		break;
	case FMT_HEADED_TEXT:
		fmt_str = "%3d|%-20s|%-20s|%s\n";
		break;
	case FMT_XML:
		fmt_str = "  <camera name=\"%4$s\" entry_number=\"%1$d\">\n"
			"    <camlib-name value=\"%2$s\"/>\n"
			"    <driver-name value=\"%3$s\"/>\n"
			"  </camera>\n";
		printf("<?xml version=\"%s\" encoding=\"%s\"?>\n"
		       "<camera-list camera-count=\"%d\">\n", 
		       "1.0", "us-ascii", count);
		break;
	case FMT_FLAT_CAMLIBS:
		fmt_str = "%2$-28s  %3$s\n";
		break;
	case FMT_COUNT:
		printf("%d\n", count);
		return(0);
		break;
	}

	strcpy(lastmodel,"xxx");
	/* For each camera in the list, add a text snippet to the 
	 * output file. */
	for (i = 0; i < count; i++) {
		CameraAbilities abilities;
		const char *camlib_basename;
		CHECK (gp_abilities_list_get_abilities (al, i, &abilities));
		camlib_basename = path_basename(abilities.library);

		if (!strcmp(lastmodel, abilities.model)) {
			fprintf(stderr,"Duplicated model name in camera list: %s\n", lastmodel);
			exit(1);
		}
		strcpy(lastmodel,abilities.model);

		switch (format) {
		case FMT_HEADED_TEXT:
			if ( ((i%25)== 0) && 
			     ( (i==0) || ((count-i) > 5) ) ) {
				print_hline();
				print_headline();
				print_hline();
			}
			break;
		case FMT_FLAT_CAMLIBS:
			break;
		case FMT_XML:
			break;
		case FMT_CSV:
			break;
		case FMT_FLAT_TEXT:
			break;
		case FMT_COUNT:
			break;
		}

		printf(fmt_str,
		       i+1, 
		       camlib_basename,
		       abilities.id,
		       abilities.model);
	}

	/* Print closing part of output file. */
	switch (format) {
	case FMT_HEADED_TEXT:
		print_hline();
		print_headline();
		print_hline();
		break;
	case FMT_FLAT_CAMLIBS:
		break;
	case FMT_XML:
		printf("</camera-list>\n");
		break;
	case FMT_CSV:
		break;
	case FMT_FLAT_TEXT:
		break;
	case FMT_COUNT:
		break;
	}
	
	CHECK (gp_abilities_list_free (al));
	return (0);
}

/*
 * Local variables:
 * mode:c
 * c-basic-offset:8
 * End:
 */
