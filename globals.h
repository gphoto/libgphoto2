#include "core.h"

/* Debug setting */
extern	int	glob_debug;

/* Camera List */
extern	CameraAbilitiesList *glob_abilities_list;


/* Currently loaded settings */
extern	int	glob_setting_count; 
extern	Setting	glob_setting[512];  

/* Session counters */
extern  int	glob_session_camera;
extern  int	glob_session_file;

/* Callback function pointers */
extern	CameraStatus   gp_fe_status;
extern	CameraProgress gp_fe_progress;
extern	CameraMessage  gp_fe_message;
extern	CameraConfirm  gp_fe_confirm; 
extern	CameraPrompt   gp_fe_prompt; 
