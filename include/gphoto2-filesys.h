/* NOTE: These functions are provided for cameras that do not support filenames.  */
/* This sets up an emulation layer to let those cameras work properly with the	  */
/* filename-centric gPhoto2 API.						  */

CameraFilesystem* gp_filesystem_new	 ();
int		  gp_filesystem_free	 (CameraFilesystem *fs);
int 		  gp_filesystem_populate (CameraFilesystem *fs, 
					  const char *folder, char *format, 
					  int count);
int 		  gp_filesystem_append   (CameraFilesystem *fs, 
					  const char *folder, 
					  const char *filename);
int 		  gp_filesystem_format   (CameraFilesystem *fs);
int 		  gp_filesystem_delete   (CameraFilesystem *fs, 
					  const char *folder, 
					  const char *filename);
int 		  gp_filesystem_count	 (CameraFilesystem *fs, 
					  const char *folder);
char*		  gp_filesystem_name     (CameraFilesystem *fs, 
					  const char *folder, int filenumber);
int		  gp_filesystem_number   (CameraFilesystem *fs, 
					  const char *folder, 
					  const char *filename);
