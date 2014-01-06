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
#include "iclick.h"

#define GP_MODULE "iclick" 

#define SQWRITE gp_port_usb_msg_write
#define SQREAD  gp_port_usb_msg_read

#define zero 	"\x0"

/*
static unsigned char *
icl_read_data (GPPort *port, unsigned char *data, int size)
{
	gp_port_read (port, data, size); 
	return GP_OK;
}
*/

int icl_access_reg (GPPort *port, enum icl_cmnd_type reg) 
{
	SQWRITE (port, 0x0c, reg,
			(CONFIG == reg) ? CONFIG_I : 0,
			NULL, 0);	/* Access a register */
	return GP_OK;
}

int 
icl_init (GPPort *port, CameraPrivateLibrary *priv)
{
	int i;
	static unsigned char dummy_buf[0x28000];
	unsigned char *catalog = malloc(0x8000);
	unsigned char *catalog_tmp;

	priv->model=SQ_MODEL_ICLICK;
	
	
	if (!catalog) return GP_ERROR_NO_MEMORY;


	icl_reset (port);

	icl_access_reg(port, CONFIG);	     /* Access config */
	gp_port_read(port, (char *)catalog, 0x8000); 
	/* Config data is in lines of 16 bytes. Each photo uses two lines. */
	icl_read_picture_data(port, dummy_buf, 0x28000);
	icl_reset (port);

	/* Photo list starts with line 0040. List terminates when we 
	 * the first entry of an even-numbered line is zero. */
	for (i=0; i< (0x8000 - 64) && catalog[i+64] ; i+=32) 
		;
	priv->nb_entries = i>>5;
	if (i) {
		catalog_tmp = realloc(catalog, i);
		if (catalog_tmp) 
			priv->catalog = catalog_tmp;
		else 
			priv->catalog = catalog;
	} else {
		free(catalog);
		priv->catalog = NULL;	/* We just have freed catalog_tmp */
	}

	icl_reset (port);

	priv->data_offset = -1;

	return GP_OK;
}

int
icl_get_start (CameraPrivateLibrary *priv, int entry)
{
	int start = 0x4b100 * entry;
	return start;
}

int
icl_get_size (CameraPrivateLibrary *priv, int entry)
{
	return 0x4b100;
}


int
icl_get_width_height (CameraPrivateLibrary *priv, int entry, int *w, int *h)
{
	*w = 640; 
	*h = 480; 
	return GP_OK;
}


int
icl_rewind (GPPort *port, CameraPrivateLibrary *priv)
{
	static unsigned char dummy_buf[0x30000];
	
	GP_DEBUG("REWIND cam's data pointer");

	/* One has to read the catalog to rewind the data stream...
	 * I don't know if it's by design. 
	 * There ought to be something better to do... :-/
	 */


	icl_access_reg(port, CONFIG);		/* Access model */

	icl_read_picture_data(port, dummy_buf, 0x30000);

	icl_reset (port);
	icl_access_reg(port, DATA);	/* Access photo data */

	priv->data_offset = icl_get_start (priv, 0);

	return GP_OK;
}

int
icl_reset (GPPort *port)
{
	icl_access_reg(port, CLEAR);	/* Release current register */

    	return GP_OK;
}

int 
icl_read_picture_data (GPPort *port, unsigned char *data, int size )
{
	int remainder = size % 0x8000;
	int offset = 0;

	while ((offset + 0x8000) <= size) {
		gp_port_read (port, (char *)data + offset, 0x8000);
		offset += 0x8000;
	}
	if (remainder)
		gp_port_read (port, (char *)data + offset, remainder);

    	return GP_OK;
} 


