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

int
delete_all_action (ActionParams *p)
{
	return (gp_camera_folder_delete_all (p->camera, p->folder, p->context));
}

int
list_folders_action (ActionParams *p)
{
	CameraList list;
	int count;
	const char *name;
	unsigned int i;

	CR (gp_camera_folder_list_folders (p->camera, p->folder, &list,
					   p->context));
	CR (count = gp_list_count (&list));
	switch (count) {
        case 0:
                printf (_("There are no folders in folder '%s'."), p->folder);
                printf ("\n");
                break;
        case 1:
                printf (_("There is one folder in folder '%s':"), p->folder);
                printf ("\n");
                break;
        default:
                printf (_("There are %i folders in folder '%s':"),
			 count, p->folder);
                printf ("\n");
                break;
	}
	for (i = 0; i < count; i++) {
		CR (gp_list_get_name (&list, i, &name));
		printf (" - %s\n", name);
	}

	return (GP_OK);
}

int
list_files_action (ActionParams *p)
{
	CameraList list;
	int count;
	const char *name;
	unsigned int i;

	CR (gp_camera_folder_list_files (p->camera, p->folder, &list,
					 p->context));
	CR (count = gp_list_count (&list));
	switch (count) {
	case 0:
		printf (_("There are no files in folder '%s'."), p->folder);
		printf ("\n");
		break;
	case 1:
		printf (_("There is one file in folder '%s':"), p->folder);
		printf ("\n");
		break;
	default:
		printf (_("There are %i files in folder '%s':"), count,
			p->folder);
		printf ("\n");
		break;
	}
	for (i = 0; i < count; i++) {
		CR (gp_list_get_name (&list, i, &name));
		CR (print_file_action (p, name));
	}

	return (GP_OK);
}

int
print_info_action (ActionParams *p, const char *filename)
{
	CameraFileInfo info;

	CR (gp_camera_file_get_info (p->camera, p->folder, filename, &info,
				     p->context));

	printf (_("Information on file '%s' (folder '%s'):\n"),
		filename, p->folder);
	printf (_("File:\n"));
	if (info.file.fields == GP_FILE_INFO_NONE)
		printf (_("  None available.\n"));
	else {
		if (info.file.fields & GP_FILE_INFO_NAME)
			printf (_("  Name:        '%s'\n"), info.file.name);
		if (info.file.fields & GP_FILE_INFO_TYPE)
			printf (_("  Mime type:   '%s'\n"), info.file.type);
		if (info.file.fields & GP_FILE_INFO_SIZE)
			printf (_("  Size:        %li byte(s)\n"), info.file.size);
		if (info.file.fields & GP_FILE_INFO_WIDTH)
			printf (_("  Width:       %i pixel(s)\n"), info.file.width);
		if (info.file.fields & GP_FILE_INFO_HEIGHT)
			printf (_("  Height:      %i pixel(s)\n"), info.file.height);
		if (info.file.fields & GP_FILE_INFO_STATUS)
			printf (_("  Downloaded:  %s\n"),
				(info.file.status == GP_FILE_STATUS_DOWNLOADED) ? _("yes") : _("no"));
		if (info.file.fields & GP_FILE_INFO_PERMISSIONS) {
			printf (_("  Permissions: "));
			if ((info.file.permissions & GP_FILE_PERM_READ) &&
			    (info.file.permissions & GP_FILE_PERM_DELETE))
				printf (_("read/delete"));
			else if (info.file.permissions & GP_FILE_PERM_READ)
				printf (_("read"));
			else if (info.file.permissions & GP_FILE_PERM_DELETE)
				printf (_("delete"));
			else
				printf (_("none"));
			printf ("\n");
		}
		if (info.file.fields & GP_FILE_INFO_MTIME)
			printf (_("  Time:        %s"),
				asctime (localtime (&info.file.mtime)));
	}
	printf (_("Thumbnail:\n"));
	if (info.preview.fields == GP_FILE_INFO_NONE)
		printf (_("  None available.\n"));
	else {
		if (info.preview.fields & GP_FILE_INFO_TYPE)
			printf (_("  Mime type:   '%s'\n"), info.preview.type);
		if (info.preview.fields & GP_FILE_INFO_SIZE)
			printf (_("  Size:        %li byte(s)\n"), info.preview.size);
		if (info.preview.fields & GP_FILE_INFO_WIDTH)
			printf (_("  Width:       %i pixel(s)\n"), info.preview.width);
		if (info.preview.fields & GP_FILE_INFO_HEIGHT)
			printf (_("  Height:      %i pixel(s)\n"), info.preview.height);
		if (info.preview.fields & GP_FILE_INFO_STATUS)
			printf (_("  Downloaded:  %s\n"),
				(info.preview.status == GP_FILE_STATUS_DOWNLOADED) ? _("yes") : _("no"));
	}
	printf (_("Audio data:\n"));
	if (info.audio.fields == GP_FILE_INFO_NONE)
		printf (_("  None available.\n"));
	else {
		if (info.audio.fields & GP_FILE_INFO_TYPE)
			printf (_("  Mime type:  '%s'\n"), info.audio.type);
		if (info.audio.fields & GP_FILE_INFO_SIZE)
			printf (_("  Size:       %li byte(s)\n"), info.audio.size);
		if (info.audio.fields & GP_FILE_INFO_STATUS)
			printf (_("  Downloaded: %s\n"),
				(info.audio.status == GP_FILE_STATUS_DOWNLOADED) ? _("yes") : _("no"));
	}

	return (GP_OK);
}

