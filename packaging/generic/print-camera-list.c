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
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>

#include <gphoto2/gphoto2-camera.h>
#include <gphoto2/gphoto2-port-log.h>

/* for detailed version message */
#include <gphoto2/gphoto2-version.h>

#include "config.h"

#ifndef TRUE
#define TRUE  (0==0)
#endif
#ifndef FALSE
#define FALSE (0!=0)
#endif

typedef struct {
	char *name;
	GPVersionFunc version_func;
} module_version;

const module_version module_versions[] = {
	{ "libgphoto2", gp_library_version },
	{ "libgphoto2_port", gp_port_library_version },
	{ NULL, NULL }
};

typedef char *string_array_t[];

typedef string_array_t *string_array_p;

typedef struct {
	int number_of_cameras;
	int add_comments;
	int argc;
	string_array_p argv;
} func_params_t;


typedef int (* begin_func_t)  (const func_params_t *params);
typedef int (* camera_func_t) (const func_params_t *params, 
			       const int i,
			       const int total,
			       const CameraAbilities *ca);
typedef int (* end_func_t)    (const func_params_t *params);


#define GP_USB_HOTPLUG_MATCH_VENDOR_ID          0x0001
#define GP_USB_HOTPLUG_MATCH_PRODUCT_ID         0x0002

#define GP_USB_HOTPLUG_MATCH_INT_CLASS          0x0080
#define GP_USB_HOTPLUG_MATCH_INT_SUBCLASS       0x0100
#define GP_USB_HOTPLUG_MATCH_INT_PROTOCOL       0x0200


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

#define FATAL(msg...) \
	do { \
		fprintf(stderr, ARGV0 ": Fatal: " msg); \
		fprintf(stderr, "\n"); \
		exit(13); \
	} while (0)


/* print_version_comment
 * Print comment to output containing information on library versions
 *
 * out        the file to write the comment to
 * startline  printed at the start of each line, e.g. "# " or "    | "
 * endline    printed as the end of each line,   e.g. "\n" or "\n"
 * firstline  printed before first line,         e.g. NULL or "<!--+\n"
 * lastline   printed after last line,           e.g. "\n" or "    +-->\n"
 */

static void
print_version_comment(FILE *out,
		      const char *startline, const char *endline,
		      const char *firstline, const char *lastline)
{
	unsigned int n;
	if (out == NULL) { FATAL("Internal error: NULL out in print_version_comment()"); }
	if (firstline != NULL) { fputs(firstline, out); }
	fputs(startline, out);
	fputs("Created from this library:", out);
	fputs(endline, out);
	for (n=0; (module_versions[n].name != NULL) && (module_versions[n].version_func != NULL); n++) {
		const char *name = module_versions[n].name;
		GPVersionFunc func = module_versions[n].version_func;
		const char **v = func(GP_VERSION_SHORT);
		unsigned int i;
		if (!v) { continue; }
		if (!v[0]) { continue; }
		if (startline != NULL) { fputs(startline, out); }
		fputs("  ", out);
		fprintf(out,"%-15s %-14s ", name, v[0]);
		for (i=1; v[i] != NULL; i++) {
			fputs(v[i], out);
			if (v[i+1] != NULL) {
				fputs(", ", out);
			}
		}
		if (endline != NULL) { fputs(endline, out); }
	}
	if (lastline != NULL) { fputs(lastline, out); }
}


static int
hotplug_begin_func (const func_params_t *params)
{
	if (params->add_comments) {
		printf("# linux-hotplug configuration file "
		       "for libgphoto2 supported devices\n");
		print_version_comment(stdout, "# ", "\n", NULL, "#\n");
	}
	return 0;
}


/* print_usb_usermap
 *
 * Print out lines that can be included into usb.usermap 
 * - for all cams supported by our instance of libgphoto2.
 *
 * usb.usermap is a file used by
 * Linux Hotplug http://linux-hotplug.sourceforge.net/
 */

