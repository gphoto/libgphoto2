/* test-filesys.c
 *
 * Copyright 2001 Lutz Mueller <lutz@users.sf.net>
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
#include <string.h>
#include <stdlib.h>

#ifdef HAVE_MCHECK_H
#include <mcheck.h>
#endif

#include <gphoto2/gphoto2-filesys.h>
#include <gphoto2/gphoto2-result.h>
#include <gphoto2/gphoto2-port-log.h>


#ifdef __GNUC__
#define __unused__ __attribute__((unused))
#else
#define __unused__
#endif


#define CHECK(r) {int ret = r; if (ret < 0) {printf ("Got error: %s\n", gp_result_as_string (ret)); return (1);}}

static void
log_func (GPLogLevel __unused__ level, const char __unused__ *domain,
	  const char *str, void __unused__ *data)
{
	printf ("%s\n", str);
}

static void
error_func (GPContext __unused__ *context, const char *str, void __unused__ *data)
{
	printf ("### %s\n", str);
}

static int
set_info_func (CameraFilesystem __unused__ *fs, const char __unused__ *folder, 
	       const char __unused__ *file,
	       CameraFileInfo __unused__ info, void __unused__ *data, 
	       GPContext __unused__ *context)
{
	printf ("  -> The camera will set the file info here.\n");

	return (GP_OK);
}

static int
get_info_func (CameraFilesystem __unused__ *fs, const char __unused__ *folder, 
	       const char *file,
	       CameraFileInfo *info, void __unused__ *data, GPContext __unused__ *context)
{
	printf ("  -> The camera will get the file info here.\n");

	info->preview.fields = GP_FILE_INFO_NONE;
	info->file.fields    = GP_FILE_INFO_NONE;
	return (GP_OK);
}

static int
file_list_func (CameraFilesystem __unused__ *fs, const char *folder, 
		CameraList *list,
		void __unused__ *data, GPContext __unused__ *context)
{
	printf ("### -> The camera will list the files in '%s' here.\n", folder);

	if (!strcmp (folder, "/whatever")) {
		gp_list_append (list, "file1", NULL);
		gp_list_append (list, "file2", NULL);
		gp_list_append (list, "file3", NULL);
		gp_list_append (list, "file4", NULL);
		gp_list_append (list, "file5", NULL);
		gp_list_append (list, "file6", NULL);
	}

	return (GP_OK);
}

static int
folder_list_func (CameraFilesystem __unused__ *fs, const char *folder, 
		  CameraList *list,
		  void __unused__ *data, GPContext __unused__ *context)
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
delete_file_func (CameraFilesystem __unused__ *fs, const char *folder, 
		  const char *file,
		  void __unused__ *data, GPContext __unused__ *context)
{
	printf ("Here we should delete %s from folder %s...\n", file, folder);

	return (GP_OK);
}

static CameraFilesystemFuncs fsfuncs = {
	.get_info_func = get_info_func,
	.set_info_func = set_info_func,
	.del_file_func = delete_file_func,
	.file_list_func = file_list_func,
	.folder_list_func = folder_list_func,
};
int
main ()
{
	CameraFilesystem *fs;
	CameraFileInfo info;
	CameraList *list;
	int x, count;
	const char *name;
	char *foldername;
	GPContext *context;

#ifdef HAVE_MCHECK_H
	mtrace();
#endif

	CHECK (gp_list_new(&list));

	gp_log_add_func (GP_LOG_DEBUG, log_func, NULL);
	context = gp_context_new ();
	gp_context_set_error_func (context, error_func, NULL);

	printf ("*** Creating file system...\n");
	CHECK (gp_filesystem_new (&fs));

	printf ("*** Setting the callbacks...\n");
	CHECK (gp_filesystem_set_funcs (fs, &fsfuncs, NULL));

	printf ("*** Adding a file...\n");
	CHECK (gp_filesystem_append (fs, "/", "my.file", context));

	gp_filesystem_dump (fs);

	printf ("*** Removing this file...\n");
	CHECK (gp_filesystem_delete_file (fs, "/", "my.file", context));

	gp_filesystem_dump (fs);

	printf ("*** Resetting...\n");
	CHECK (gp_filesystem_reset (fs));

	gp_filesystem_dump (fs);

	printf ("*** Adding /...\n");
	CHECK (gp_filesystem_append (fs, "/", NULL, context));

	printf ("*** Adding /whatever ...\n");
	CHECK (gp_filesystem_append (fs, "/whatever", NULL, context));

	printf ("*** Adding /whatever/dir...\n");
	CHECK (gp_filesystem_append (fs, "/whatever/dir", NULL, context));

	printf ("*** Adding /whatever/dir/file1...\n");
	CHECK (gp_filesystem_append (fs, "/whatever/dir", "file1", context));

	gp_filesystem_dump (fs);

	printf ("*** Adding /whatever/dir/file2...\n");
	CHECK (gp_filesystem_append (fs, "/whatever/dir", "file2", context));
	CHECK (gp_filesystem_append (fs, "/whatever/dir", "file3", context));
	CHECK (gp_filesystem_append (fs, "/whatever/dir", "file4", context));
	CHECK (gp_filesystem_append (fs, "/whatever/dir", "file5", context));

	gp_filesystem_dump (fs);

	printf ("*** Deleting everything below root...\n");
	CHECK (gp_filesystem_delete_all (fs, "/", context));

	gp_filesystem_dump (fs);

	printf ("*** Appending root directory...\n");
	CHECK (gp_filesystem_append (fs, "/", NULL, context));

	printf ("*** Appending some directories...\n");
	CHECK (gp_filesystem_append (fs, "/whatever", NULL, context));
	CHECK (gp_filesystem_append (fs, "/whatever/directory", NULL, context));

	printf ("*** Adding some files...\n");
	CHECK (gp_filesystem_append (fs, "/whatever/directory",
				     "some.file", context));
	CHECK (gp_filesystem_append (fs, "/whatever/directory",
				     "some.file2", context));
	CHECK (gp_filesystem_append (fs, "/another/directory",
				     "another.file", context));

	gp_filesystem_dump (fs);

	printf ("*** Getting info about a file...\n");
	CHECK (gp_filesystem_get_info (fs, "/whatever/directory", "some.file",
				       &info, context));

	printf ("*** Getting info again (cache!)...\n");
	CHECK (gp_filesystem_get_info (fs, "/whatever/directory", "some.file",
				       &info, context));

	printf ("*** Set info about another file...\n");
	CHECK (gp_filesystem_set_info (fs, "/whatever/directory", "some.file2",
				       info, context));

	printf ("*** Getting info about this file (cache!)...\n");
	CHECK (gp_filesystem_get_info (fs, "/whatever/directory", "some.file2",
				       &info, context));

	printf ("*** Deleting a file...\n");
	CHECK (gp_filesystem_delete_file (fs, "/whatever/directory",
					  "some.file2", context));

	gp_filesystem_dump (fs);

	printf ("*** Resetting the filesystem...\n");
	CHECK (gp_filesystem_reset (fs));

	gp_filesystem_dump (fs);

	printf ("*** Getting file list for folder '/whatever/directory'...\n");
	CHECK (gp_filesystem_list_folders (fs, "/whatever/directory",
					   list, context));

	printf ("*** Getting file list for folder '/whatever/directory' "
		"again (cached!)...\n");
	CHECK (gp_filesystem_list_folders (fs, "/whatever/directory",
					   list, context));

	printf ("*** Counting the contents...\n");
	CHECK (count = gp_list_count (list));

	printf ("*** Listing the contents...\n");
	for (x = 0; x < count; x++) {
		CHECK (gp_list_get_name (list, x, &name));
		printf (" %i: '%s'\n", x, name);
	}

	printf ("*** Getting folder of 'file1'...\n");
	CHECK (gp_filesystem_get_folder (fs, "file1", &foldername, context));
	printf ("... found in '%s'.\n", foldername);
	free(foldername);

	printf ("*** Deleting a couple of files...\n");
	CHECK (gp_filesystem_delete_file (fs, "/whatever", "file5", context));
	CHECK (gp_filesystem_delete_file (fs, "/whatever", "file4", context));
	CHECK (gp_filesystem_delete_file (fs, "/whatever", "file3", context));

	gp_filesystem_dump (fs);

	printf ("*** Freeing file system...\n");
	CHECK (gp_filesystem_free (fs));

	gp_context_unref (context);

	CHECK (gp_list_free(list));

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
