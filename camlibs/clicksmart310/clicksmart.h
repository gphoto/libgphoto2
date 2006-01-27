/* clicksmart.h
 *
 * headers for libgphoto2/camlibs/clicksmart310
 *
 * Copyright (C) 2006 Theodore Kilgore <kilgota@auburn.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details. 
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __CLICKSMART_H__
#define __CLICKSMART_H__

#include <libgphoto2_port/gphoto2-port.h>

struct _CameraPrivateLibrary {
	unsigned char *catalog;
	int num_pics;
	unsigned char full;
};


int clicksmart_init             (GPPort *port, CameraPrivateLibrary *priv);
int clicksmart_get_res_setting  (CameraPrivateLibrary *priv, int n);
int clicksmart_read_pic_data 	(CameraPrivateLibrary *priv,
					GPPort *port, unsigned char *data, 
							    int n);
int clicksmart_delete_all_pics  (GPPort *port);

int clicksmart_reset      	(GPPort *port);

int clicksmart_decode		(unsigned char * dst, unsigned char * src, 
				    int width, int height);
int CLICKSMART_READ 		(GPPort *port, int index, char *data);
int CLICKSMART_READ_STATUS 	(GPPort *port, char *data);

int create_jpeg_from_data 	(unsigned char * dst, unsigned char * src, 
					int qIndex, int w, int h, 
					unsigned char format, 
					int o_size, int *size,
					int omit_huffman_table, 
					int omit_escape);


#endif 
