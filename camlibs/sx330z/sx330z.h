/* sx330z.h
 *
 * Copyright 2002 Dominik Kuhlen <dkuhlen@fhm.edu>
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
/*
 * GPhoto library for sx330z camera ....
 * first try 6/2002 by Dominik Kuhlen  <dkuhlen@fhm.edu>
 */


#include <gphoto2/gphoto2-camera.h>
#include <gphoto2/gphoto2-context.h>


/* USB spec violating ! */
#define USB_REQ_RESERVED 0x04

/* is this correct ? */
#define USB_VENDOR_TRAVELER	0x0d96

/* this one should work */
#define USB_PRODUCT_SX330Z	0x3300

/* very experimental !!!
 * Please report if this works
 */
#define USB_PRODUCT_SX410Z	0x4100
/* same as MD6000 */
#define USB_PRODUCT_MD9700	0x4102

/* different requests (are there more ?) */
#define SX330Z_REQUEST_INIT	0x0001		/* not sure */
#define SX330Z_REQUEST_TOC_SIZE 0x0002
#define SX330Z_REQUEST_TOC	0x0003
#define SX330Z_REQUEST_IMAGE	0x0004
#define SX330Z_REQUEST_DELETE	0x0010

#define SX_THUMBNAIL 1
#define SX_IMAGE 0


/* 0x14 Bytes TOC entry */
struct traveler_toc_entry
{
 char 		name[12];	/* SIMGxxxx.jpg (not 0 terminated) */
 int32_t 	time;		/* ? */
 int32_t 	size;		/* in bytes (%4096 is 0)*/
};

/* 0x200 Bytes TOC Page */
struct traveler_toc_page
{
 int32_t data0;			/* don't know ? (0) */
 int32_t data1;			/* don't know ? (0) */
 int16_t always1;		/* 1 ? */
 int16_t numEntries;		/* number of entries in TOC page (0 .. 25)*/
 struct traveler_toc_entry entries[25]; /* entries */
};

struct _CameraPrivateLibrary
{
 int usb_product;		/* different Thumbnail size */
};


/* 0x20 Bytes  Request	*/
struct traveler_req
{
 int16_t	always1;		/* 0x01 */
 int16_t	requesttype;		/* 0x0003 : TOC , 0x0004 Data */
 int32_t	data;			/* dontknow */
 int32_t	timestamp;		/* counter? only 24 bit  */
 int32_t 	offset;			/* fileoffset */
 int32_t 	size;			/* transfer bolcksize */
 char		filename[12];		/* SIMG????jpg for Real image */   					/* TIMG????jpg for Thumbnail */
};

/* 0x10 Bytes Acknowledge */
struct traveler_ack
{
 int32_t always3;	/* 3 */
 int32_t timestamp;	/* not sure */
 int32_t size;		/* for TOC and other transfers */
 int32_t dontknow;	/* always 0 */
};


/*
 *  There's not a real initialization,
 *  but ...
 */
int sx330z_init(Camera *camera,GPContext *context);


/*
 * Get number of TOC pages
 */
int sx330z_get_toc_num_pages(Camera *camera,GPContext *context, int32_t *pages);


/*
 * Get TOC
 */
int sx330z_get_toc_page(Camera *camera,GPContext *context,struct traveler_toc_page *toc,int page);

/*
 *  Load image data (thumbnail(exif) / image)
 */
int sx330z_get_data(Camera *camera,GPContext *context, const char* filename,
   		    char **,unsigned long int *size,int thumbnail);



/*
 *  Delete image
 */
int sx330z_delete_file(Camera *camera,GPContext *context,const char *filename);

/*
 *  Exit camera
 */
int camera_exit(Camera *camera, GPContext *context);
