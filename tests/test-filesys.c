#include <gphoto2-filesys.h>
#include <gphoto2-result.h>
#include <gphoto2-core.h>

#include <stdio.h>

#define CHECK(r) {int ret = r; if (ret != GP_OK) {printf ("Got error: %s\n", gp_result_as_string (ret)); return (1);}}

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

int
main (int argc, char **argv)
{
	CameraFilesystem *fs;
	CameraFileInfo info;

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

	printf ("*** Freeing file system...\n");
	CHECK (gp_filesystem_free (fs));

	return (0);
}
