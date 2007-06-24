#ifndef __SQ905_H__
#define __SQ905_H__

#include <gphoto2/gphoto2-port.h>

/* A 'picture' is a single standalone picture.
 * A 'clip' is an autoshoot clip.
 * A 'frame' is a single a picture out of a clip.
 * An 'entry' corresponds to a line listed in the camera's configuration data,
 * part of which is a rudimentary allocation table (the 'catalog'). 
 * An entry, therefore, may be either a picture or a clip. 
 * We reserve the word 'file' for the user fetchable file,
 * that is, a picture or a frame.
 */

typedef enum {
	SQ_MODEL_POCK_CAM,
	SQ_MODEL_PRECISION,
	SQ_MODEL_MAGPIX, 
/*	SQ_MODEL_913c, */
	SQ_MODEL_DEFAULT
} SQModel;

struct _CameraPrivateLibrary {
	SQModel model;
	unsigned char *catalog;
	int nb_entries;
	int last_fetched_entry;
	unsigned char *last_fetched_data;
};


#define ID	0xf0
#define CONFIG	0x20
#define DATA	0x30
#define CLEAR	0xa0
#define CAPTURE 0x61


int sq_access_reg 		     (GPPort *, int reg);
int sq_reset             		(GPPort *);
int sq_rewind                        (GPPort *, CameraPrivateLibrary *);
int sq_init                          (GPPort *, CameraPrivateLibrary *);
int sq_read_picture_data  (GPPort *, unsigned char *data, int size);
int sq_delete_all                    (GPPort *, CameraPrivateLibrary *);

/* Those functions don't need data transfer with the camera */
int sq_get_num_frames                (CameraPrivateLibrary *, int entry);
 
int sq_get_comp_ratio      	     (CameraPrivateLibrary *, int entry);
int sq_get_picture_width             (CameraPrivateLibrary *, int entry);
int sq_is_clip                       (CameraPrivateLibrary *, int entry);

int
sq_preprocess		(SQModel model, int comp_ratio, 
				unsigned char is_in_clip, 
				unsigned char *data, int w, int h);
int
sq_decompress 		(SQModel model, unsigned char *output, unsigned char *data, 
				int w, int h);
#endif

