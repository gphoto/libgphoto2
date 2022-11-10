/* print-libgphoto2-version.c - print libgphoto2 library version
 *
 * Copyright 2002,2005 Hans Ulrich Niedermann <hun@users.sourceforge.net>
 * Portions Copyright 2002 Lutz Mueller <lutz@users.sourceforge.net>
 * Portions Copyright 2005 Julien BLACHE <jblache@debian.org>
 * Portions Copyright 2020 Hans Ulrich Niedermann <hun@n-dimensional.de>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 *
 *
 * This has been mostly copied from the file
 *
 *     packaging/generic/print-camera-list.c
 *
 */


#include <stdio.h>
#include <stdlib.h>

#include "gphoto2/gphoto2-version.h"


/* Not sure whether this is the best possible name */
#define ARGV0 "print-libgphoto2-version"


#define FATAL(msg...)				\
	do { \
		fprintf(stderr, ARGV0 ": Fatal: " msg); \
		fprintf(stderr, "\n"); \
		exit(13); \
	} while (0)


typedef struct {
	char *name;
	GPVersionFunc version_func;
} module_version;


const module_version module_versions[] = {
	{ "libgphoto2", gp_library_version },
	{ "libgphoto2_port", gp_port_library_version },
	{ NULL, NULL }
};


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
	if (startline != NULL) { fputs(startline, out); }
	fputs("Short runtime version information on the libgphoto2 build:", out);
	if (endline != NULL) { fputs(endline, out); }
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


static void
print_version_verbose(FILE *out,
		      const char *startline, const char *endline,
		      const char *firstline, const char *lastline)
{
	unsigned int n;
	if (out == NULL) { FATAL("Internal error: NULL out in print_version_verbose()"); }
	if (firstline != NULL) { fputs(firstline, out); }
	if (startline != NULL) { fputs(startline, out); }
	fputs("Verbose runtime version information on the libgphoto2 build:", out);
	if (endline != NULL) { fputs(endline, out); }
	for (n=0; (module_versions[n].name != NULL) && (module_versions[n].version_func != NULL); n++) {
		const char *name = module_versions[n].name;
		GPVersionFunc func = module_versions[n].version_func;
		const char **v = func(GP_VERSION_VERBOSE);
		unsigned int i;
		if (!v) { continue; }
		if (!v[0]) { continue; }
		if (startline != NULL) { fputs(startline, out); }
		fputs("  * ", out);
		fprintf(out,"%s %s", name, v[0]);
		if (endline != NULL) { fputs(endline, out); }
		for (i=1; v[i] != NULL; i++) {
			fprintf(out, "      * %s", v[i]);
			if (endline != NULL) { fputs(endline, out); }
		}
	}
	if (lastline != NULL) { fputs(lastline, out); }
}


int main(void)
{
	print_version_comment(stdout, NULL, "\n", NULL, NULL);
	printf("\n");
	print_version_verbose(stdout, NULL, "\n", NULL, NULL);
	return 0;
}


/*
 * Local Variables:
 * c-file-style:"linux"
 * indent-tabs-mode:t
 * End:
 */
