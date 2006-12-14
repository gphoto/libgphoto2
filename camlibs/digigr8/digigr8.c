/*
 * digigr8.c
 *
 * Part of libgphoto2/camlibs/digigr8
 * Here are the functions which are needed to get data from the camera.  
 *
 * Copyright (c) 2005 Theodore Kilgore <kilgota@auburn.edu>
 * Camera library support under libgphoto2.1.1 for camera(s) 
 * with chipset from Service & Quality Technologies, Taiwan. 
 * The chip supported by this driver is suspected to be the SQ914,  
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
#include "digigr8.h"

#define GP_MODULE "digigr8" 

#define SQWRITE gp_port_usb_msg_write
#define SQREAD  gp_port_usb_msg_read

int 
digi_init (GPPort *port, CameraPrivateLibrary *priv)
{
	unsigned char c[0x14];
	int i;
	unsigned char *catalog = malloc(0x4000);
	unsigned char *catalog_tmp;
	if (!catalog) return GP_ERROR_NO_MEMORY;

	SQWRITE (port, 0x0c, 0x14f4, 0x0, NULL, 0);
	SQREAD (port, 0x0c, 0xf5, 0x00, c, 0x14);
	SQWRITE (port, 0x0c, 0x1440, 0x110f, NULL, 0);
	digi_reset (port);
	SQWRITE (port, 0x0c, 0x14f0, 0x0, NULL, 0);

	gp_port_read (port, c, 0x14); 	
	digi_reset(port);
	SQWRITE (port, 0x0c, 0x20, 0x40, NULL, 0);
	gp_port_read(port, catalog, 0x4000); /* We need 16 bytes for each photo. */
	digi_reset (port);

	/* The first occurence of a zero denotes end of files entries */
	for (i=0; i<0x4000 && catalog[i]; i+=16) ;
	priv->nb_entries = i>>4;
	catalog_tmp = realloc(catalog, i);
	if (i) {
	if (catalog_tmp) priv->catalog = catalog_tmp;
	else priv->catalog = catalog;
	} else {
		priv->catalog = NULL;	/* We just have freed catalog_tmp */
	}

	digi_reset (port);
	priv->last_fetched_entry = -1;

	return GP_OK;
}


unsigned char
digi_get_comp_ratio (CameraPrivateLibrary *priv, int entry)
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
digi_get_data_size (CameraPrivateLibrary *priv, int entry)
{
	return ((priv->catalog[16*entry + 6] <<16) 
		| (priv->catalog[16*entry + 5] <<8) 
		| (priv->catalog[16*entry + 4] ) );
}

int
digi_get_picture_width (CameraPrivateLibrary *priv, int entry)
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
digi_rewind (GPPort *port, CameraPrivateLibrary *priv)
{
	static unsigned char dummy_buf[0x4000];
	
	
	GP_DEBUG("REWIND cam's data pointer");

	/* One has to read the catalog to rewind the data stream...
	 * I don't know if it's by design. There ought to be something better to do... :-/
	 */

	digi_reset(port);
	SQWRITE (port, 0x0c, 0x20, 0x40, NULL, 0); /* Access config */
	gp_port_read(port, dummy_buf, 0x4000); 
	digi_reset (port);

	priv->last_fetched_entry = -1;
	return GP_OK;
}


int
digi_reset (GPPort *port)
{
	/* Release current register */
	SQWRITE (port, 0x0c, 0xa0, 0x00, NULL, 0);

    	return GP_OK;
}

unsigned char *
digi_read_picture_data (GPPort *port, unsigned char *data, int size, int n )
{


	int remainder = size % 0x8000;
	int offset = 0;
	if (!n) {
    		SQWRITE (port, 0x0c, 0x30, 0x00, NULL, 0); 
	}	
	while ((offset + 0x8000 < size)) {
		gp_port_read (port, data + offset, 0x8000);
			offset = offset + 0x8000;
	}
 	gp_port_read (port, data + offset, remainder);

    	return GP_OK;
} 
