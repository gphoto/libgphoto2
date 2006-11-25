/* spca50x_flash.h
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

#ifndef __SPCA50X_FLASH_H__
#define __SPCA50X_FLASH_H_

#include <gphoto2/gphoto2.h>
#include <gphoto2/gphoto2-port.h>

#define CHECK(result) {int res; res = result; if (res < 0) return (res);}

int spca50x_flash_init(CameraPrivateLibrary *pl, GPContext *context);
int spca50x_flash_close(CameraPrivateLibrary *pl, GPContext *context);

int spca50x_flash_get_TOC(CameraPrivateLibrary *pl, int *filecount);
int spca50x_flash_get_file_name(CameraPrivateLibrary *pl, int index, char *name);
int spca50x_flash_get_file_dimensions(CameraPrivateLibrary *pl, int index,
		int *w, int *h);
int spca50x_flash_get_file (CameraPrivateLibrary *pl, GPContext *context,
		uint8_t **buf, unsigned int *len, int index, int thumbnail);
int spca50x_flash_get_file_size(CameraPrivateLibrary *pl, int index, int *size);
int spca50x_flash_get_filecount(CameraPrivateLibrary *pl, int *filecount);
int spca50x_flash_delete_all (CameraPrivateLibrary *pl, GPContext *context);

/* for testing */
int spca500_flash_delete_file (CameraPrivateLibrary *pl, int index);
int spca500_flash_capture (CameraPrivateLibrary *pl);

#endif
