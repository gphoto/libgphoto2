#include <string.h>
#include <stdio.h>
#include <gphoto2.h>

CameraFilesystemEntry *gp_filesystem_entry_new (char *filename, int number) {

	CameraFilesystemEntry *fse;

	fse = (CameraFilesystemEntry*)malloc(sizeof(CameraFilesystemEntry));
	
	if (!fse)
		return (NULL);

	strcpy(fse->name, filename);

	return (fse);
}	

CameraFilesystem *gp_filesystem_new() {

	CameraFilesystem *fs;

	fs = (CameraFilesystem*)malloc(sizeof(CameraFilesystem));

	if (!fs)
		return (NULL);

	fs->entry = NULL;
	fs->count = 0;

	return (fs);
}

int gp_filesystem_free(CameraFilesystem *fs) {

	gp_filesystem_format(fs);

	free(fs);

	return (GP_OK);
}

int gp_filesytem_populate (CameraFilesystem *fs, char *format, int count) {

	int x;
	char buf[1024];

	gp_filesystem_format(fs);

	fs->entry = (CameraFilesystemEntry**)malloc(sizeof(CameraFilesystemEntry*)*count);
	if (!fs->entry)
		return (GP_ERROR);

	for (x=1; x<=count; x++) {
		sprintf(buf, format, x);
		fs->entry[x-1] = gp_filesystem_entry_new(buf, x);
	}
	strcpy(fs->format, format);
	fs->count = count;

	return (GP_OK);
}

int gp_filesystem_count (CameraFilesystem *fs) {

	return (fs->count);
}

int gp_filesystem_file_delete (CameraFilesystem *fs, char *filename) {

	int x, shift=0;

	for (x=0; x<fs->count; x++) {
		if (strcmp(fs->entry[x]->name, filename)==0)
			shift = 1;
		if ((shift)&&(x<fs->count-1))
			memcpy(&fs->entry[x], &fs->entry[x+1], 
				sizeof(CameraFilesystemEntry));
	}

	if (!shift)
		return (GP_ERROR);

	fs->count -= 1;
	return (GP_OK);

}

int gp_filesystem_format (CameraFilesystem *fs) {

	if (fs->entry)
		free(fs->entry);
	fs->count = 0;

	return (GP_OK);
}

char *gp_filesystem_name (CameraFilesystem *fs, int filenumber) {

	int x;

	if (filenumber > fs->count)
		return (NULL);

	return (fs->entry[filenumber]->name);
}

int gp_filesystem_number (CameraFilesystem *fs, char *filename) {

	int x;

	for (x=0; x<fs->count; x++) {
		if (strcmp(fs->entry[x]->name, filename)==0)
			return (x);
	}

	return (GP_ERROR);
}

int gp_filesystem_dump (CameraFilesystem *fs) {
	/* debug dump the filesystem */

	int x;

	printf("core: CameraFilesystem dump:\n");

	for (x=0; x<fs->count; x++) {
		printf("core:\t%i %s\n", x, fs->entry[x]->name);
	}

	return (GP_OK);
}
