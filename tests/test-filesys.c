/* test-filesys.c
 *
 * Copyright (C) 2001 Lutz Müller <urc8@rz.uni-karlsruhe.de>
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

#include <stdio.h>
#include <string.h>

#ifdef HAVE_MCHECK_H
#include <mcheck.h>
#endif

#include <gphoto2-filesys.h>
#include <gphoto2-result.h>
#include <gphoto2-debug.h>

#define CHECK(r) {int ret = r; if (ret < 0) {printf ("Got error: %s\n", gp_result_as_string (ret)); return (1);}}

static int
set_info_func (CameraFilesystem *fs, const char *folder, const char *file,
	       CameraFileInfo info, void *data, GPContext *context)
{
	printf ("  -> The camera will set the file info here.\n");

	return (GP_OK);
}

static int
get_info_func (CameraFilesystem *fs, const char *folder, const char *file,
	       CameraFileInfo *info, void *data, GPContext *context)
{
	printf ("  -> The camera will get the file info here.\n");

	info->preview.fields = GP_FILE_INFO_NONE;
	info->file.fields    = GP_FILE_INFO_NAME;
	strcpy (info->file.name, "Some file on the camera");

	return (GP_OK);
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data, GPContext *context)
{
	printf ("### -> The camera will list the files in '%s' here.\n", folder);

	if (!strcmp (folder, "/whatever")) {
		gp_list_append (list, "file1", NULL);
		gp_list_append (list, "file2", NULL);
	}

	return (GP_OK);
}

static int
folder_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		  void *data, GPContext *context)
{
	printf ("### -> The camera will list the folders in '%s' here.\n", 
		folder);

	if (!strcmp (folder, "/")) {
		gp_list_append (list, "whatever", NULL);
		gp_list_append (list, "another", NULL);
	}

	if (!strcmp (folder, "/whatever")) {
		gp_list_append (list, "directory", NULL);
		gp_list_append (list, "dir", NULL);
	}

	if (!strcmp (folder, "/whatever/directory")) {
		gp_list_append (list, "my_special_folder", NULL);
	}

	return (GP_OK);
}

static int
delete_file_func (CameraFilesystem *fs, const char *folder, const char *file,
		  void *data, GPContext *context)
{
	printf ("Here we should delete %s from folder %s...\n", file, folder);

	return (GP_OK);
}

int
main (int argc, char **argv)
{
	CameraFilesystem *fs;
	CameraFileInfo info;
	CameraList list;
	int x, count;
	const char *name;

#ifdef HAVE_MCHECK_H
	mtrace();
#endif

	printf ("*** Creating file system...\n");
	CHECK (gp_filesystem_new (&fs));

	printf ("*** Setting the callbacks...\n");
	CHECK (gp_filesystem_set_info_funcs (fs, get_info_func, set_info_func,
					     NULL));
	CHECK (gp_filesystem_set_file_funcs (fs, NULL, delete_file_func,
					     NULL));

	printf ("*** Adding a file...\n");
	CHECK (gp_filesystem_append (fs, "/", "my.file", NULL));

	gp_filesystem_dump (fs);

	printf ("*** Removing this file...\n");
	CHECK (gp_filesystem_delete_file (fs, "/", "my.file", NULL));

	gp_filesystem_dump (fs);

	printf ("*** Resetting...\n");
	CHECK (gp_filesystem_reset (fs));

	gp_filesystem_dump (fs);

	printf ("*** Adding /...\n");
	CHECK (gp_filesystem_append (fs, "/", NULL, NULL));

	printf ("*** Adding /whatever ...\n");
	CHECK (gp_filesystem_append (fs, "/whatever", NULL, NULL));

	printf ("*** Adding /whatever/dir...\n");
	CHECK (gp_filesystem_append (fs, "/whatever/dir", NULL, NULL));

	printf ("*** Adding /whatever/dir/file1...\n");
	CHECK (gp_filesystem_append (fs, "/whatever/dir", "file1", NULL));

	gp_filesystem_dump (fs);

	printf ("*** Adding /whatever/dir/file2...\n");
	CHECK (gp_filesystem_append (fs, "/whatever/dir", "file2", NULL));
	CHECK (gp_filesystem_append (fs, "/whatever/dir", "file3", NULL));
	CHECK (gp_filesystem_append (fs, "/whatever/dir", "file4", NULL));
	CHECK (gp_filesystem_append (fs, "/whatever/dir", "file5", NULL));

	gp_filesystem_dump (fs);

	printf ("*** Deleting everything below root...\n");
	CHECK (gp_filesystem_delete_all (fs, "/", NULL));

	gp_filesystem_dump (fs);

	printf ("*** Appending root directory...\n");
	CHECK (gp_filesystem_append (fs, "/", NULL, NULL));

	printf ("*** Appending some directories...\n");
	CHECK (gp_filesystem_append (fs, "/whatever", NULL, NULL));
	CHECK (gp_filesystem_append (fs, "/whatever/directory", NULL, NULL));

	printf ("*** Adding some files...\n");
	CHECK (gp_filesystem_append (fs, "/whatever/directory",
				     "some.file", NULL));
	CHECK (gp_filesystem_append (fs, "/whatever/directory",
				     "some.file2", NULL));
	CHECK (gp_filesystem_append (fs, "/another/directory",
				     "another.file", NULL));

	gp_filesystem_dump (fs);

	printf ("*** Getting info about a file...\n");
	CHECK (gp_filesystem_get_info (fs, "/whatever/directory", "some.file",
				       &info, NULL));

	printf ("*** Getting info again (cache!)...\n");
	CHECK (gp_filesystem_get_info (fs, "/whatever/directory", "some.file",
				       &info, NULL));

	printf ("*** Set info about another file...\n");
	CHECK (gp_filesystem_set_info (fs, "/whatever/directory", "some.file2",
				       info, NULL));

	printf ("*** Getting info about this file (cache!)...\n");
	CHECK (gp_filesystem_get_info (fs, "/whatever/directory", "some.file2",
				       &info, NULL));

	printf ("*** Deleting a file...\n");
	CHECK (gp_filesystem_delete_file (fs, "/whatever/directory",
					  "some.file2", NULL));

	gp_filesystem_dump (fs);

	printf ("*** Resetting the filesystem...\n");
	CHECK (gp_filesystem_reset (fs));

	printf ("*** Setting the listing callbacks...\n");
	CHECK (gp_filesystem_set_list_funcs (fs, file_list_func,
					     folder_list_func, NULL));

	gp_filesystem_dump (fs);

	printf ("*** Getting file list for folder '/whatever/directory'...\n");
	CHECK (gp_filesystem_list_folders (fs, "/whatever/directory",
					   &list, NULL));

	printf ("*** Getting file list for folder '/whatever/directory' "
		"again (cached!)...\n");
	CHECK (gp_filesystem_list_folders (fs, "/whatever/directory",
					   &list, NULL));

	printf ("*** Counting the contents...\n");
	CHECK (count = gp_list_count (&list));

	printf ("*** Listing the contents...\n");
	for (x = 0; x < count; x++) {
		CHECK (gp_list_get_name (&list, x, &name));
		printf (" %i: '%s'\n", x, name);
	}

	printf ("*** Getting folder of 'file1'...\n");
	CHECK (gp_filesystem_get_folder (fs, "file1", &name, NULL));
	printf ("... found in '%s'.\n", name);

	printf ("*** Freeing file system...\n");
	CHECK (gp_filesystem_free (fs));

#ifdef HAVE_MCHECK_H
	muntrace();
#endif

	return (0);
}

/*
 * Local variables:
 *  compile-command: "gcc -pedantic -Wstrict-prototypes -O2 -g test-filesys.c -o test-filesys `gphoto2-config --cflags --libs` && export MALLOC_TRACE=test-filesys.log && ./test-filesys && mtrace ./test-filesys test-filesys.log"
 * End:
 */
