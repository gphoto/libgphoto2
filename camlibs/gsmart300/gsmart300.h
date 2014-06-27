/****************************************************************/
/* gsmart300.h - Gphoto2 library for the Mustek gSmart 300      */
/*                                                              */
/* Copyright (C) 2002 Jérôme Lodewyck                           */
/*                                                              */
/* Author: Jérôme Lodewyck <jerome.lodewyck@ens.fr>             */
/*                                                              */
/* based on code by: Till Adam <till@adam-lilienthal.de>        */
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

#ifndef __GSMART300_H__
#define __GSMART300_H__
#include <_stdint.h>
#include <gphoto2/gphoto2-camera.h>

#define FLASH_PAGE_SIZE_300 0x200
#define GSMART_FILE_TYPE_IMAGE 0x00

#define CHECK(result) {int res; res = result; if (res < 0) return (res);}

struct GsmartFile
{
	char *name;
	int width;
	int height;
	int index;
	uint8_t *fat;
	int mime_type;
};

struct _CameraPrivateLibrary
{
	GPPort *gpdev;
	int dirty;
	int num_files;
	uint8_t *fats;
	struct GsmartFile *files;
};

int gsmart300_reset (CameraPrivateLibrary * lib);
int gsmart300_get_info (CameraPrivateLibrary * lib);
int gsmart300_delete_file (CameraPrivateLibrary * lib, unsigned int index);
int gsmart300_delete_all (CameraPrivateLibrary * lib);
int gsmart300_get_file_info (CameraPrivateLibrary * lib, unsigned int index,
			  struct GsmartFile **file);
int gsmart300_request_file (CameraPrivateLibrary * lib, CameraFile *file,
			 unsigned int number);
int gsmart300_request_thumbnail (CameraPrivateLibrary * lib, CameraFile *file,
			      unsigned int number, int *type);

#endif /* __GSMART300_H__ */
