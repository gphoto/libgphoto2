/* print-camera-list - print libgphoto2 camera list in different formats
 *
 * Copyright © 2002,2005 Hans Ulrich Niedermann <hun@users.sourceforge.net>
 * Portions Copyright © 2002 Lutz Müller <lutz@users.sourceforge.net>
 * Portions Copyright © 2005 Julien BLACHE <jblache@debian.org>
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

#define GP_USB_HOTPLUG_SCRIPT "usbcam"

/* Not sure whether this is the best possible name */
#define ARGV0 "print-camera-list"

#define HELP_TEXT \
"* WARNING * WARNING * WARNING * WARNING * WARNING * WARNING * WARNING *\n" \
"  " ARGV0 " is not implemented completely yet\n" \
"* WARNING * WARNING * WARNING * WARNING * WARNING * WARNING * WARNING *\n" \
"\n" \
ARGV0 " - print libgphoto2 camera list in different formats" \
"\n" \
"Syntax:\n" \
"    " ARGV0 " [<options>] <FORMAT> [<format specific arguments>]\n" \
"\n" \
"Options:\n" \
" --debug          print all debug output\n" \
" --help           print this help message\n" \
" --verbose        also print comments with camera model names\n" \
"\n" \
ARGV0 " prints the camera list in the specified format FORMAT on stdout.\n" \
"\n" \
"All other messages are printed on stderr. In case of any error, the \n" \
"program aborts regardless of data printed on stdout and returns a non-zero\n" \
"status code.\n"


#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>

#include <gphoto2/gphoto2-camera.h>
#include <gphoto2/gphoto2-port-log.h>

#ifndef TRUE
#define TRUE  (0==0)
#endif
#ifndef FALSE
#define FALSE (0!=0)
#endif


typedef struct {
	int number_of_cameras;
	int add_comments;
} func_params_t;


typedef int (* begin_func_t)  (const func_params_t *params);
typedef int (* camera_func_t) (const func_params_t *params, const CameraAbilities *ca);
typedef int (* end_func_t)    (const func_params_t *params);


#define GP_USB_HOTPLUG_MATCH_VENDOR_ID          0x0001
#define GP_USB_HOTPLUG_MATCH_PRODUCT_ID         0x0002

#define GP_USB_HOTPLUG_MATCH_DEV_CLASS          0x0080
#define GP_USB_HOTPLUG_MATCH_DEV_SUBCLASS       0x0100
#define GP_USB_HOTPLUG_MATCH_DEV_PROTOCOL       0x0200


