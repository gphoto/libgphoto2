#include <gphoto2-filesys.h>
#include <gphoto2-result.h>
#include <gphoto2-core.h>

#include <stdio.h>

#define CHECK(r) {int ret = r; if (ret < 0) {printf ("Got error: %s\n", gp_result_as_string (ret)); return (1);}}

static int
set_info_func (CameraFilesystem *fs, const char *folder, const char *file,
	       CameraFileInfo *info, void *data)
{
	printf ("  -> The camera will set the file info here.\n");

	return (GP_OK);
}

static int
get_info_func (CameraFilesystem *fs, const char *folder, const char *file,
	       CameraFileInfo *info, void *data)
{
	printf ("  -> The camera will get the file info here.\n");

	info->preview.fields = GP_FILE_INFO_NONE;
	info->file.fields    = GP_FILE_INFO_NAME;
	strcpy (info->file.name, "Some file on the camera");

	return (GP_OK);
}

static int
file_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		void *data)
{
	printf ("  -> The camera will list the files in '%s' here.\n", folder);

	if (!strcmp (folder, "/whatever")) {
		gp_list_append (list, "file1", NULL);
		gp_list_append (list, "file2", NULL);
	}

	return (GP_OK);
}

static int
folder_list_func (CameraFilesystem *fs, const char *folder, CameraList *list,
		  void *data)
{
	printf ("  -> The camera will list the folders in '%s' here.\n", 
		folder);

	if (!strcmp (folder, "/")) {
		gp_list_append (list, "whatever", NULL);
		gp_list_append (list, "another", NULL);
	}

	if (!strcmp (folder, "/whatever")) {
		gp_list_append (list, "directory", NULL);
	}

	if (!strcmp (folder, "/whatever/directory")) {
		gp_list_append (list, "my_special_folder", NULL);
	}

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

	CHECK (gp_init (GP_DEBUG_HIGH));

	printf ("*** Creating file system...\n");
	CHECK (gp_filesystem_new (&fs));

	printf ("*** Setting the info callbacks...\n");
	CHECK (gp_filesystem_set_info_funcs (fs, get_info_func, set_info_func,
					     NULL));

	printf ("*** Adding a file...\n");
	CHECK (gp_filesystem_append (fs, "/", "my.file"));

	gp_filesystem_dump (fs);

	printf ("*** Removing this file...\n");
	CHECK (gp_filesystem_delete (fs, "/", "my.file"));

	gp_filesystem_dump (fs);

	printf ("*** Removing /...\n");
	CHECK (gp_filesystem_delete (fs, "/", NULL));

	gp_filesystem_dump (fs);

	printf ("*** Adding some files...\n");
	CHECK (gp_filesystem_append (fs, "/", NULL));
	CHECK (gp_filesystem_append (fs, "/whatever", NULL));
	CHECK (gp_filesystem_append (fs, "/whatever/dir", NULL));
	CHECK (gp_filesystem_populate (fs, "/whatever/dir", "file%i", 5));

	gp_filesystem_dump (fs);

	printf ("*** Deleting everything below root...\n");
	CHECK (gp_filesystem_delete_all (fs, "/"));

	gp_filesystem_dump (fs);

	printf ("*** Appending root directory...\n");
	CHECK (gp_filesystem_append (fs, "/", NULL));

	printf ("*** Appending some directories...\n");
	CHECK (gp_filesystem_append (fs, "/whatever", NULL));
	CHECK (gp_filesystem_append (fs, "/whatever/directory", NULL));

	printf ("*** Adding some files...\n");
	CHECK (gp_filesystem_append (fs, "/whatever/directory", "some.file"));
	CHECK (gp_filesystem_append (fs, "/whatever/directory", "some.file2"));
	CHECK (gp_filesystem_append (fs, "/another/directory", "another.file"));

	gp_filesystem_dump (fs);

	printf ("*** Getting info about a file...\n");
	CHECK (gp_filesystem_get_info (fs, "/whatever/directory", "some.file",
				       &info));

	printf ("*** Getting info again (cache!)...\n");
	CHECK (gp_filesystem_get_info (fs, "/whatever/directory", "some.file",
				       &info));

	printf ("*** Set info about another file...\n");
	CHECK (gp_filesystem_set_info (fs, "/whatever/directory", "some.file2",
				       &info));

	printf ("*** Getting info about this file (cache!)...\n");
	CHECK (gp_filesystem_get_info (fs, "/whatever/directory", "some.file2",
				       &info));

	printf ("*** Deleting a file...\n");
	CHECK (gp_filesystem_delete (fs, "/whatever/directory", "some.file2"));

	gp_filesystem_dump (fs);

	printf ("*** Deleting a directory...\n");
	CHECK (gp_filesystem_delete (fs, "/whatever", NULL));

	gp_filesystem_dump (fs);

	printf ("*** Formatting the filesystem...\n");
	CHECK (gp_filesystem_format (fs));

	printf ("*** Setting the listing callbacks...\n");
	CHECK (gp_filesystem_set_list_funcs (fs, file_list_func,
					     folder_list_func, NULL));

	printf ("*** Getting file list for folder '/whatever/directory'...\n");
	CHECK (gp_filesystem_list_folders (fs, "/whatever/directory", &list));

	printf ("*** Getting file list for folder '/whatever/directory' "
		"again (cached!)...\n");
	CHECK (gp_filesystem_list_folders (fs, "/whatever/directory", &list));

	printf ("*** Counting the contents...\n");
	CHECK (count = gp_list_count (&list));

	printf ("*** Listing the contents...\n");
	for (x = 0; x < count; x++) {
		CHECK (gp_list_get_name (&list, x, &name));
		printf (" %i: '%s'\n", x, name);
	}

	printf ("*** Getting folder of 'file1'...\n");
	CHECK (gp_filesystem_get_folder (fs, "file1", &name));
	printf ("... found in '%s'.\n", name);

	printf ("*** Freeing file system...\n");
	CHECK (gp_filesystem_free (fs));

	return (0);
}
