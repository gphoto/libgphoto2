#ifndef __SAMPLES_H
#define __SAMPLES_H
#include <gphoto2/gphoto2-camera.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int sample_autodetect (CameraList *list, GPContext *context);
extern int sample_open_camera (Camera ** camera, const char *model, const char *port, GPContext *context);
extern GPContext* sample_create_context(void);

extern int get_config_value_string (Camera *, const char *, char **, GPContext *);
extern int set_config_value_string (Camera *, const char *, const char *, GPContext *);
int canon_enable_capture (Camera *camera, int onoff, GPContext *context);

extern int camera_auto_focus (Camera *list, GPContext *context, int onoff);
extern int camera_eosviewfinder (Camera *list, GPContext *context, int onoff);
extern int camera_manual_focus (Camera *list, int tgt, GPContext *context);

#if !defined (O_BINARY)
	/*To have portable binary open() on *nix and on Windows */
	#define O_BINARY 0
#endif

#ifdef __cplusplus
};
#endif

#endif
