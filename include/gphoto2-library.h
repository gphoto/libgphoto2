/* All return values are either GP_OK, or a GP_ERROR variation  */

/* NOTE: all the pointers passed in to the camera libraries	*/
/*       already allocated. here's the default allocations:	*/
/*		char*			128 characters		*/
/*		CameraAbilities*	struct allocated	*/
/*		CameraFolderInfo*	512 entries		*/
/*		CameraFile*		struct allocated, but 	*/
/*					you must allocate the	*/
/*					data.			*/	
/*		CameraSetting*		allocated enough for	*/
/*					for 'count' entries.	*/
/*		CameraWidget*		allocated a window.	*/

int camera_id 			(char *id);
	/* Copies a unique library ID into 'id' 		*/
	/* This is used to uniquely identify a library so that	*/
	/* multiple libraries can support the same camera. 	*/
	/* The id is limited to 64 characters.			*/

int camera_debug_set		(int onoff);
	/* Turns on or off camera library debugging. onoff will */
	/* be 0 to turn off debugging, or 1 to turn it on. Use  */
	/* this to set a flag in the library to be very verbose */

int camera_abilities 		(CameraAbilities *abilities, 
				 int *count);
	/* Returns the camera abilities for all the cameras 	*/
	/* that the library supports. The value of 		*/
	/* 'count' should be set to the number of cameras the	*/
	/* library supports.					*/

int camera_init 		(CameraInit *init);
	/* Initializes the library for use. Library uses the 	*/
	/* information in 'init' to determine what options the  */
	/* the user has chosen.					*/
	/* If init->port_settings.speed == 0, use a default 	*/
	/* speed for the camera.				*/

int camera_exit 		();
	/* Called when library is being closed or another 	*/
	/* camera model (possibly from the same library) has	*/
	/* been chosen.						*/

int camera_folder_list		(char *folder_path, 
				 CameraFolderInfo *folder);
	/* Returns a list of sub-folders from the 'folder_name'	*/
	/* folder. The first call will be with folder_name	*/
	/* set to "/" to get a listing of all the top-level	*/
	/* top-level folders.					*/

int camera_folder_set		(char *folder_path);
	/* Sets the current folder path on the camera to 	*/
	/* camera_path. the folder_path should have been	*/
	/* obtained from repeat calls to camera_folder_list()	*/

int camera_file_count 		();
	/* Returns the number of files in the current folder on */
	/* the camera.						*/

int camera_file_get 	    	(int file_number, 
				 CameraFile *file);
	/* Fills in the file struct with a file #file_number	*/
	/* from the camera.					*/

int camera_file_get_preview 	(int file_number, 
				 CameraFile *preview);
	/* Fills in the file struct with a file preview 	*/
	/* #file_number	from the camera. The preview is a	*/
	/* picture thumbnail, or snippet from a movie/sound	*/
	/* if supported.					*/

int camera_file_put 	    	(CameraFile *file);
	/* Uploads a file to the current folder on the camera. 	*/
	/* The user is in charge of converting an image to the	*/
	/* appropriate format. The camera_manual should explain	*/
	/* the image format to upload with directions on how	*/
	/* to do the conversion.				*/

int camera_file_delete 		(int file_number);
	/* Deletes a picture from the current folder on the	*/
	/* camera. 						*/

int camera_config_get		(CameraWidget *window);
	/* Builds the configuration window interface using the	*/
	/* gp_widget_* functions. This is a toolkit independent	*/
	/* widget set that is very generic and easy to use.	*/

int camera_config_set		(CameraSetting *conf,
				 int count);
	/* Sets configuration values in the library. These	*/
	/* values are obtained from the configuration dialog.	*/

int camera_capture 		(CameraFileType type);
	/* Have the camera capture a file of type 'type'. 	*/
	/* type will be one of the generic CameraFileType's.	*/
	/* This is used to get live previews.			*/


/* For each of the following functions, the char* buffers	*/
/* passed in are 32k large.					*/

int camera_summary 		(char *summary);
	/* Returns information about the current status of the	*/
	/* camera, such as pictures taken/left, battery		*/
	/* percent, etc...					*/

int camera_manual 		(char *manual);
	/* Returns information on using the camera library.	*/

int camera_about 		(char *about);
	/* Returns information about the camera library.	*/
	/* Includes author's name, email, models supported,	*/
	/* and anything else library-specific.			*/
