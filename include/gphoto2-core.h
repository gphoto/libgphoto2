/* 	Header file for gPhoto2

	Author: Scott Fritzinger <scottf@unr.edu>

	This library is covered by the LGPL.
*/


	/* Save/retrieve library or front-end configuration settings. */
	/* ============================================================================== */

	/* Set the "key" to "value" */
	int gp_set_setting (char *key, char *value);

	/* Retrieve the value of "key" into "value" */
	int gp_get_setting (char *key, char *value);

	/* Initialize/Quit the gPhoto library */
	/* ============================================================================== */
	int gp_init();
	int gp_exit();

	/* Debugging options */
	/* ============================================================================== */

	/* Turn on or off debugging information (very verbose!) in the gPhoto core */
	int gp_debug_set(int value);
		/* value=0, debugging off */
		/* value=1, debugging on  */

	/* Port information */
	/* ============================================================================== */

	/* Retrieve the number of ports available */
	int gp_port_count();

	/* Retrieve port information */
	int gp_port_info(int port_number, CameraPortInfo *info);

	/* Camera information operations */
	/* ============================================================================== */

	/* Retrieve number of cameras */
	int gp_camera_count ();

	/* Retrieve camera name */
	int gp_camera_name (int camera_number, char *camera_name);

	/* Retrieve the camera abilities */
	/* ============================================================================== */
	int gp_camera_abilities (int camera_number, CameraAbilities *abilities);
	int gp_camera_abilities_by_name (char *camera_name, CameraAbilities *abilities);

	/* Set the current camera library */
	/* ============================================================================== */
	int gp_camera_set (int camera_number, CameraPortSettings *port);
	int gp_camera_set_by_name (char *camera_name, CameraPortSettings *port);

	/* Init/Exit, a camera library */
	/* ============================================================================== */
	int gp_camera_init (CameraInit *init);
	int gp_camera_exit ();

	/* Folder operations */
	/* ============================================================================== */
	/* Retrieve the contents of a folder, returns the count */
	int gp_folder_list(char *folder_path, CameraFolderInfo *list);

	/* Set the current folder */
	int gp_folder_set (char *folder_path);

	/* File transfer functions */
	/* ============================================================================== */
	/* Retrieve the number of files in the current folder */
	int gp_file_count ();

	/* Download a file or a preview from the camera from the current folder*/
	int gp_file_get (int file_number, CameraFile *file);
	int gp_file_get_preview (int file_number, CameraFile *preview);

	/* Upload a file to the camera */
	int gp_file_put (CameraFile *file);

	/* Delete a file from the current folder */
	int gp_file_delete (int file_number);


	/* CameraFile utility functions */
	/* ============================================================================== */

	/* Allocates a new CameraFile */
	CameraFile* gp_file_new ();

	/* Saves/Open image (full path and filename). File data in "file" */
	int gp_file_save (CameraFile *file, char *filename);
	int gp_file_open (CameraFile *file, char *filename);

	/* Frees a CameraFile's components. Same as freeing and creating a new CameraFile */
	int gp_file_clean (CameraFile *file);

	/* Frees a CameraFile from memory */
	int gp_file_free (CameraFile *file);

	/* Camera configuration operations */
	/* ============================================================================== */

	/* Retrieve and set configuration options for the camera */
	int gp_config_get (CameraWidget *window);

	/* Send configuration settings to the camera library */
	int gp_config_set (CameraSetting *setting, int count);

	/* Miscellaneous functions */
	/* ============================================================================== */

	/* Captures the current view. Basically, it takes a picture */
	int gp_capture (int type);

	/* Retrieves camera status (number of pictures, charge, etc...) */
	int gp_summary (char *summary);

	/* Retrieves usage of the camera library (config settings, problems, etc) */
	int gp_manual (char *manual);

	/* Retrieves information about the camera library (author, version, etc) */
	int gp_about (char *about);

	/* Widget functions */
	/* ============================================================================== */

	/* Create a new widget */
	CameraWidget* gp_widget_new(CameraWidgetType type, char *label);

	/* Add a child widget to the parent widget */
	int gp_widget_append (CameraWidget *parent, CameraWidget *child);
	int gp_widget_prepend(CameraWidget *parent, CameraWidget *child);

	/* Retrieve the number of children a parent has */
	int gp_widget_child_count(CameraWidget *parent);

	/* Retrieve a pointer to a child #child_number of the parent */
	CameraWidget* gp_widget_child(CameraWidget *parent, int child_number);

	/* Frees a widget, as well as all the children */
	int gp_widget_free(CameraWidget *widget);


	/* Utility functions for libraries */
	/* ============================================================================== */

	/* Displays the current status on an operation in the camera */
	int gp_status (char *status);

	/* Displays a percentage done in the current operation */
	int gp_progress (float percentage);

	/* Displays a message */
	int gp_message (char *message);

	/* Displays a confirmation. returns 1 if answer was "yes", 0 for "no" */
	int gp_confirm (char *message);
