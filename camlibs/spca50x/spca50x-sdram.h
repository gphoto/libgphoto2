/****************************************************************/
/* spca50x_sdram.h -  Gphoto2 library for cameras with sunplus  */
/*                    spca50x chips                             */
/*                                                              */
/* Copyright 2002, 2003 Till Adam                               */
/*                                                              */
/* Author: Till Adam <till@adam-lilienthal.de>                  */
/*                                                              */
/* This library is free software; you can redistribute it       */
/* and/or modify it under the terms of the GNU Library General  */
/* Public License as published by the Free Software Foundation; */
/* either version 2 of the License, or (at your option) any     */
/* later version.                                               */
/*                                                              */
/* This library is distributed in the hope that it will be      */
/* useful, but WITHOUT ANY WARRANTY; without even the implied   */
/* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR      */
/* PURPOSE.  See the GNU Library General Public License for     */
/* more details.                                                */
/*                                                              */
/* You should have received a copy of the GNU Library General   */
/* Public License along with this library; if not, write to the */
/* Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,*/
/* Boston, MA  02110-1301  USA					*/
/****************************************************************/

#ifndef __SPCA50X_SDRAM_H__
#define __SPCA50X_SDRAM_H__
#include <gphoto2/gphoto2-camera.h>

int spca50x_sdram_get_info (CameraPrivateLibrary * lib);
int spca50x_sdram_delete_file (CameraPrivateLibrary * lib, unsigned int index);
int spca50x_sdram_delete_all (CameraPrivateLibrary * lib);
int spca50x_sdram_get_file_info (CameraPrivateLibrary * lib, unsigned int index,
			  struct SPCA50xFile **file);
int spca50x_sdram_request_file (CameraPrivateLibrary * lib, uint8_t ** buf,
			 unsigned int *len, unsigned int number, int *type);
int spca50x_sdram_request_thumbnail (CameraPrivateLibrary * lib,
		uint8_t ** buf, unsigned int *len,
		unsigned int number, int *type);

#endif /* __SPCA50X_SDRAM_H__ */
