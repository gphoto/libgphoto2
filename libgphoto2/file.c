#include <stdlib.h>
#include <stdio.h>
#include <gphoto2.h>
#include "file.h"


CameraFile* gp_file_new () {

	/*
	Allocates a new CameraFile
	*/

	CameraFile *f;

	f = (CameraFile*)malloc(sizeof(CameraFile));
	f->type = GP_FILE_UNKNOWN;
	strcpy(f->name, "");
	f->data = NULL;
	f->size = 0;

	return(f);
}

int gp_file_save (CameraFile *file, char *filename) {

	FILE *fp;

	if ((fp = fopen(filename, "w"))==NULL)
		return (GP_ERROR);
	fwrite(file->data, (size_t)sizeof(char), (size_t)file->size, fp);
	fclose(fp);

	return (GP_OK);
}

int gp_file_open (CameraFile *file, char *filename) {

	return (GP_ERROR);
}

int gp_file_clean (CameraFile *file) {

	/*
	frees a CameraFile's components, not the CameraFile itself.
	this is used to prep a CameraFile struct to be filled.
	*/

	if (file->data != NULL)
		free(file->data);
	strcpy(file->name, "");
	file->data = NULL;
	file->size = 0;
	return(GP_OK);
}

int gp_file_free (CameraFile *file) {

	/*
	frees a CameraFile from memory
	*/

	gp_file_clean(file);
	free(file);
	return(GP_OK);
}
