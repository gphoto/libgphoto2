/* All return values are either GP_OK, or a GP_ERROR variation  */

int camera_id 			(char *id);
	/* Copies a unique library ID into 'id' 		*/
	/* This is used to uniquely identify a library so that	*/
	/* multiple libraries can support the same camera. 	*/
	/* The id is limited to 64 characters.			*/

int camera_abilities 		(CameraAbilities *abilities, 
				 int *count);
	/* Returns the camera abilities for all the cameras 	*/
	/* that the library supports. The value of 		*/
	/* 'count' should be set to the number of cameras the	*/
	/* library supports.					*/

int camera_init 		(Camera *camera, 
				 CameraInit *init);
	/* Initializes the library for use. Library uses the 	*/
	/* information in 'init' to determine what options the  */
	/* the user has chosen.					*/
	/* If init->port_settings.speed == 0, use a default 	*/
	/* speed for the camera.				*/
	/* init->debug will be set to 1 if debugging is turned	*/
	/* on.							*/

int camera_exit 		(Camera *camera);
	/* Called when library is being closed or another 	*/
	/* camera model (possibly from the same library) has	*/
	/* been chosen.						*/

int camera_folder_list		(Camera *camera, 
				 CameraList *list,
				 char *folder);
	/* Returns a list of sub-folders from the 'folder_name'	*/
	/* folder. The first call will be with folder_name	*/
	/* set to "/" to get a listing of all the top-level	*/
	/* top-level folders.					*/

int camera_file_list		(Camera *camera, 
				 CameraList *list,
				 char *folder);
	/* Returns a list of file from the 'folder_name'	*/
	/* folder. 						*/

int camera_file_get 	    	(Camera *camera,
				 CameraFile *file,
				 char *folder,
				 char *filename); 
	/* Fills in the file struct with a file #file_number	*/
	/* from the camera.					*/

int camera_file_get_preview 	(Camera *camera, 
				 CameraFile *file,
				 char *folder,
				 char *filename);
	/* Fills in the file struct with a file preview 	*/
	/* #file_number	from the camera. The preview is a	*/
	/* picture thumbnail, or snippet from a movie/sound	*/
	/* if supported.					*/

int camera_file_put 	    	(Camera *camera,
				 CameraFile *file,
				 char *folder);
	/* Uploads a file to the current folder on the camera. 	*/
	/* The user is in charge of converting an image to the	*/
	/* appropriate format. The camera_manual should explain	*/
	/* the image format to upload with directions on how	*/
	/* to do the conversion.				*/

int camera_file_delete 		(Camera *camera, 
				 char *folder,
				 char *filename);
	/* Deletes a picture from the current folder on the	*/
	/* camera. 						*/

int camera_config_get		(Camera *camera, 
				 CameraWidget *window);
	/* Builds the configuration window interface using the	*/
	/* gp_widget_* functions. This is a toolkit independent	*/
	/* widget set that is very generic and easy to use.	*/

int camera_config_set		(Camera *camera, 
				 CameraSetting *conf,
				 int count);
	/* Sets configuration values in the library. These	*/
	/* values are obtained from the configuration dialog.	*/

int camera_capture 		(Camera *camera,
				 CameraFile *file,
				 CameraCaptureInfo *info);
	/* Have the camera capture a file of type 'type'. 	*/
	/* type will be one of the generic CameraFileType's.	*/
	/* This is used to get live previews.			*/

int camera_summary 		(Camera *camera, 
			 	 CameraText *summary);
	/* Returns information about the current status of the	*/
	/* camera, such as pictures taken/left, battery		*/
	/* percent, etc...					*/

int camera_manual 		(Camera *camera, 
				 CameraText *manual);
	/* Returns information on using the camera library.	*/

int camera_about 		(Camera *camera, 
				 CameraText *about);
	/* Returns information about the camera library.	*/
	/* Includes author's name, email, models supported,	*/
	/* and anything else library-specific.			*/
