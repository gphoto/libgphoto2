/* spca504_flash.h
 *  
 *  This library is free software; you can redistribute and/or
 *  modify it inder the terms of the GNU Lesser Genreral Public
 *  License as publiced by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warrenty of
 *  MERCHANTABULITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 *  lesser General Public License for more details.
 *
 *  Author: Till Adam <till@adam-lilienthal.de>
 *  based on work by: Mark A. Zimmerman <mark@foresthaven.com>           
 */

#ifndef __SPCA504_FLASH_H__
#define __SPCA504_FLASH_H__

#include <_stdint.h>
#include <gphoto2.h>
#include <gphoto2-port.h>

#define CHECK(result) {int res; res = result; if (res < 0) return (res);}

struct _CameraPrivateLibrary
{
	GPPort *gpdev;
	uint8_t *toc;
	int dirty :1;
};



int spca504_flash_init(GPPort *port, GPContext *context);
int spca504_flash_close(GPPort *port, GPContext *context);

int spca504_flash_get_TOC(CameraPrivateLibrary *pl, int *filecount);
int spca504_flash_get_file_name(CameraPrivateLibrary *pl, int index, char *name);
int spca504_flash_get_file_dimensions(CameraPrivateLibrary *pl, int index,
		int *w, int *h);
int spca504_flash_get_file (CameraPrivateLibrary *lib, GPContext *context, 
		uint8_t **buf, unsigned int *len, int index, int thumbnail);
int spca504_flash_get_file_size(CameraPrivateLibrary *pl, int index, int *size);
int spca504_flash_get_filecount(GPPort *port, int *filecount);
#endif 

