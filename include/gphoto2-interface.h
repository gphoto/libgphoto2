/* 	Header file for gPhoto 0.5-Dev

	Author: Scott Fritzinger <scottf@unr.edu>

	This library is covered by the LGPL.
*/


/* Functions provided by the front-end interface
   ---------------------------------------------------------------- */

/* Displays the current status on an operation in the camera */
	int interface_status (char *status);

/* Displays a percentage done in the current operation */
	int interface_progress (float percentage);

/* Displays a message (error, very important info) */
	int interface_message (char *message);

/* Displays a configuration */
	int interface_confirm (char *message);

