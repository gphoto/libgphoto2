/*
 * digigr8.h 
 *
 * Header file for libgphoto2/camlibs/digigr8.  
 *
 * Copyright (c) 2005 Theodore Kilgore <kilgota@auburn.edu>
 * Camera library support under libgphoto2.1.1 for camera(s) 
 * with chipset from Service & Quality Technologies, Taiwan. 
 * Cameras supported by this driver have Product ID 0x905C, 0x9050, or 
 * 0x913D.
 *
 * Licensed under GNU Lesser General Public License, as part of Gphoto
 * camera support project. For a copy of the license, see the file 
 * COPYING in the main source tree of libgphoto2.
 */    

#ifndef __DIGIGR8_H__
#define __DIGIGR8_H__

#include <gphoto2/gphoto2-port.h>

struct _CameraPrivateLibrary {
	unsigned char *catalog;
	int nb_entries;
	int last_fetched_entry;
	int delete_all;
	unsigned char init_done;
};


int digi_reset             		(GPPort *);
int digi_init                          (GPPort *, CameraPrivateLibrary *);
int digi_rewind (GPPort *port, CameraPrivateLibrary *priv);
int digi_read_picture_data  (GPPort *, unsigned char *data, 
					    int size, int n);

/* Those functions don't need data transfer with the camera */
int digi_get_num_frames                (CameraPrivateLibrary *, int entry);
int digi_get_comp_ratio      	     (CameraPrivateLibrary *, int entry);
int digi_get_data_size             (CameraPrivateLibrary *, int entry);
int digi_get_picture_width             (CameraPrivateLibrary *, int entry);
int digi_is_clip                       (CameraPrivateLibrary *, int entry);
int digi_decompress (unsigned char *out_data, unsigned char *data, 
							    int w, int h);
int digi_postprocess	(int width, int height, unsigned char* rgb);
int digi_delete_all	(GPPort *, CameraPrivateLibrary *priv);

int
white_balance (unsigned char *data, unsigned int size, float saturation);


#endif

