#ifndef __SAMPLES_H
#define __SAMPLES_H
#include <gphoto2/gphoto2-camera.h>

extern int sample_autodetect (CameraList *list, GPContext *context);
extern int sample_open_camera (Camera ** camera, const char *model, const char *port);
extern GPContext* sample_create_context(void);
#endif
