/* gphoto2-cmd-exif.c
 *
 * Copyright (C) 2002 Lutz Müller <lutz@users.sourceforge.net>
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
#include <config.h>
#include "gphoto2-cmd-exif.h"

#include <stdio.h>

#include <libexif/exif-data.h>

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define _(String) (String)
#  define N_(String) (String)
#endif

#define CR(result)       {int r=(result); if (r<0) return r;}
#define CRU(result,file) {int r=(result); if (r<0) {gp_file_unref(file);return r;}}

static void
show_ifd (ExifContent *content)
{
	ExifEntry *e;
	unsigned int i;

	for (i = 0; i < content->count; i++) {
		e = content->entries[i];
		printf ("%-20.20s", exif_tag_get_name (e->tag));
		printf ("|");
		printf ("%-59.59s", exif_entry_get_value (e));
		printf ("\n");
	}
}

static void
print_hline (void)
{
	int i;

	for (i = 0; i < 20; i++)
		printf ("-");
	printf ("+");
	for (i = 0; i < 59; i++)
		printf ("-");
	printf ("\n"); 
}
							
int
gp_cmd_exif (Camera *camera, const char *folder, const char *filename,
	     GPContext *context)
{
	CameraFile *file;
	const char *data;
	unsigned long size;
	ExifData *ed;

	/* Did the user specify a number? */
	if (!strchr (filename, '.')) {
		gp_context_error (context, _("Please specify a filename."));
		return (GP_ERROR);
	}

	CR (gp_file_new (&file));
	CRU (gp_camera_file_get (camera, folder, filename, GP_FILE_TYPE_EXIF,
				 file, context), file);
	CRU (gp_file_get_data_and_size (file, &data, &size), file);
	ed = exif_data_new_from_data (data, size);
	gp_file_unref (file);
	if (!ed) {
		gp_context_error (context, _("Could not parse EXIF data."));
		return (GP_ERROR);
	}

	printf (_("EXIF tags:"));
	printf ("\n");
	print_hline ();
	printf ("%-20.20s", _("Tag"));
	printf ("|");
	printf ("%-59.59s", _("Value"));
	printf ("\n");
	print_hline ();
	if (ed->ifd0)
		show_ifd (ed->ifd0);
	if (ed->ifd1)
		show_ifd (ed->ifd1);
	if (ed->ifd_exif)
		show_ifd (ed->ifd_exif);
	if (ed->ifd_gps)
		show_ifd (ed->ifd_gps);
	if (ed->ifd_interoperability)
		show_ifd (ed->ifd_interoperability);
	print_hline ();
	if (ed->size) {
		printf (_("EXIF data contains a thumbnail (%i bytes)."),
			ed->size);
		printf ("\n");
	}

	exif_data_unref (ed);

	return (GP_OK);
}
