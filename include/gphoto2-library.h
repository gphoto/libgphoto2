/* All return values are either GP_OK, or a GP_ERROR variation  */

int camera_id 			(CameraText *id);
	/* Copies a unique library ID into 'id' 		*/
	/* This is used to uniquely identify a library so that	*/
	/* multiple libraries can support the same camera. 	*/
	/* The id is limited to 64 characters.			*/

int camera_abilities 		(CameraAbilitiesList *list);
	/* Returns the camera abilities for all the cameras 	*/
	/* that the library supports. The value of 		*/
	/* 'count' should be set to the number of cameras the	*/
	/* library supports.					*/

int camera_init 		(Camera *camera);
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

int camera_file_info_get        (Camera *camera,
                                 CameraFileInfo *info,
                                 char *folder,
                                 char *filename);
        /* Returns information about a file on the camera       */

int camera_file_info_set	(Camera *camera,
				 CameraFileInfo *info,
				 char *folder,
				 char *filename);
	/* Sets information about a file on the camera		*/

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

int camera_file_config_get	(Camera *camera,
				 CameraWidget **window,
				 char *folder,
				 char *filename);
	/* Constructs a window containing various configuration	*/
	/* options for files like 'protected status' or		*/
	/* 'rotate'.						*/

int camera_file_config_set	(Camera *camera,
				 CameraWidget *window,
				 char *folder,
				 char *filename);
	/* Sets the file configuration as given in the window  	*/
	/* which has previously been constructed by 		*/
	/* camera_file_config_get and modified (values) by the	*/
	/* frontend. 						*/

int camera_folder_config_get	(Camera *camera,
				 CameraWidget **window,
				 char *folder);
	/* Constructs a window containing various configuration	*/
	/* options for folders like 'delete all'.		*/

int camera_folder_config_set 	(Camera *camera,
				 CameraWidget *window,
				 char *folder);
	/* Sets the folder configuration as given in the window	*/
	/* which has previously been constructed by 		*/
	/* camera_folder_config_get and modified (values) by  	*/
	/* the frontend. 					*/

int camera_config_get		(Camera *camera, 
				 CameraWidget **window);
	/* Constructs a window containing various configuration	*/
	/* options for the camera like 'flash'.			*/

int camera_config_set		(Camera *camera,
				 CameraWidget *window);
	/* Sets the camera configuration as given in the window	*/
	/* which has previously been constructed by		*/
	/* camera_config_get and modified (values) by the 	*/
	/* frontend.						*/

int camera_folder_put_file    	(Camera *camera,
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

int camera_folder_delete_all   	(Camera *camera,
                                 char *folder);
        /* Delete all files in a given folder.                  */
        /* Some cameras have a built-in command that makes this */
        /* instantaneous. If yours doesn't, don't support this. */


int camera_config		(Camera *camera);
	/* Builds the configuration window interface using the	*/
	/* gp_widget_* functions. This is a toolkit independent	*/
	/* widget set that is very generic and easy to use.	*/

int camera_capture 		(Camera *camera,
				 CameraFilePath *path,
				 CameraCaptureSetting *setting);
        /* Capture options are stored in the option struct.     */
        /* The library should return the full path and name of  */
        /* the file that was just captured.                     */

int camera_capture_preview     	(Camera *camera,
                                 CameraFile *file);
        /* Download a quick from the camera. This is used for   */
        /* cameras that support quick CCD dumps such that the   */
        /* the shutter on the camera doesn't need to be pressed */
        /* to take a picture. If the shutter needs to be        */
        /* pressed, OR if there is a file stored on the camera  */
        /* when this is called, use camera_capture instead and  */
        /* don't support this. */

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

char *camera_result_as_string 	(Camera *camera, 
				 int result);
	/* Returns a pointer to a description of the result.	*/
	/* Do not free it.					*/
