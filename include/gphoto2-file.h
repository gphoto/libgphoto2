/* 	Header file for gPhoto2

	Author: Scott Fritzinger <scottf@unr.edu>

	This library is covered by the LGPL.
*/

CameraFile* gp_file_new ();

int gp_file_save (CameraFile *file, char *filename);
int gp_file_open (CameraFile *file, char *filename);

int gp_file_clean (CameraFile *file);
int gp_file_free  (CameraFile *file);

int gp_file_append         (CameraFile *file, char *data,  int size);
int gp_file_get_last_chunk (CameraFile *file, char **data, int *size);
