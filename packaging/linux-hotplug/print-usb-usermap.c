/* $Id$
 *
 * Copyright (C) 2002 Hans Ulrich Niedermann <hun@users.sourceforge.net>
 * Portions Copyright (C) 2002 Lutz Müller <lutz@users.sourceforge.net>
 * Portions Copyright (C) 2002 FIXME
 *
 * This program is free software; you can redistribute it and/or
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

#define HELP_TEXT \
"print-usb-usermap - print usb.usermap file for linux-hotplug\n" \
"\n" \
"Syntax:\n" \
"    print-usb-usermap [ --verbose ] [ --debug ] [ <scriptname> ]\n" \
"\n" \
" --verbose   also print comments with camera model names\n" \
" --debug     print all debug output\n" \
"\n" \
"print-usb-usermap prints the lines for the usb.usermap file on stdout.\n" \
"All other messages are printed on stderr. In case of any error, the \n" \
"program aborts regardless of data printed on stdout and returns a non-zero\n" \
"status code.\n" \
"If no <scriptname> is given, print-usb-usermap uses the script name\n" \
"\"usbcam\"."

#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>

#include <gphoto2-camera.h>
#include <gphoto2-port-log.h>

#ifndef TRUE
#define TRUE  (0==0)
#endif
#ifndef FALSE
#define FALSE (0!=0)
#endif

#define GP_USB_HOTPLUG_SCRIPT "usbcam"

#define GP_USB_HOTPLUG_MATCH_VENDOR_ID          0x0001
#define GP_USB_HOTPLUG_MATCH_PRODUCT_ID         0x0002

#define GP_USB_HOTPLUG_MATCH_DEV_CLASS          0x0080
#define GP_USB_HOTPLUG_MATCH_DEV_SUBCLASS       0x0100
#define GP_USB_HOTPLUG_MATCH_DEV_PROTOCOL       0x0200

#define CR(result) {int r = (result); if (r < 0) return (r);}

/* print-usb-usermap
 *
 * Print out lines that can be included into usb.usermap 
 * - for all cams supported by our instance of libgphoto2.
 *
 * written after list_cameras 
 */

static int print_usb_usermap(const char *usermap_script, const int add_comments)
{
	int x, n;
	CameraAbilitiesList *al;
        CameraAbilities a;

	CR (gp_abilities_list_new (&al));
	CR (gp_abilities_list_load (al, NULL)); /* use NULL context */
	CR (n = gp_abilities_list_count (al));

        for (x = 0; x < n; x++) {
		int flags = 0;
		int class = 0, subclass = 0, proto = 0;
		int usb_vendor = 0, usb_product = 0;

		CR (gp_abilities_list_get_abilities (al, x, &a));

		if (!(a.port & GP_PORT_USB))
		    continue;

		if (a.usb_vendor) { /* usb product id may be 0! */
			class = 0;
			subclass = 0;
			proto = 0;
			flags = GP_USB_HOTPLUG_MATCH_VENDOR_ID | GP_USB_HOTPLUG_MATCH_PRODUCT_ID;
			usb_vendor = a.usb_vendor;
			usb_product = a.usb_product;
		} else if (a.usb_class) {
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
			usb_vendor = 0;
			usb_product = 0;
		} else {
			flags = 0;
		}

		if (flags != 0) {
			printf ("# %s\n", 
				a.model);
			printf ("%-20s "
				"0x%04x      0x%04x   0x%04x    0x0000       "
				"0x0000       0x%02x         0x%02x            "
				"0x%02x            0x00            0x00               "
				"0x00               0x00000000\n",
				usermap_script, flags, 
				a.usb_vendor, a.usb_product,
				class, subclass, proto);
		} else {
			fputs ("Error: Neither vendor/product nor class/subclass matched\n", stderr);
			return 2;
		}
        }
	CR (gp_abilities_list_free (al));

        return (0);
}

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

int main(int argc, char *argv[])
{
	char *usermap_script = NULL; /* script name to use */
	int add_comments = FALSE;    /* whether to add cam model as a comment */
	int debug_mode = FALSE;      /* whether we should output debug messages */

	int i;

	/* check command line arguments */
	for (i=1; i<argc; i++) {
		if (0 == strcmp(argv[i], "--verbose")) {
			if (add_comments) {
				fprintf(stderr, "Error: duplicate parameter: option \"%s\"\n", argv[i]);
				return 1;
			}
			add_comments = TRUE;
		} else if (0 == strcmp(argv[i], "--debug")) {
			if (debug_mode) {
				fprintf(stderr, "Error: duplicate parameter: option \"%s\"\n", argv[i]);
				return 1;
			}
			debug_mode = TRUE;
			/* now is time zero for debug log time stamps */
			gettimeofday (&glob_tv_zero, NULL);
			gp_log_add_func (GP_LOG_ALL, debug_func, NULL);
		} else if (0 == strcmp(argv[i], "--help")) {
			puts(HELP_TEXT);
			return 0;
		} else {
			if (usermap_script != NULL) {
				fprintf(stderr, "Error: duplicate script parameter: \"%s\"\n", argv[i]);
				return 1;
			}
			/* assume script name */
			usermap_script = argv[i];
		}
	}

	if (NULL == usermap_script) {
		usermap_script = GP_USB_HOTPLUG_SCRIPT;
	}

	return print_usb_usermap(usermap_script, add_comments);
}

/*
 * Local Variables:
 * c-file-style:"linux"
 * indent-tabs-mode:t
 * End:
 */
