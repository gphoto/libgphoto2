/****************************************************************/
/* gsmart.h - Gphoto2 library for the Mustek gSmart mini 2      */
/*                                                              */
/* Copyright (C) 2002 Till Adam                                 */
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
/* Free Software Foundation, Inc., 59 Temple Place - Suite 330, */
/* Boston, MA 02111-1307, USA.                                  */
/****************************************************************/

#ifndef __GSMART_H__
#define __GSMART_H__
#include <gphoto2-camera.h>

#define FLASH_PAGE_SIZE 0x100
#define GSMART_FILE_TYPE_IMAGE 0x00
#define GSMART_FILE_TYPE_AVI 0x01

#define CHECK(result) {int res; res = result; if (res < 0) return (res);}

struct GsmartFile
{
	char *name;
	int width;
	int height;
	int fat_start;
	int fat_end;
	u_int8_t *fat;
	int mime_type;
};

typedef enum {
           GSMART_BRIDGE_SPCA500,
	   GSMART_BRIDGE_SPCA504A
} GsmartBridgeChip;

struct _CameraPrivateLibrary
{
	GPPort *gpdev;
	GsmartBridgeChip bridge;
	int dirty;
	int num_files;
	int num_images;
	int num_movies;
	int size_used;
	int size_free;
	u_int8_t *fats;
	struct GsmartFile *files;
};

int gsmart_reset (CameraPrivateLibrary * lib);
int gsmart_capture (CameraPrivateLibrary * lib);
int gsmart_get_file_count (CameraPrivateLibrary * lib);
int gsmart_get_info (CameraPrivateLibrary * lib);
int gsmart_delete_file (CameraPrivateLibrary * lib, unsigned int index);
int gsmart_delete_all (CameraPrivateLibrary * lib);
int gsmart_get_file_info (CameraPrivateLibrary * lib, unsigned int index,
			  struct GsmartFile **file);
int gsmart_request_file (CameraPrivateLibrary * lib, u_int8_t ** buf,
			 unsigned int *len, unsigned int number, int *type);
int gsmart_request_thumbnail (CameraPrivateLibrary * lib, u_int8_t ** buf,
			      unsigned int *len, unsigned int number, int *type);


#endif /* __GSMART_H__ */