static int
hotplug_camera_func (const func_params_t *params, 
		     const int i,
		     const int total,
		     const CameraAbilities *a)
{
	int flags = 0;
	int class = 0, subclass = 0, proto = 0;
	int usb_vendor = 0, usb_product = 0;
	const char *usermap_script = 
		((*params->argv)[0] != NULL)
		?((*params->argv)[0])
		:(GP_USB_HOTPLUG_SCRIPT);

	if (a->port & GP_PORT_USB) {
		if (a->usb_vendor) { /* usb product id may be zero! */
			class = 0;
			subclass = 0;
			proto = 0;
			flags = (GP_USB_HOTPLUG_MATCH_VENDOR_ID 
				 | GP_USB_HOTPLUG_MATCH_PRODUCT_ID);
			usb_vendor = a->usb_vendor;
			usb_product = a->usb_product;
		} else if ((a->usb_class) && (a->usb_class != 666)) {
			class = a->usb_class;
			subclass = a->usb_subclass;
			proto = a->usb_protocol;
			flags = GP_USB_HOTPLUG_MATCH_INT_CLASS;
			if (subclass != -1)
				flags |= GP_USB_HOTPLUG_MATCH_INT_SUBCLASS;
			else
				subclass = 0;
			if (proto != -1)
				flags |= GP_USB_HOTPLUG_MATCH_INT_PROTOCOL;
			else
				proto = 0;
			usb_vendor = 0;
			usb_product = 0;
		}
	} else {
		/* not a USB camera */
		return 0;
	}

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
	return 0;
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


static int
human_begin_func (const func_params_t *params)
{
	print_hline();
	print_headline();
	print_hline();
	return 0;
}


static int
human_end_func (const func_params_t *params)
{
	print_hline();
	print_headline();
	return 0;
}


/** C equivalent of basename(1) */
static const char *
basename (const char *pathname)
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


static int
human_camera_func (const func_params_t *params, 
		   const int i, 
		   const int total,
		   const CameraAbilities *a)
{
	const char *camlib_basename;
	camlib_basename = basename(a->library);
	printf("%3d|%-20s|%-20s|%s\n",
	       i+1, 
	       camlib_basename,
	       a->id,
	       a->model);
	return 0;
}


static int
idlist_camera_func (const func_params_t *params, 
		   const int i, 
		   const int total,
		   const CameraAbilities *a)
{
	if (a->usb_vendor) { /* usb product id may be zero! */
		printf("%04x:%04x %s\n",
		       a->usb_vendor,
		       a->usb_product,
		       a->model);
	}
	return 0;
}


static int
udev_begin_func (const func_params_t *params)
{
	printf ("# udev rules file for libgphoto2 devices (udev < 0.98)\n");
	print_version_comment(stdout, "# ", "\n", NULL, "#\n");
	/* Note: 2 lines because we need to use || ... having them on the same
	 * line would mean &&.
	 */
	printf ("BUS!=\"usb*\", GOTO=\"libgphoto2_rules_end\"\n");
	printf ("ACTION!=\"add\", GOTO=\"libgphoto2_rules_end\"\n\n");
	return 0;
}

static int
udev_098_begin_func (const func_params_t *params)
{
	printf ("# udev rules file for libgphoto2 devices (udev >= 0.98)\n");
	print_version_comment(stdout, "# ", "\n", NULL, "#\n");
	/* Note: 2 lines because we need to use || ... having them on the same
	 * line would mean &&.
	 */
	printf ("SUBSYSTEM!=\"usb*\", GOTO=\"libgphoto2_rules_end\"\n");
	printf ("ACTION!=\"add\", GOTO=\"libgphoto2_rules_end\"\n\n");
	return 0;
}

static int
udev_end_func (const func_params_t *params)
{
	printf ("\nLABEL=\"libgphoto2_rules_end\"\n");
	return 0;
}




static int
udev_camera_func (const func_params_t *params, 
		  const int i,
		  const int total,
		  const CameraAbilities *a)
{
	int flags = 0;
	int class = 0, subclass = 0, proto = 0;
	int usb_vendor = 0, usb_product = 0;

	if (!(a->port & GP_PORT_USB))
		return 0;

	if (a->usb_vendor) { /* usb product id may be zero! */
		class = 0;
		subclass = 0;
		proto = 0;
		flags = (GP_USB_HOTPLUG_MATCH_VENDOR_ID 
			 | GP_USB_HOTPLUG_MATCH_PRODUCT_ID);
		usb_vendor = a->usb_vendor;
		usb_product = a->usb_product;
	} else {
		if (a->usb_class) {
			class = a->usb_class;
			subclass = a->usb_subclass;
			proto = a->usb_protocol;
			flags = GP_USB_HOTPLUG_MATCH_INT_CLASS;
			if (subclass != -1)
				flags |= GP_USB_HOTPLUG_MATCH_INT_SUBCLASS;
			else
				subclass = 0;
			if (proto != -1)
				flags |= GP_USB_HOTPLUG_MATCH_INT_PROTOCOL;
			else
				proto = 0;
			usb_vendor = 0;
			usb_product = 0;
		}
	}

	if (params->add_comments) {
		printf ("# %s\n", a->model);
	}

	if (flags & GP_USB_HOTPLUG_MATCH_INT_CLASS) {
		if ((flags & (GP_USB_HOTPLUG_MATCH_INT_CLASS|GP_USB_HOTPLUG_MATCH_INT_SUBCLASS|GP_USB_HOTPLUG_MATCH_INT_PROTOCOL)) == (GP_USB_HOTPLUG_MATCH_INT_CLASS|GP_USB_HOTPLUG_MATCH_INT_SUBCLASS|GP_USB_HOTPLUG_MATCH_INT_PROTOCOL)) {
			printf("PROGRAM=\"check_ptp_camera %02d/%02d/%02d\", ", class, subclass, proto);
		} else {
			if (class == 666) {
				printf("PROGRAM=\"check_mtp_device\", ");
			} else {
				fprintf(stderr,"unhandled interface match flags %x\n", flags);
			}
		}
	} else {
		printf ("SYSFS{idVendor}==\"%04x\", SYSFS{idProduct}==\"%04x\", ",
			a->usb_vendor, a->usb_product);
	}
	if ((*params->argv)[1] == NULL) {
		const char *hotplug_script = ((*params->argv)[0] != NULL)
			?((*params->argv)[0])
			:("/etc/hotplug/usb/" GP_USB_HOTPLUG_SCRIPT);
		printf("RUN+=\"%s\"\n", hotplug_script);
	} else { /* more than two parameters in params->argv */
		char *mode = NULL;
		char *owner = NULL;
		char *group = NULL;

		int i;
		char *key = NULL, *val = NULL;
		for (i=0; ((key=(*params->argv)[i])   != NULL) && 
			  ((val=(*params->argv)[i+1]) != NULL); i+=2) {
			if (strcmp("owner", key)==0) {
				owner = val;
			} else if (strcmp("group", key)==0) {
				group = val;
			} else if (strcmp("mode", key)==0) {
				mode = val;
			} else {
				FATAL("Unknown key argument: %s", key);
			}
		}
		if ((key != NULL) && (val == NULL)) {
			FATAL("Single argument remaining; need pairs of key and value");
		}

		if (mode != NULL || owner != NULL || group != NULL) {
			if (mode != NULL) {
				printf("MODE=\"%s\"", mode);
				if (owner != NULL || group != NULL) {
					printf(", ");
				}
			}
			if (owner != NULL) {
				printf("OWNER=\"%s\"", owner);
				if (group != NULL) {
					printf(", ");
				}
			}
			if (group != NULL) {
				printf("GROUP=\"%s\"", group);
			}
			printf("\n");
		} else {
			FATAL("Arguments required");
		}
	}
	/*
	printf("MUH!\n");
	*/
	return 0;
}

static int
udev_098_camera_func (const func_params_t *params, 
		  const int i,
		  const int total,
		  const CameraAbilities *a)
{
	int flags = 0;
	int class = 0, subclass = 0, proto = 0;
	int usb_vendor = 0, usb_product = 0;

	if (!(a->port & GP_PORT_USB))
		return 0;
	if (a->usb_vendor) { /* usb product id may be zero! */
		class = 0;
		subclass = 0;
		proto = 0;
		flags = (GP_USB_HOTPLUG_MATCH_VENDOR_ID 
			 | GP_USB_HOTPLUG_MATCH_PRODUCT_ID);
		usb_vendor = a->usb_vendor;
		usb_product = a->usb_product;
	} else {
		if (a->usb_class) {
			class = a->usb_class;
			subclass = a->usb_subclass;
			proto = a->usb_protocol;
			flags = GP_USB_HOTPLUG_MATCH_INT_CLASS;
			if (subclass != -1)
				flags |= GP_USB_HOTPLUG_MATCH_INT_SUBCLASS;
			else
				subclass = 0;
			if (proto != -1)
				flags |= GP_USB_HOTPLUG_MATCH_INT_PROTOCOL;
			else
				proto = 0;
			usb_vendor = 0;
			usb_product = 0;
		}
	}

	if (params->add_comments)
		printf ("# %s\n", a->model);

	if (flags & GP_USB_HOTPLUG_MATCH_INT_CLASS) {
		if ((flags & (GP_USB_HOTPLUG_MATCH_INT_CLASS|GP_USB_HOTPLUG_MATCH_INT_SUBCLASS|GP_USB_HOTPLUG_MATCH_INT_PROTOCOL)) == (GP_USB_HOTPLUG_MATCH_INT_CLASS|GP_USB_HOTPLUG_MATCH_INT_SUBCLASS|GP_USB_HOTPLUG_MATCH_INT_PROTOCOL)) {
			printf("PROGRAM=\"check_ptp_camera %02d/%02d/%02d\", ", class, subclass, proto);
		} else {
			if (class == 666)
				printf("PROGRAM=\"check_mtp_device\", ");
			else
				fprintf(stderr,"unhandled interface match flags %x\n", flags);
		}
	} else {
		printf ("ATTRS{idVendor}==\"%04x\", ATTRS{idProduct}==\"%04x\", ",
			a->usb_vendor, a->usb_product);
	}
	if ((*params->argv)[1] == NULL) {
		const char *hotplug_script = ((*params->argv)[0] != NULL)
			?((*params->argv)[0])
			:("/etc/hotplug/usb/" GP_USB_HOTPLUG_SCRIPT);
		printf("RUN+=\"%s\"\n", hotplug_script);
	} else { /* more than two parameters in params->argv */
		char *mode = NULL;
		char *owner = NULL;
		char *group = NULL;

		int i;
		char *key = NULL, *val = NULL;
		for (i=0; ((key=(*params->argv)[i])   != NULL) && 
			  ((val=(*params->argv)[i+1]) != NULL); i+=2) {
			if (strcmp("owner", key)==0) {
				owner = val;
			} else if (strcmp("group", key)==0) {
				group = val;
			} else if (strcmp("mode", key)==0) {
				mode = val;
			} else {
				FATAL("Unknown key argument: %s", key);
			}
		}
		if ((key != NULL) && (val == NULL)) {
			FATAL("Single argument remaining; need pairs of key and value");
		}

		if (mode != NULL || owner != NULL || group != NULL) {
			if (mode != NULL) {
				printf("MODE=\"%s\"", mode);
				if (owner != NULL || group != NULL) {
					printf(", ");
				}
			}
			if (owner != NULL) {
				printf("OWNER=\"%s\"", owner);
				if (group != NULL) {
					printf(", ");
				}
			}
			if (group != NULL) {
				printf("GROUP=\"%s\"", group);
			}
			printf("\n");
		} else {
			FATAL("Arguments required");
		}
	}
	/*
	printf("MUH!\n");
	*/
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



#ifdef ENABLED_GP2DDB

static int
ddb_begin_func (const func_params_t *params)
{
	printf("# Beginning of gphoto2 device database (PRE-ALPHA format!)\n\n");
	print_version_comment(stdout, "# ", "\n", NULL, "\n");
	return 0;
}


static int
ddb_end_func (const func_params_t *params)
{
	printf("# End of gphoto2 device database (PRE-ALPHA format!).\n");
	return 0;
}


static void
ddb_list_out_func(const char *str, void *data)
{
	int *first = (int *)data;
	printf("%s %s", (*first)?"":",", str);
	*first = 0;
}


static void
ddb_delayed_head(
	const func_params_t *params, 
	const int i,
	const int total,
	const CameraAbilities *a,
	const char *camlib_basename)
{
	int first = 1;
	printf("# This is a PRE-ALPHA data format. Do not use it.\n\n");

	if (params->add_comments) {
		printf ("# %s\n", a->model);
	}

	printf("# automatically generated camera record %d/%d\n", i+1, total);
	printf("device \"%s\" {\n", a->model);

	printf("    device_type");
	first = 1;
	gpi_enum_to_string(a->device_type, 
			   gpi_gphoto_device_type_map,
			   ddb_list_out_func,
			   (void *) &first);
	printf(";\n");

	printf("    driver \"%s\";\n", camlib_basename);

	printf("    driver_status");
	first = 1;
	gpi_enum_to_string(a->status, 
			   gpi_camera_driver_status_map,
			   ddb_list_out_func,
			   (void *) &first);
	printf(";\n");

	printf("    operations");
	first = 1;
	gpi_flags_to_string_list(a->operations, 
				 gpi_camera_operation_map,
				 ddb_list_out_func,
				 (void *) &first);
	printf(";\n");

	printf("    file_operations");
	first = 1;
	gpi_flags_to_string_list(a->file_operations, 
				 gpi_file_operation_map,
				 ddb_list_out_func,
				 (void *) &first);
	printf(";\n");

	printf("    folder_operations");
	first = 1;
	gpi_flags_to_string_list(a->folder_operations, 
				 gpi_folder_operation_map,
				 ddb_list_out_func,
				 (void *) &first);
	printf(";\n");
}


static int
ddb_camera_func (const func_params_t *params, 
		 const int i,
		 const int total,
		 const CameraAbilities *a)
{
	const char *camlib_basename = basename(a->library);
	int head_printed = 0;
#define DELAYED_HEAD() \
	do { \
		if (!head_printed) {			      \
			ddb_delayed_head(params, i, total,    \
					 a, camlib_basename); \
			head_printed = 1;		      \
		} \
	} while (0)

	if ((a->port & GP_PORT_SERIAL)) {
		int first = 1;
		DELAYED_HEAD();
		printf("    interface serial {\n");
		if (a->speed[0] != 0) {
			unsigned int i;
			printf("        speeds");
			for (i=0; 
			     ((i < (sizeof(a->speed)/sizeof(a->speed[0]))) &&
			      (a->speed[i] != 0)); i++) {
				printf("%s %d", (first)?"":",", a->speed[i]);
				first = 0;
			}
			printf(";\n");
		}
		printf("    };\n");
	}

	if ((a->port & GP_PORT_USB)) {
		if (a->usb_vendor) { 
			/* usb product id may have the legal value zero! */
			DELAYED_HEAD();
			printf("    interface usb {\n");
			printf("        vendor  0x%04x;\n", a->usb_vendor);
			printf("        product 0x%04x;\n", a->usb_product);
			printf("    };\n");
		}
		if ((a->usb_class) && (a->usb_class != 666)) {
			DELAYED_HEAD();
			printf("    interface usb {\n");
			printf("        class 0x%02x;\n", a->usb_class);
			if (a->usb_subclass != -1) {
				printf("        subclass 0x%02x;\n", a->usb_subclass);
			}
			if (a->usb_protocol != -1) {
				printf("        protocol 0x%02x;\n", a->usb_protocol);
			}
			printf("    };\n");
		}
	}
	if ((a->port & GP_PORT_DISK)) {
		DELAYED_HEAD();
		printf("    interface disk;\n");
	}
	if ((a->port & GP_PORT_PTPIP)) {
		DELAYED_HEAD();
		printf("    interface ptpip;\n");
	}
#undef DELAYED_HEAD

	if (head_printed) {
		printf("    # driver_options {\n");
		printf("    #      option \"foo\";\n");
		printf("    #      option \"bar\" \"bla\";\n");
		printf("    # };\n");

		printf("}; # %s\n\n", a->model);
	}
	return 0;
}

#endif /* ENABLED_GP2DDB */


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
	print_version_comment(stdout, "    | ", "\n", "<!--+\n", "    +-->\n");
	printf("<deviceinfo version=\"0.2\">\n");
	printf(" <device>\n");
	printf("  <match key=\"info.bus\" string=\"usb\">\n");
	return 0;
}


static int
fdi_camera_func (const func_params_t *params, 
		 const int i,
		 const int total,
		 const CameraAbilities *a)
{
	char	*s, *d, model[256];

	if (!(a->port & GP_PORT_USB))
		return 0;

	s = (char *) a->model;
	d = model;
	while (*s && (d < &d[sizeof(d)-1])) {
		if (*s == '&') {
			strcpy(d,"&amp;");
			d += strlen(d);
		} else {
		    	*d++ = *s;
		}
		s++;
	}
	*d = '\0';

	if ((a->port & GP_PORT_USB)) {
		if (a->usb_vendor == 0x07b4 && a->usb_product == 0x105) {
			/* Marcus says: The Olympus Sierra/Storage dual mode camera firmware.
			 * Some HAL using software gets deeply confused by this being here
			 * and also detected as mass storage elsewhere, so blacklist
			 * it here.
			 */
			return 0;
		}
		if (a->usb_vendor) { /* usb product id might be 0! */
			printf("   <match key=\"usb.vendor_id\" int=\"%d\">\n", a->usb_vendor);
			printf("    <match key=\"usb.product_id\" int=\"%d\">\n", a->usb_product);
			printf("     <merge key=\"info.category\" type=\"string\">camera</merge>\n");
			if (a->device_type & GP_DEVICE_AUDIO_PLAYER) {
				printf("     <append key=\"info.capabilities\" type=\"strlist\">camera</append>\n");
				printf("     <merge key=\"info.category\" type=\"string\">portable_audio_player</merge>\n");
				printf("     <append key=\"info.capabilities\" type=\"strlist\">portable_audio_player</append>\n");
				printf("     <merge key=\"portable_audio_player.access_method\" type=\"string\">user</merge>\n");
				printf("     <merge key=\"portable_audio_player.type\" type=\"string\">mtp</merge>\n");
				
				/* FIXME: needs true formats ... But all of them can do MP3 */
				printf("     <append key=\"portable_audio_player.output_formats\" type=\"strlist\">audio/mpeg</append>\n");
			} else {
				printf("     <append key=\"info.capabilities\" type=\"strlist\">camera</append>\n");
				
				/* HACK alert ... but the HAL / gnome-volume-manager guys want that */
				if (NULL!=strstr(a->library,"ptp"))
					printf("     <merge key=\"camera.access_method\" type=\"string\">ptp</merge>\n");
				else
					printf("     <merge key=\"camera.access_method\" type=\"string\">proprietary</merge>\n");
			}
			/* leave them here even for audio players */
			printf("     <merge key=\"camera.libgphoto2.name\" type=\"string\">%s</merge>\n", model);
			printf("     <merge key=\"camera.libgphoto2.support\" type=\"bool\">true</merge>\n");
			printf("    </match>\n");
			printf("   </match>\n");
			
		} else if ((a->usb_class) && (a->usb_class != 666)) {
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
	} /* camera has USB connection */
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


/* print_fdi_device_map
 *
 * Print FDI Device Information file for HAL with information on 
 * all cams supported by our instance of libgphoto2.
 *
 * For specs, look around on http://freedesktop.org/ and search
 * for FDI on the HAL pages.
 */

static int
fdi_device_begin_func (const func_params_t *params)
{
	printf("<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?> <!-- -*- SGML -*- -->\n");
	printf("<!-- This file was generated by %s - - fdi-device -->\n",
	       "libgphoto2 " ARGV0);
	print_version_comment(stdout, "    | ", "\n", "<!--+\n", "    +-->\n");
	printf("<deviceinfo version=\"0.2\">\n");
	printf(" <device>\n");
	printf("  <match key=\"info.bus\" string=\"usb\">\n");
	return 0;
}


static int
fdi_device_camera_func (const func_params_t *params, 
		 	const int i,
			const int total,
		 	const CameraAbilities *a)
{
	char	*s, *d, model[256];

	if (!(a->port & GP_PORT_USB))
		return 0;

	s = (char *) a->model;
	d = model;
	while (*s && (d < &d[sizeof(d)-1])) {
		if (*s == '&') {
			strcpy(d,"&amp;");
			d += strlen(d);
		} else {
		    	*d++ = *s;
		}
		s++;
	}
	*d = '\0';

	if ((a->port & GP_PORT_USB)) {

		if (a->usb_vendor == 0x07b4 && a->usb_product == 0x105) {
			/* Marcus says: The Olympus Sierra/Storage dual mode camera firmware.
			 * Some HAL using software gets deeply confused by this being here
			 * and also detected as mass storage elsewhere, so blacklist
			 * it here.
			 */
			return 0;
		}
		if (a->usb_vendor) { /* usb product id might be 0! */
			/* do not set category. We don't really know what this device really is.
			 * But we do now that is capable of being a camera, so add to capabilities
			 */
			printf("   <match key=\"usb_device.vendor_id\" int=\"%d\">\n", a->usb_vendor);
			printf("    <match key=\"usb_device.product_id\" int=\"%d\">\n", a->usb_product);
			if (params->add_comments) {
				printf("     <!-- %s -->\n", a->model);
			}
			if (a->device_type & GP_DEVICE_AUDIO_PLAYER)
				printf("     <append key=\"info.capabilities\" type=\"strlist\">portable_audio_player</append>\n");
			else
				printf("     <append key=\"info.capabilities\" type=\"strlist\">camera</append>\n");
			printf("    </match>\n");
			printf("   </match>\n");
		}
#if 0
		/* would need to be able to merge upwards ... but cannot currently */
		else if ((a->usb_class) && (a->usb_class != 666)) {
			printf("   <match key=\"usb.interface.class\" int=\"%d\">\n", a->usb_class);
			printf("    <match key=\"usb.interface.subclass\" int=\"%d\">\n", a->usb_subclass);
			printf("     <match key=\"usb.interface.protocol\" int=\"%d\">\n", a->usb_protocol);
			printf("      <append key=\"info.capabilities\" type=\"strlist\">camera</append>\n");
			printf("     </match>\n");
			printf("    </match>\n");
			printf("   </match>\n");
		}
#endif
	}
	return 0;
}

static int
fdi_device_end_func (const func_params_t *params)
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


static int
iterate_camera_list (const int add_comments, 
		     const output_format_t *format, 
		     string_array_p argv)
{
	int number_of_cameras;
	CameraAbilitiesList *al;
	CameraAbilities a;
	func_params_t params;

	CR (gp_abilities_list_new (&al));
	CR (gp_abilities_list_load (al, NULL)); /* use NULL context */
	CR (number_of_cameras = gp_abilities_list_count (al));

	params.add_comments = add_comments;
	params.number_of_cameras = number_of_cameras;
	params.argv = argv;

	if (format->begin_func != NULL) {
		format->begin_func(&params);
	}

	if (format->camera_func != NULL) {
		int i;
		for (i = 0; i < number_of_cameras; i++) {
			CR (gp_abilities_list_get_abilities (al, i, &a));
			format->camera_func(&params, i, number_of_cameras, &a);
		}
	}

	if (format->end_func != NULL) {
		format->end_func(&params);
	}
	CR (gp_abilities_list_free (al));
	return 0;
}


/* FIXME: Add hyperlink to format documentation. */

/** list of supported output formats */
static const output_format_t formats[] = {
	{name: "human-readable",
	 descr: "human readable list of cameras",
	 help: NULL,
	 paramdescr: NULL,
	 begin_func: human_begin_func,
	 camera_func: human_camera_func,
	 end_func: human_end_func
	},
	{name: "usb-usermap",
	 descr: "usb.usermap include file for linux-hotplug",
	 help:
	 "If no <scriptname> is given, uses the script name "
	 "\"" GP_USB_HOTPLUG_SCRIPT "\".\n"
	 "        Put this into /etc/hotplug/usb/<scriptname>.usermap",
	 paramdescr: "<NAME_OF_HOTPLUG_SCRIPT>",
	 begin_func: hotplug_begin_func,
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
	{name: "hal-fdi-device",
	 descr: "fdi device file for HAL",
	 help: "Put it into /usr/share/hal/fdi/information/10freedesktop/10-camera-libgphoto2-device.fdi",
	 paramdescr: NULL,
	 begin_func: fdi_device_begin_func,
	 camera_func: fdi_device_camera_func,
	 end_func: fdi_device_end_func
	},
	{name: "udev-rules",
	 descr: "udev < 0.98 rules file",
	 help: "Put it into /etc/udev/rules.d/90-libgphoto2.rules, set file mode, owner, group\n"
	 "        or add script to run. This rule files also uses the\n"
	 "        check_ptp_camera script included in libgphoto2 source. Either put it to\n"
	 "        /lib/udev/check_ptp_camera or adjust the path in the generated rules file.",
	 paramdescr: "( <PATH_TO_SCRIPT> | [mode <mode>|owner <owner>|group <group>]* ) ",
	 begin_func: udev_begin_func, 
	 camera_func: udev_camera_func,
	 end_func: udev_end_func
	},
	{name: "udev-rules-0.98",
	 descr: "udev >= 0.98 rules file",
	 help:
	 "Put it into /etc/udev/rules.d/90-libgphoto2.rules, set file mode, owner, group\n"
	 "        or add script to run, for udev >= 0.98. This rule files also uses the\n"
	 "        check_ptp_camera script included in libgphoto2 source. Either put it to\n"
	 "        /lib/udev/check_ptp_camera or adjust the path in the generated rules file.",
	 paramdescr: "( <PATH_TO_SCRIPT> | [mode <mode>|owner <owner>|group <group>]* ) ",
	 begin_func: udev_098_begin_func, 
	 camera_func: udev_098_camera_func,
	 end_func: udev_end_func
	},
	{name: "idlist",
	 descr: "list of IDs and names",
	 help: "grep for an ID to find the device name",
	 paramdescr: NULL,
	 begin_func: empty_begin_func, 
	 camera_func: idlist_camera_func,
	 end_func: empty_end_func
	},
#ifdef ENABLED_GP2DDB
	{name: "gp2ddb",
	 descr: "gphoto2 device database (PRE-ALPHA)",
	 help: "PRE-ALPHA test stage, do not use for production! Machine parseable.",
	 paramdescr: NULL,
	 begin_func: ddb_begin_func,
	 camera_func: ddb_camera_func,
	 end_func: ddb_end_func
	},
#endif
	{NULL, NULL, NULL, NULL, 
	 NULL, NULL, NULL}
};


/** \brief print list of output formats with descriptions
 */

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


/** \brief print program help
 */

static int print_help()
{
	puts(HELP_TEXT);
	return print_format_list(formats);
}


/** \brief main program: parse and check arguments, then delegate the work
 */

int main(int argc, char *argv[])
{
	int add_comments = FALSE; /* whether to add cam model as a comment */
	int debug_mode = FALSE;   /* whether we should output debug messages */

	char *format_name = NULL; /* name of desired output format */
	int format_index;         /* index number of (desired) output format */
	static char *fmt_argv[16]; /* format specific arguments */

	int i, j;
	unsigned int ui;

	/* initialize parameters to NULL */
	for (ui=0; ui<(sizeof(fmt_argv)/sizeof(fmt_argv[0])); ui++) {
		fmt_argv[ui] = NULL;
	}

	/* walk through command line arguments until format is encountered */
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

	/* walk through output format list, searching for the requested one */
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
	if ((formats[format_index].name == NULL) || 
	    (strcmp(formats[format_index].name, format_name) != 0)) {
		return print_help();
	}

	/* copy remaining arguments */
	for (j=i; 
	     (j<argc) && ((j-i)<(int)((sizeof(fmt_argv)/sizeof(fmt_argv[0]))-1)); 
	     j++) {
		fmt_argv[j-i] = argv[j];
	}

	/* execute the work using the given parameters*/
	return iterate_camera_list(add_comments, &formats[format_index],
				   &fmt_argv
		);
}

/*
 * Local Variables:
 * c-file-style:"linux"
 * indent-tabs-mode:t
 * End:
 */
