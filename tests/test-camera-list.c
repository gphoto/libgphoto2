/* test-gphoto2.c
 *
 * Copyright © 2001 Lutz Müller <lutz@users.sf.net>
 * Copyright © 2005 Hans Ulrich Niedermann <gp@n-dimensional.de>
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
#include "config.h"

#include <stdio.h>

#include <time.h>
#include <sys/time.h>

#include "gphoto2-port-log.h"
#include "gphoto2-camera.h"
#include "gphoto2-port-portability.h"

#define CHECK(f) {int res = f; if (res < 0) {printf ("ERROR: %s\n", gp_result_as_string (res)); return (1);}}


/* #define TEST_DEBUG */
#undef TEST_DEBUG

#ifdef TEST_DEBUG

/* time zero for debug log time stamps */
struct timeval glob_tv_zero = { 0, 0 };

static void
#ifdef __GNUC__
		__attribute__((__format__(printf,3,0)))
#endif
debug_func (GPLogLevel level, const char *domain, const char *format,
	    va_list args, void *data)
{
	struct timeval tv;
	long sec, usec;

	gettimeofday (&tv, NULL);
	sec = tv.tv_sec  - glob_tv_zero.tv_sec;
	usec = tv.tv_usec - glob_tv_zero.tv_usec;
	if (usec < 0) {sec--; usec += 1000000L;}
	fprintf (stderr, "%li.%06li %s(%i): ", sec, usec, domain, level);
	vfprintf (stderr, format, args);
	fputc ('\n', stderr);
}

#endif /* TEST_DEBUG */

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

static const char *
basename (const char *pathname)
{
	char *result, *tmp;
	/* remove path part from camlib name */
	for (result=tmp=pathname; *tmp != '\0'; tmp++) {
		if ((*tmp == gp_system_dir_delim) 
		    && (*(tmp+1) != '\0')) {
			result = tmp+1;
		}
	}
	return (const char *)result;
}

int
main (int argc, char *argv [])
{
	CameraAbilitiesList *al;
	int i;
	int count;

#ifdef TEST_DEBUG
	gettimeofday (&glob_tv_zero, NULL);
	CHECK (gp_log_add_func (GP_LOG_ALL, debug_func, NULL));

	gp_log (GP_LOG_DEBUG, "main", "test-camera-list start");
#endif /* TEST_DEBUG */

	CHECK (gp_abilities_list_new (&al));
	CHECK (gp_abilities_list_load (al, NULL));

	count = gp_abilities_list_count (al);
	if (count < 0) {
		printf("gp_abilities_list_count error: %d\n", count);
		return(1);
	} else if (count == 0) {
		printf("no camera drivers (camlibs) found in camlib dir\n");
		return(1);
	}

	print_hline();
	print_headline();
	print_hline();
	for (i = 0; i < count; i++) {
		CameraAbilities abilities;
		const char *camlib_basename;
		CHECK (gp_abilities_list_get_abilities (al, i, &abilities));
		camlib_basename = basename(abilities.library);
		if (((i%25)== 0) && ((count-i) > 5)) {
			print_hline();
			print_headline();
			print_hline();
		}
		printf("%3d|%-20s|%-20s|%s\n", i+1, 
		       camlib_basename,
		       abilities.id,
		       abilities.model);
	}
	print_hline();
	print_headline();
	print_hline();
	
	CHECK (gp_abilities_list_free (al));
	return (0);
}

/*
 * Local variables:
 * mode:c
 * c-basic-offset:8
 * End:
 */
