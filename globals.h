extern	int		glob_debug;

extern	CameraPortSettings glob_port_settings;

	/* Currently Selected camera/folder */
	/* ------------------------------------------------ */
			/* currently selected camera number */
extern	int 		glob_camera_number;
			/* currently selected folder path */
extern	char 		glob_folder_path[512];

	/* Camera List */
	/* ------------------------------------------------ */
			/* total cameras found 		*/
extern	int		glob_camera_count;
			/* camera/library list 		*/
extern	CameraChoice	glob_camera[512];
			/* camera abilities 		*/
extern	CameraAbilities glob_camera_abilities[512];
			/* camera library id's 		*/
extern	char		*glob_camera_id[512];
			/* number of camera library id's*/
extern	int		glob_camera_id_count;

	/* Currently loaded settings */
	/* ------------------------------------------------ */
			/* number of settings found 	*/
extern	int		glob_setting_count; 
			/* setting key/value list   	*/
extern	Setting		glob_setting[512];  


	/* Current loaded library's handle */
	/* ------------------------------------------------ */
			/* current library handle    	*/
extern	void 		*glob_library_handle; 
			/* pointer to current camera function set */
extern	Camera   	glob_c;
