/* 	Header file for gPhoto2

	Author: Scott Fritzinger <scottf@unr.edu>

	This library is covered by the LGPL.
*/

int 	gp_init(int debug);  
int 	gp_exit();

void 	gp_debug_printf(int level, char *id, char *format, ...);

int 	gp_frontend_register(CameraStatus status, CameraProgress progress, 
			     CameraMessage message, CameraConfirm confirm,
			     CameraPrompt prompt);

char   *gp_result_as_string (int result);