#ifdef __GNUC__
#define CR(result) \
	do { \
		int r = (result); \
		if (r < 0) { \
			fprintf(stderr, ARGV0 ": " \
				"Fatal error running `%s'.\n" \
				"Aborting.\n", #result ); \
			return (r); \
		} \
	} while (0)
#else /* !__GNUC__ */
#define CR(result) \
	do { \
		int r = (result); \
		if (r < 0) { \
			fprintf(stderr, ARGV0 ": " \
				"Fatal error detected, aborting.\n"); \
			return (r); \
		} \
	} while (0)
#endif /* __GNUC__ */


/* print_usb_usermap
 *
 * Print out lines that can be included into usb.usermap 
 * - for all cams supported by our instance of libgphoto2.
 *
 * usb.usermap is a file used by
 * Linux Hotplug http://linux-hotplug.sourceforge.net/
 */

static int
hotplug_camera_func (const func_params_t *params, const CameraAbilities *a)
{
	int flags = 0;
	int class = 0, subclass = 0, proto = 0;
	int usb_vendor = 0, usb_product = 0;
	const char *usermap_script = "OERKSFIXME";

	if (a->usb_vendor) { /* usb product id may be 0! */
		class = 0;
		subclass = 0;
		proto = 0;
		flags = GP_USB_HOTPLUG_MATCH_VENDOR_ID | GP_USB_HOTPLUG_MATCH_PRODUCT_ID;
		usb_vendor = a->usb_vendor;
		usb_product = a->usb_product;
	} else if (a->usb_class) {
		class = a->usb_class;
		subclass = a->usb_subclass;
		proto = a->usb_protocol;
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
		if (params->add_comments) {
			printf ("# %s\n", 
				a->model);
		}
		/* The first 3 lone bytes are the device class.
		 * the second 3 lone bytes are the interface class.
		 * for PTP we want the interface class.
		 */
		printf ("%-20s "
			"0x%04x      0x%04x   0x%04x    0x0000       "
			"0x0000      0x00         0x00            "
			"0x00            0x%02x            0x%02x               "
			"0x%02x               0x00000000\n",
			usermap_script, flags, 
			a->usb_vendor, a->usb_product,
			class, subclass, proto);
	} else {
		fputs ("Error: Neither vendor/product nor class/subclass matched\n", stderr);
		return 2;
	}
	return 0;
}


static int
udev_begin_func (const func_params_t *params)
{
	printf ("# udev rules file for libgphoto2\n#\n");
	printf ("BUS!=\"usb\", ACTION!=\"add\", GOTO=\"libgphoto2_rules_end\"\n\n");	
	return 0;
}

static int
udev_end_func (const func_params_t *params)
{
	printf ("\nLABEL=\"libgphoto2_rules_end\"\n");
	return 0;
}




static int
udev_camera_func (const func_params_t *params, const CameraAbilities *a)
{
	int flags = 0;
	int class = 0, subclass = 0, proto = 0;
	int usb_vendor = 0, usb_product = 0;
	const char *hotplug_script = "OERKSFIXME";

	if (a->usb_vendor) { /* usb product id may be 0! */
		class = 0;
		subclass = 0;
		proto = 0;
		flags = GP_USB_HOTPLUG_MATCH_VENDOR_ID | GP_USB_HOTPLUG_MATCH_PRODUCT_ID;
		usb_vendor = a->usb_vendor;
		usb_product = a->usb_product;
	} else if (a->usb_class) {
		class = a->usb_class;
		subclass = a->usb_subclass;
		proto = a->usb_protocol;
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
		if (params->add_comments) {
			printf ("# %s\n", a->model);
		}

		if (flags & GP_USB_HOTPLUG_MATCH_DEV_CLASS) {
			printf("SYSFS{bDeviceClass}==\"%02x\", ", class);
			if (flags & GP_USB_HOTPLUG_MATCH_DEV_SUBCLASS) {
				printf("SYSFS{bDeviceSubClass}==\"%02x\", ", subclass);
			}
			if (flags & GP_USB_HOTPLUG_MATCH_DEV_PROTOCOL) {
				printf("SYSFS{bDeviceProtocol}==\"%02x\", ", proto);
			}
		} else {
			printf ("SYSFS{idVendor}==\"%04x\", SYSFS{idProduct}==\"%04x\", ",
				a->usb_vendor, a->usb_product);
		}
		printf("RUN+=\"%s\"\n", hotplug_script);
	} else {
		fputs ("Error: Neither vendor/product nor class/subclass matched\n", stderr);
		return 2;
	}
	return 0;
}


static int
empty_begin_func (const func_params_t *params)
{
	return 0;
}

static int
empty_end_func (const func_params_t *params)
{
	return 0;
}


/* print_fdi_map
 *
 * Print FDI Device Information file for HAL with information on 
 * all cams supported by our instance of libgphoto2.
 *
 * For specs, look around on http://freedesktop.org/ and search
 * for FDI on the HAL pages.
 */

static int
fdi_begin_func (const func_params_t *params)
{
	printf("<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?> <!-- -*- SGML -*- -->\n");
	printf("<!-- This file was generated by %s - - fdi -->\n",
	       "libgphoto2 " ARGV0);
	printf("<deviceinfo version=\"0.2\">\n");
	printf(" <device>\n");
	printf("  <match key=\"info.bus\" string=\"usb\">\n");
	return 0;
}


static int
fdi_camera_func (const func_params_t *params, const CameraAbilities *a)
{
	char	*s, *d, model[256];

	if (!(a->port & GP_PORT_USB))
		return 0;

	s = (char *) a->model;
	d = model;
	while (*s) {
		if (*s == '&') {
			strcpy(d,"&amp;");
			d += strlen(d);
		} else {
		    	*d++ = *s;
		}
		s++;
	}
	*d = '\0';
	if (a->usb_vendor) { /* usb product id might be 0! */
		printf("   <match key=\"usb.vendor_id\" int=\"%d\">\n", a->usb_vendor);
		printf("    <match key=\"usb.product_id\" int=\"%d\">\n", a->usb_product);
		printf("     <merge key=\"info.category\" type=\"string\">camera</merge>\n");
		printf("     <append key=\"info.capabilities\" type=\"strlist\">camera</append>\n");

		/* HACK alert ... but the HAL / gnome-volume-manager guys want that */
		if (NULL!=strstr(a->library,"ptp"))
			printf("     <merge key=\"camera.access_method\" type=\"string\">ptp</merge>\n");
		else
			printf("     <merge key=\"camera.access_method\" type=\"string\">proprietary</merge>\n");
		printf("     <merge key=\"camera.libgphoto2.name\" type=\"string\">%s</merge>\n", model);
		printf("     <merge key=\"camera.libgphoto2.support\" type=\"bool\">true</merge>\n");
		printf("    </match>\n");
		printf("   </match>\n");

	} else if (a->usb_class) {
		printf("   <match key=\"usb.interface.class\" int=\"%d\">\n", a->usb_class);
		printf("    <match key=\"usb.interface.subclass\" int=\"%d\">\n", a->usb_subclass);
		printf("     <match key=\"usb.interface.protocol\" int=\"%d\">\n", a->usb_protocol);
		printf("      <merge key=\"info.category\" type=\"string\">camera</merge>\n");
		printf("      <append key=\"info.capabilities\" type=\"strlist\">camera</append>\n");
		if (a->usb_class == 6) {
			printf("      <merge key=\"camera.access_method\" type=\"string\">ptp</merge>\n");
		} else {
			if (a->usb_class == 8) {
				printf("      <merge key=\"camera.access_method\" type=\"string\">storage</merge>\n");
			} else {
				printf("      <merge key=\"camera.access_method\" type=\"string\">proprietary</merge>\n");
			}
		}
		printf("      <merge key=\"camera.libgphoto2.name\" type=\"string\">%s</merge>\n", model);
		printf("      <merge key=\"camera.libgphoto2.support\" type=\"bool\">true</merge>\n");
		printf("     </match>\n");
		printf("    </match>\n");
		printf("   </match>\n");
	}
	return 0;
}

static int
fdi_end_func (const func_params_t *params)
{
	printf("  </match>\n");
	printf(" </device>\n");
	printf("</deviceinfo>\n");
	return 0;
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


typedef struct {
	char *name;
	char *descr;
	char *help;
	char *paramdescr;
	begin_func_t begin_func;
	camera_func_t camera_func;
	end_func_t end_func;
} output_format_t;


static int iterate_camera_list(const int add_comments, const output_format_t *format, char *argv[])
{
	int number_of_cameras;
	CameraAbilitiesList *al;
	CameraAbilities a;
	func_params_t params;

	CR (gp_abilities_list_new (&al));
	CR (gp_abilities_list_load (al, NULL)); /* use NULL context */
	CR (number_of_cameras = gp_abilities_list_count (al));
	
	params.add_comments = add_comments;
	params. number_of_cameras = number_of_cameras;
	
	if (format->begin_func != NULL) {
		format->begin_func(&params);
	}

	if (format->camera_func != NULL) {
		int i;
		for (i = 0; i < number_of_cameras; i++) {
			CR (gp_abilities_list_get_abilities (al, i, &a));
			format->camera_func(&params, &a);
		}
	}

	if (format->end_func != NULL) {
		format->end_func(&params);
	}
	CR (gp_abilities_list_free (al));
	return 0;
}


/* FIXME: Add hyperlink to format documentation. */

static const output_format_t formats[] = {
	{name: "human-readable",
	 descr: "human readable list of cameras",
	 help: NULL,
	 paramdescr: NULL,
	 begin_func: NULL,
	 camera_func: NULL,
	 end_func: NULL
	},
	{name: "usb-usermap",
	 descr: "usb.usermap include file for linux-hotplug",
	 help: "If no <scriptname> is given, uses the script name "
	 "\"" GP_USB_HOTPLUG_SCRIPT "\".\n        Put this into /etc/hotplug/usb/<scriptname>.usermap",
	 paramdescr: "<NAME_OF_HOTPLUG_SCRIPT>",
	 begin_func: empty_begin_func,
	 camera_func: hotplug_camera_func,
	 end_func: empty_end_func
	},
	{name: "hal-fdi",
	 descr: "fdi file for HAL",
	 help: "Put it into /usr/share/hal/fdi/information/10freedesktop/10-camera-libgphoto2.fdi",
	 paramdescr: NULL,
	 begin_func: fdi_begin_func,
	 camera_func: fdi_camera_func,
	 end_func: fdi_end_func
	},
	{name: "udev-rules",
	 descr: "udev rules file",
	 help: "Put it into /etc/udev/libgphoto2.rules",
	 paramdescr: "<PATH_TO_HOTPLUG_SCRIPT>",
	 begin_func: udev_begin_func, 
	 camera_func: udev_camera_func,
	 end_func: udev_end_func
	},
	{NULL, NULL, NULL, NULL, 
	 NULL, NULL, NULL}
};


static int print_format_list(const output_format_t *formats)
{
	int i;
	printf("List of output formats:\n");
	for (i=0; formats[i].name != NULL; ++i) {
		if (formats[i].paramdescr != NULL) {
			printf("    %s %s\n        %s\n", 
			       formats[i].name,
			       formats[i].paramdescr,
			       formats[i].descr);
		} else {
			printf("    %s\n        %s\n", 
			       formats[i].name, formats[i].descr);
		}
		if (formats[i].help != NULL) {
			printf("        %s\n", formats[i].help);
		}
	}
	return 0;
}


static int print_help()
{
	puts(HELP_TEXT);
	return print_format_list(formats);
}


int main(int argc, char *argv[])
{
	int add_comments = FALSE;    /* whether to add cam model as a comment */
	int debug_mode = FALSE;      /* whether we should output debug messages */
	char *format_name = NULL;
	int format_index;

	int i;

	/* check command line arguments */
	for (i=1; (i<argc) && (format_name == NULL); i++) {
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
			return print_help();
		} else {
			format_name = argv[i];
		}
	}

	if (format_name == NULL) {
		return print_help();
	}
	format_index=0;
	while (formats[format_index].name != NULL) {
		if (strcmp(formats[format_index].name, format_name) == 0) {
			break;
		}
		format_index++;
	}
	if ((formats[format_index].name == NULL) || (strcmp(formats[format_index].name, format_name) != 0)) {
		return print_help();
	}
	return iterate_camera_list(add_comments, &formats[format_index], (i<argc)?(&argv[i]):NULL);
}

/*
 * Local Variables:
 * c-file-style:"linux"
 * indent-tabs-mode:t
 * End:
 */
