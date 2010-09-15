/*
 * iclick.c
 *
 * Here, the functions which are needed to get data from the camera.  
 *
 * Copyright (c) 2004 Theodore Kilgore <kilgota@auburn.edu>, 
 * Stephen Pollei <stephen_pollei@comcast.net>.
 * Camera library support under libgphoto2.1.1 for camera(s) 
 * with chipset from Service & Quality Technologies, Taiwan. 
 * The chip supported by this driver is presumed to be the SQ915.  
 *
 * This library is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details. 
 *
 * Licensed under GNU Lesser General Public License, as part of Gphoto
 * camera support project. For a copy of the license, see the file 
 * COPYING in the main source tree of libgphoto2.
 */    



#ifndef __ICLICK_H__
#define __ICLICK_H__

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
	SQ_MODEL_ICLICK,
	SQ_MODEL_DEFAULT
} SQModel;



struct _CameraPrivateLibrary {
	SQModel model;
	unsigned char *catalog;
	int nb_entries;
	int data_offset;
};


/* #define ID	0xf0 */
enum icl_cmnd_type {
	CONFIG =0x20,
	DATA   =0x30,
	CLEAR  =0xa0,
	CAPTURE=0x61,
};

enum icl_index_type {
	CONFIG_I =0x300,
	DATA_I   =0x000,
	CLEAR_I  =0x000,
	CAPTURE_I=0x000,
};


int icl_access_reg 		     (GPPort *, enum icl_cmnd_type);

#if 0
#define icl_access_reg(iar_port, iar_kind) do { \
	SQWRITE (iar_port, 0x0c, iar_kind, iar_kind##_I, NULL, 0); \
	} while (0)
#endif

int icl_reset             		(GPPort *);
int icl_rewind                        (GPPort *, CameraPrivateLibrary *);
int icl_init                          (GPPort *, CameraPrivateLibrary *);
int icl_read_picture_data  (GPPort *, unsigned char *data, int size);

/* Those functions don't need data transfer with the camera */

int icl_get_start                     (CameraPrivateLibrary *, int entry);
int icl_get_size                      (CameraPrivateLibrary *, int entry);
int icl_get_width_height              (CameraPrivateLibrary *, int entry, int *w, int *h);

#endif

