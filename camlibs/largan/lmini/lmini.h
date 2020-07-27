/* library.h (for Largan)
 *
 * Copyright 2002 Hubert Figuiere <hfiguiere@teaser.fr>
 * Code largely inspired by lmini-0.1 by Steve O Connor
 * With the help of specifications for lmini camera by Largan
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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#ifndef _LARGAN_LIBRARY_H_
#define _LARGAN_LIBRARY_H_

#include <gphoto2/gphoto2.h>
#include "gphoto2-endian.h"


typedef enum {
	LARGAN_NONPICT = 0,
	LARGAN_PICT,
	LARGAN_THUMBNAIL
} largan_pict_type;


typedef struct {
	largan_pict_type	type;
	uint8_t			quality;
	uint32_t		data_size;
	char * 			data;
} largan_pict_info;



largan_pict_info * largan_pict_new (void);
void largan_pict_free (largan_pict_info *);


int largan_open (Camera * camera);
int largan_get_num_pict (Camera * camera);
int largan_get_pict (Camera * camera, largan_pict_type type,
		uint8_t index, largan_pict_info * pict);
int largan_erase (Camera *camera, int all);
int largan_capture (Camera *camera);
#endif
