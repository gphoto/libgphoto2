/* actions.c
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
#include "actions.h"

#include <string.h>
#include <stdio.h>

#include "main.h"
#include "globals.h"

#ifdef HAVE_EXIF
#include <libexif/exif-data.h>
#endif

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

/* Folder actions 						*/
/* ------------------------------------------------------------ */

int
print_folder (const char *subfolder, image_action action, int reverse)
{
	char *c;
	
	/* remove the basename for clarity purposes */
	c = strrchr(subfolder, '/');
	if (*c == '/')
		c++;
	printf("\"%s\"\n", c);
		
	return (GP_OK);
}

int
delete_folder_files (const char *subfolder, image_action action, int reverse)
{
	cli_debug_print("Deleting all files in '%s'", subfolder);
	
	return gp_camera_folder_delete_all (glob_camera, subfolder, glob_context);
}

/* File actions 						*/
/* ------------------------------------------------------------ */

int
print_picture_action (const char *folder, const char *filename)
{
	static int x=0;

	if (glob_quiet)
		printf ("\"%s\"\n", filename);
	else {
		CameraFileInfo info;
		if (gp_camera_file_get_info(glob_camera, folder, filename, &info, NULL) == GP_OK) {
		    printf("#%-5i %-27s", x+1, filename);
		    if (info.file.fields & GP_FILE_INFO_PERMISSIONS) {
			printf("%s%s",
				(info.file.permissions & GP_FILE_PERM_READ) ? "r" : "-",
				(info.file.permissions & GP_FILE_PERM_DELETE) ? "d" : "-");
		    }
		    if (info.file.fields & GP_FILE_INFO_SIZE)
			printf(" %5d KB", (info.file.size+1023) / 1024);
		    if ((info.file.fields & GP_FILE_INFO_WIDTH) && +
			    (info.file.fields & GP_FILE_INFO_HEIGHT))
			printf(" %4dx%-4d", info.file.width, info.file.height);
		    if (info.file.fields & GP_FILE_INFO_TYPE)
			printf(" %s", info.file.type);
			printf("\n");
		} else {
		    printf("#%-5i %s\n", x+1, filename);
		}
	}
	x++;
	return (GP_OK);
}

int
save_picture_action (const char *folder, const char *filename)
{
	return (save_picture_to_file(folder, filename, GP_FILE_TYPE_NORMAL));
}

int
save_exif_action (const char *folder, const char *filename)
{
	return (save_picture_to_file (folder, filename, GP_FILE_TYPE_EXIF));
}

int
save_thumbnail_action (const char *folder, const char *filename)
{
	return (save_picture_to_file (folder, filename, GP_FILE_TYPE_PREVIEW));
}

int
save_raw_action (const char *folder, const char *filename)
{
	return (save_picture_to_file (folder, filename, GP_FILE_TYPE_RAW));
}

int
save_audio_action (const char *folder, const char *filename)
{
	return (save_picture_to_file (folder, filename, GP_FILE_TYPE_AUDIO));
}

int delete_picture_action (const char *folder, const char *filename)
{
	return (gp_camera_file_delete(glob_camera, folder, filename, glob_context));
}

#ifdef HAVE_EXIF
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
#endif

int
print_exif_action (const char *folder, const char *filename)
{
#ifdef HAVE_EXIF
        CameraFile *file;
        const char *data;
        unsigned long size;
        ExifData *ed;

        CR (gp_file_new (&file));
        CRU (gp_camera_file_get (glob_camera, folder, filename,
				GP_FILE_TYPE_EXIF, file, glob_context), file);
        CRU (gp_file_get_data_and_size (file, &data, &size), file);
        ed = exif_data_new_from_data (data, size);
        gp_file_unref (file);
        if (!ed) {
                gp_context_error (glob_context,
				  _("Could not parse EXIF data."));
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
#else
	gp_context_error (glob_context, _("gphoto2 has been compiled without "
		"EXIF support."));
	return (GP_ERROR_NOT_SUPPORTED);
#endif
}
