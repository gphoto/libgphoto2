#include <gphoto2-filesys.h>
#include <gphoto2-result.h>
#include <gphoto2-core.h>

#include <stdio.h>

#define CHECK(r) {int ret = r; if (ret != GP_OK) {printf ("Got error: %s\n", gp_result_as_string (ret)); return (1);}}

int
main (int argc, char **argv)
{
	CameraFilesystem *fs;

	CHECK (gp_init (GP_DEBUG_HIGH));

	printf ("*** Creating file system...\n");
	fs = gp_filesystem_new ();

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