int
print_file_action (ActionParams *p, const char *filename)
{
	static int x=0;

	if (glob_quiet)
		printf ("\"%s\"\n", filename);
	else {
		CameraFileInfo info;
		if (gp_camera_file_get_info (p->camera, p->folder, filename,
					     &info, NULL) == GP_OK) {
		    printf("#%-5i %-27s", x+1, filename);
		    if (info.file.fields & GP_FILE_INFO_PERMISSIONS) {
			printf("%s%s",
				(info.file.permissions & GP_FILE_PERM_READ) ? "r" : "-",
				(info.file.permissions & GP_FILE_PERM_DELETE) ? "d" : "-");
		    }
		    if (info.file.fields & GP_FILE_INFO_SIZE)
			printf(" %5ld KB", (info.file.size+1023) / 1024);
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
save_file_action (ActionParams *p, const char *filename)
{
	return (save_file_to_file (p->camera, p->context, p->folder, filename,
				   GP_FILE_TYPE_NORMAL));
}

int
save_exif_action (ActionParams *p, const char *filename)
{
	return (save_file_to_file (p->camera, p->context, p->folder, filename,
				   GP_FILE_TYPE_EXIF));
}

int
save_thumbnail_action (ActionParams *p, const char *filename)
{
	return (save_file_to_file (p->camera, p->context, p->folder, filename,
				   GP_FILE_TYPE_PREVIEW));
}

int
save_raw_action (ActionParams *p, const char *filename)
{
	return (save_file_to_file (p->camera, p->context, p->folder, filename,
				   GP_FILE_TYPE_RAW));
}

int
save_audio_action (ActionParams *p, const char *filename)
{
	return (save_file_to_file (p->camera, p->context, p->folder, filename,
				   GP_FILE_TYPE_AUDIO));
}

int
delete_file_action (ActionParams *p, const char *filename)
{
	return (gp_camera_file_delete (p->camera, p->folder, filename,
				       p->context));
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
print_exif_action (ActionParams *p, const char *filename)
{
#ifdef HAVE_EXIF
        CameraFile *file;
        const char *data;
        unsigned long size;
        ExifData *ed;
#ifdef HAVE_EXIF_0_5_4
	unsigned int i;
#endif

        CR (gp_file_new (&file));
        CRU (gp_camera_file_get (p->camera, p->folder, filename,
				 GP_FILE_TYPE_EXIF, file, p->context), file);
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
#ifdef HAVE_EXIF_0_5_4
	for (i = 0; i < EXIF_IFD_COUNT; i++)
		if (ed->ifd[i])
			show_ifd (ed->ifd[i]);
#else
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
#endif
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
