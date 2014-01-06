/*
 * sq905.c
 *
 * Here, the functions which are needed to get data from the camera.  
 *
 * Copyright (c) 2003 Theodore Kilgore <kilgota@auburn.edu>
 * Camera library support under libgphoto2.1.1 for camera(s) 
 * with chipset from Service & Quality Technologies, Taiwan. 
 * The chip supported by this driver is presumed to be the SQ905,  
 *
 * Licensed under GNU Lesser General Public License, as part of Gphoto
 * camera support project. For a copy of the license, see the file 
 * COPYING in the main source tree of libgphoto2.
 */    

#include <config.h>


#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>
#ifdef OS2
#include <db.h>
#endif

#include <gphoto2/gphoto2.h>
#include <gphoto2/gphoto2-port.h>
#include "sq905.h"

#define GP_MODULE "sq905" 

#define SQWRITE gp_port_usb_msg_write
#define SQREAD  gp_port_usb_msg_read

#define zero 	"\x0"

static int
sq_read_data (GPPort *port, unsigned char *data, int size)
{
	SQWRITE (port, 0x0c, 0x03, size, zero, 1); 
	gp_port_read (port, (char *)data, size); 
	return GP_OK;
}


int sq_access_reg (GPPort *port, int reg) 
{
	char c;
	SQWRITE (port, 0x0c, 0x06, reg, zero, 1);	/* Access a register */
	SQREAD (port, 0x0c, 0x07, 0x00, &c, 1);
	return GP_OK;
}

int 
sq_init (GPPort *port, CameraPrivateLibrary *priv)
{
	unsigned char c[0x4];
	int i;
	unsigned char *catalog = malloc(0x4000);
	unsigned char *catalog_tmp;
	if (!catalog) return GP_ERROR_NO_MEMORY;

	sq_reset (port);
    	sq_access_reg(port, ID);	/* Access model or chip id */

	sq_read_data (port, c, 4);
	sq_reset (port);
	if (!memcmp (c, "\x09\x05\x01\x19", 4)) {
		priv->model = SQ_MODEL_POCK_CAM;
	} else if (!memcmp (c, "\x09\x05\x01\x32", 4)) {
		priv->model = SQ_MODEL_MAGPIX;
/*
	} else if (!memcmp (c, "\x09\x13\x06\x67", 4)) {
		priv->model = SQ_MODEL_913c;
*/
/*
	} else if (!memcmp (c, "\x50\x05\x00\x26", 4)) {
		priv->model = SQ_MODEL_PRECISION;
*/
	} else {
		priv->model = SQ_MODEL_DEFAULT;
	}

	sq_access_reg(port, CONFIG);	     /* Access config */
	sq_read_data(port, catalog, 0x4000); /* We need 16 bytes for each photo. */
	sq_reset (port);

	/* The first occurence of a zero denotes end of files entries (here clips count as 1 entry) */
	for (i=0; i<0x4000 && catalog[i]; i+=16) ;
	priv->nb_entries = i>>4;
	if (i) {
		catalog_tmp = realloc(catalog, i);
		if (catalog_tmp)
			priv->catalog = catalog_tmp;
		else
			priv->catalog = catalog;
	} else {
		free (catalog);
		priv->catalog = NULL;	/* We just have freed catalog_tmp */
	}

	sq_reset (port);

	priv->last_fetched_entry = -1;
	free(priv->last_fetched_data);
	priv->last_fetched_data = NULL;

	return GP_OK;
}

int
sq_get_num_frames (CameraPrivateLibrary *priv, int entry)
{
	if (sq_is_clip(priv, entry)) {
		GP_DEBUG(" Number of frames in clip %i is %i\n", entry, priv->catalog[16*entry+7]);	
		return priv->catalog[16*entry+7];
	} else {
		return 1;
	}
}


