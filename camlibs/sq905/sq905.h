#ifndef __SQ905_H__
#define __SQ905_H__

#include <libgphoto2_port/gphoto2-port.h>

typedef unsigned char SQData;

typedef enum {
	SQ_MODEL_ARGUS,
	SQ_MODEL_POCK_CAM,
	SQ_MODEL_UNKNOWN
} SQModel;

int sq_reset             (GPPort *port);
int sq_init              (GPPort *port, SQModel *, SQData *data);

/* Those functions don't need data transfer with the camera */
int sq_get_num_pics      (SQData *data); 
int sq_get_comp_ratio    (SQData *data, int n);
int sq_get_picture_width (SQData *data, int n);

unsigned char *sq_read_data         (GPPort *port, char *data, int size);
unsigned char *sq_read_picture_data (GPPort *port, char *data, int size);

#endif

