/* 	Header file for gPhoto 0.5-Dev

	Author: Scott Fritzinger <scottf@unr.edu>

	This library is covered by the LGPL.
*/


/* Functions required to be provided by the front-end interface
   ---------------------------------------------------------------- */

/* Displays the current status on an operation in the camera */
	int gp_interface_status (char *status);

/* Displays a percentage done in the current operation */
	int gp_interface_progress (float percentage);

/* Displays a message (error, very important info) */
	int gp_interface_message (char *message);

/* Displays a configuration */
	int gp_interface_confirm (char *message);