int
sq_get_comp_ratio (CameraPrivateLibrary *priv, int entry)
{
	switch (priv->catalog[16*entry]) {
	case 0x61:
	case 0x62:
	case 0x63:
	case 0x76: return 2;
	case 0x41:
	case 0x42:
	case 0x43:
	case 0x52:
	case 0x53:
	case 0x56: 
	case 0x72: return 1;
	default:
		GP_DEBUG ("Your camera has unknown resolution settings.\n");
			return 0;
	}
}

int
sq_get_picture_width (CameraPrivateLibrary *priv, int entry)
{
	switch (priv->catalog[16*entry]) {  
	case 0x41:
	case 0x52:
	case 0x61: return 352;
	case 0x42:
	case 0x62:
	case 0x72: return 176;
	case 0x43:
	case 0x53:
	case 0x63: return 320;
	case 0x56:
	case 0x76: return 640;
	default:
		GP_DEBUG ("Your pictures have unknown width.\n");
			return 0;
	}
}

int
sq_is_clip (CameraPrivateLibrary *priv, int entry)
{
	switch (priv->catalog[16*entry]) {  
	case 0x52:
	case 0x53:
	case 0x72: return 1;
	default:   return 0;
	}
}

int
sq_rewind (GPPort *port, CameraPrivateLibrary *priv)
{
	static unsigned char dummy_buf[0x4000];
	
	
	GP_DEBUG("REWIND cam's data pointer");

	/* One has to read the catalog to rewind the data stream...
	 * I don't know if it's by design. There ought to be something better to do... :-/
	 */


	sq_access_reg(port, CONFIG);		/* Access config */

	sq_read_data(port, dummy_buf, 0x4000);
	sq_reset (port);
	sq_access_reg(port, DATA);	/* Access photo data */

	priv->last_fetched_entry = -1;
	free(priv->last_fetched_data);
	priv->last_fetched_data = NULL;
	return GP_OK;
}

int
sq_reset (GPPort *port)
{
	sq_access_reg(port, CLEAR);	/* Release current register */

    	return GP_OK;
}

int
sq_read_picture_data (GPPort *port, unsigned char *data, int size )
{
	int remainder = size % 0x8000;
	int offset = 0;
	char c;

	while ((offset + 0x8000 < size)) {
		sq_read_data (port, data + offset, 0x8000);
		offset = offset + 0x8000;
	}
 	sq_read_data (port, data + offset, remainder);

    	SQWRITE (port, 0x0c, 0xc0, 0x00, &c, 1);  
    	return GP_OK;
} 

int
sq_delete_all (GPPort *port, CameraPrivateLibrary *priv)
{
	/* This will work on the Argus DC-1510, perhaps some others. 
	 * Will not successfully delete on all SQ905 cameras!
	 */  
	 
	switch (priv->catalog[2]) {
	case 0xd0: 				/* Searches for DC-1510 */
		sq_access_reg(port, CAPTURE);	/* used here to delete */

		sq_reset(port);
		break;
	default:
		return (GP_ERROR_NOT_SUPPORTED);	
	}
    	return GP_OK;
}

int 
sq_preprocess (SQModel model, int comp_ratio, unsigned char is_in_clip,  
				    unsigned char *data, int w, int h)
{
	int i, m, b;
	unsigned char temp;
	b=w*h/comp_ratio;

	GP_DEBUG("Running sq_preprocess\n");

	if (!is_in_clip) {
		/* Turning the picture right-side up. */
    		for (i = 0; i < b/2; ++i) {
        		temp = data[i];
        		data[i] = data[b -1 -i];
        		data[b - 1 - i] = temp;
    		}    	
		/* But clip frames are already right-side-up */
    	}
	/*
	 * POCK_CAM needs de-mirror-imaging, too. But if a photo is 
	 * compressed we de-mirror after decompression, so not here. 
	 */
	if ((model == SQ_MODEL_POCK_CAM) && (comp_ratio == 1)) {
    		for (i = 0; i < h; i++) {
			for (m = 0 ; m < w/2; m++) { 
        			temp = data[w*i +m];
				data[w*i + m] = data[w*i + w -1 -m];
				data[w*i + w - 1 - m] = temp;
			}
    		} 
	}
	return GP_OK;
}	


