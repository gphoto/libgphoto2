#ifndef __SONYDSCF1_H__
#define __SONYDSCF1_H__

#include <gphoto2-port.h>

#define JPEG 0
#define JPEG_T 1
#define PMP 2
#define PMX 3

typedef struct {
        gp_port *dev;
        gp_port_settings settings;
} SonyStruct;

#endif /* __SONYDSCF1_H__ */
