/* All return values are either GP_OK, or a GP_ERROR variation */

int camera_id (char *id);
	/* Copies a unique library ID into 'id' 		*/
	/* This is used to uniquely identify a library so that	*/
	/* multiple libraries can support the same camera. 	*/
	/* The id is limited to 64 characters.			*/

int camera_abilities (CameraAbilities *abilities, int *count);
	/* Returns the camera abilities for all the cameras 	*/
	/* that the library supports. The value of 		*/
	/* 'count' should be set to the number of cameras the	*/
	/* library supports.					*/

int camera_init (CameraInit *init);
	/* Initializes the library for use. Library uses the 	*/
	/* information in 'init' to determine what options the  */
	/* the user has chosen.					*/

int camera_exit ();
	/* Called when library is being closed or another 	*/
	/* camera model (possibly from the same library) has	*/
	/* been chosen.						*/

int camera_open ();
	/* Opens the camera for use.				*/

int camera_close ();
	/* Closes the camera after use.				*/

int camera_folder_count ();
	/* Returns the number of folders on the camera.		*/

int camera_folder_info(int folder_number, CameraFolder *folder);
	/* Returns information about a particular folder on the	*/
	/* camera.						*/

int camera_folder_set(int folder_number);

int camera_file_count ();
int camera_file_get (int file_number, CameraFile *file);
int camera_file_get_preview (int file_number, CameraFile *preview);
int camera_file_delete (int file_number);

int camera_file_lock   (int file_number);
int camera_file_unlock (int file_number);

int camera_config_set (CameraConfig *conf);
int camera_capture (int type);
int camera_summary (char *summary);
int camera_manual (char *manual);
int camera_about (char *about);
