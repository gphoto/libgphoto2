int gp_interface_status (Camera *camera, char *status);
int gp_interface_message (Camera *camera, char *message);
int gp_interface_confirm (Camera *camera, char *message);
int gp_interface_progress (Camera *camera, CameraFile *file, float percentage);
