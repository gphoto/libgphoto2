#ifndef _BLINK2_H
#define _BLINK2_H
extern int web2_init(GPPort *port, GPContext *context);
extern int web2_exit(GPPort *port, GPContext *context);
extern int web2_select_picture(GPPort *port, GPContext *context, int picnum);
extern int web2_getnumpics( GPPort *port, GPContext *context, int *numpics, int *x2, int *x3, int *freemem); 
extern int web2_getexif(GPPort *port, GPContext *context, CameraFile *file);
extern int web2_getthumb(GPPort *port, GPContext *context, CameraFile *file);
extern int web2_getpicture(GPPort *port, GPContext *context, CameraFile *file);
extern int web2_get_picture_info( GPPort *port, GPContext *context, int picnum, int *perc, int *incamera, int *flags, int *unk);
extern int web2_get_file_info(GPPort *port, GPContext *context, char *name, int *filesize);
extern int web2_set_picture_attribute(GPPort *port, GPContext *context, int x, int *y);
extern int web2_set_xx_mode(GPPort *port, GPContext *context, int mode);
#endif
