	/* Debug setting */
	/* ------------------------------------------------ */
extern	int		glob_debug;

	/* Camera List */
	/* ------------------------------------------------ */
			/* total cameras found 		*/
extern	int		glob_camera_count;
			/* camera/library list 		*/
extern	CameraChoice	glob_camera_list[512];
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
