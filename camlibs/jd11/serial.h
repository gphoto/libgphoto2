#ifndef _JD11_SERIAL_H
#define _JD11_SERIAL_H

#include <gphoto2.h>
#include <gphoto2-port.h>

extern int jd11_file_count(GPPort *port, int *count);
extern int jd11_get_image_preview(GPPort *port,int nr, char **data, int *size);
extern int jd11_get_image_full(GPPort *port,int nr, char **data, int *size,int raw);
extern int jd11_erase_all(GPPort *port);
extern int jd11_ping(GPPort *port);
extern int jd11_float_query(GPPort *port);
extern int jd11_select_index(GPPort *port);
extern int jd11_select_image(GPPort *port, int nr);
extern int jd11_set_bulb_exposure(GPPort *port, int nr);
extern int jd11_set_rgb(GPPort *port, float red, float green, float blue);
extern int jd11_get_rgb(GPPort *port, float *red, float *green, float *blue);
#endif
