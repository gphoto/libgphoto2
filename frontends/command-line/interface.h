void status_func (Camera *camera, const char *status, void *data);
void progress_func (Camera *camera, float percentage, void *data);
int gp_interface_message (Camera *camera, char *message);
int gp_interface_confirm (Camera *camera, char *message);
