#ifndef __SQ905_H__
#define __SQ905_H__

#include <libgphoto2_port/gphoto2-port.h>

typedef char SQConfig[0x4000];

unsigned int sq_config_get_num_pic (SQConfig data);
unsigned int sq_config_get_width   (SQConfig data, unsigned int n);
unsigned int sq_config_get_height  (SQConfig data, unsigned int n);
unsigned int sq_config_get_comp    (SQConfig data, unsigned int n);

int sq_init  (GPPort *port, SQConfig data);
int sq_reset (GPPort *port);

#endif /* __SQ905_H__ */
