void status_func (Camera *camera, const char *status, void *data);
void progress_func (Camera *camera, float percentage, void *data);
void message_func (Camera *camera, const char *message, void *data);

int gp_interface_confirm (Camera *camera, char *message);
