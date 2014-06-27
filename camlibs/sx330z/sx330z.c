/* sx330z.c
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

#include "sx330z.h"

/* sprintf */
#include "stdio.h"

/* malloc */
#include <stdlib.h>

/* strncmp */
#include <string.h>

#include "gphoto2-endian.h"

/*convert to correct endianness */
static int
sx330z_fill_req(uint8_t *buf, struct traveler_req *req)
{
 *((int16_t*)buf) = htole16(req->always1);     buf += 2;
 *((int16_t*)buf) = htole16(req->requesttype); buf += 2;
 *((int32_t*)buf) = htole32(req->data);        buf += 4;
 *((int32_t*)buf) = htole32(req->timestamp);   buf += 4;
 *((int32_t*)buf) = htole32(req->offset);      buf += 4;
 *((int32_t*)buf) = htole32(req->size);        buf += 4;
 memcpy(buf, req->filename, 0x0c);
 return(GP_OK);
} /* sx330z_fill_req */

/*convert to correct endianness */
static int
sx330z_fill_ack(uint8_t *buf, struct traveler_ack *ack)
{
 ack->always3 = le32toh(*(int32_t*)buf);   buf += 4;
 ack->timestamp = le32toh(*(int32_t*)buf); buf += 4;
 ack->size = le32toh(*(uint32_t*)buf);     buf += 4;
 ack->dontknow = le32toh(*(int32_t*)buf);  buf += 4;
 return(GP_OK);
} /* sx330z_fill_ack */

/*convert to correct endianness */
static int
sx330z_fill_toc_page(uint8_t *buf, struct traveler_toc_page *toc)
{
 int cnt;
 toc->data0 = le32toh(*(int32_t*)buf);      buf += 4;
 toc->data1 = le32toh(*(int32_t*)buf);      buf += 4;
 toc->always1 = le16toh(*(int16_t*)buf);    buf += 2;
 toc->numEntries = le16toh(*(int16_t*)buf); buf += 2;
 for (cnt = 0;cnt < 25;cnt++)
 {
  memcpy(toc->entries[cnt].name,buf,12);            buf += 12;
  toc->entries[cnt].time = le32toh(*(int32_t*)buf); buf += 4;
  toc->entries[cnt].size = le32toh(*(int32_t*)buf); buf += 4;
 }
 return(GP_OK);
} /* sx330z_fill_toc_page */

/*
 * SX330z initialization
 *  (not really an initialization, but lets check if we have contact )
 */
int 
sx330z_init(Camera *camera, GPContext *context)
{
/* struct traveler_ack ack;*/
 uint8_t trxbuf[0x10];
 int ret;
 ret = gp_port_usb_msg_read(camera->port, USB_REQ_RESERVED, SX330Z_REQUEST_INIT, 0, (char *)trxbuf, 0x10);
 if (ret != 0x10) return(GP_ERROR); /* more specific about error ? */  
 return(GP_OK); 
} /* sx330z_init */


/*
 * Read block described by req 
 */
static int 
sx330z_read_block(Camera *camera, GPContext *context, struct traveler_req *req, uint8_t *buf)
{
 int ret;
 uint8_t trxbuf[0x20];
 /* 1. send request */
 sx330z_fill_req(trxbuf, req);
 ret = gp_port_usb_msg_write(camera->port,
    USB_REQ_RESERVED, req->requesttype, 0, (char *)trxbuf, 0x20);
  if (ret != 0x20) return(GP_ERROR_IO_WRITE);
 /* 2. read data */
 ret = gp_port_read(camera->port, (char *)buf, req->size);
  if (ret != req->size)return(GP_ERROR_IO_READ);
 /* 3. read Ack */
 ret = gp_port_read(camera->port, (char *)trxbuf, 0x10);
  if (ret != 0x10) return(GP_ERROR); 
 /* FIXME : Security check ???*/
 return(GP_OK);
} /* read block */


/*
 * Get TOC size
 */
int 
sx330z_get_toc_num_pages(Camera *camera, GPContext *context, int32_t *pages)
{
 struct traveler_ack ack;
 uint8_t trxbuf[0x10];
 int ret;

 ret=gp_port_usb_msg_read(camera->port,
    USB_REQ_RESERVED,SX330Z_REQUEST_TOC_SIZE, 0, (char *)trxbuf, 0x10);
 if (ret != 0x10) return(GP_ERROR);

 sx330z_fill_ack(trxbuf, &ack); 		/* convert endianness */

 *pages = ack.size / 0x200 + 1;		/* TOC Pages */

 /* bug in camera ??*/
 if (ack.size == 0x200) (*pages)--;
 if ((ack.size > 0x200)&&
    (((ack.size - 0xc) % 0x200) == 0)) (*pages)--;
  
 return(GP_OK);
} /* get toc size */

/*
 * Get TOC 
 * Read a single TOC page 
 * specified by "page"
 */
int 
sx330z_get_toc_page(Camera *camera, GPContext *context, struct traveler_toc_page *TOC, int page)
{
 int ret;
 struct traveler_req req;
 uint8_t tocbuf[0x200];
 
 req.always1 = 1;
 req.requesttype = SX330Z_REQUEST_TOC;	/* 0x03 */ 
 req.offset = 0x200 * page;			/* offset */
 req.timestamp = 0x123;			/* ? */
 req.size = 0x200;			/* 512 Bytes / tocpage*/
 req.data = 0;				/* ? */
 memset(req.filename, 0, 12);		/* ? */
 
 ret=sx330z_read_block(camera,context,&req,tocbuf);
 if (ret<0) return(ret);
 
 sx330z_fill_toc_page(tocbuf,TOC); /* convert */
 
 /*  TOC sanity check   */
 if ((TOC->numEntries < 0) || (TOC->numEntries > 25)) return(GP_ERROR_CORRUPTED_DATA);
 return(GP_OK);
} /* get toc */


/*
 *  get data 
 * could be  Image / Thumbnail 
 */
int 
sx330z_get_data(Camera *camera, GPContext *context, const char *filename,
   		 char **data, unsigned long int *size, int thumbnail)
{
 uint8_t *dptr;
 int pages, cnt, ret;
 struct traveler_req req;
 int found;
 int tocpages, tcnt, ecnt;
 struct traveler_toc_page toc;
 int id;/* progress ? */ 
 pages = 0;
 found = 0;
 memcpy(req.filename, filename, 12);
 
 if (thumbnail == SX_THUMBNAIL) 
 {
  if (camera->pl->usb_product == USB_PRODUCT_MD9700)
    pages = 7; /* first 28k only*/
   else  
    pages = 5; /* first 20k only */
  req.filename[0] = 'T';		/* 'T'humbnail indicator ?*/
  id = gp_context_progress_start(context, 0x1000 * pages, "Thumbnail %.4s _", &filename[4]);
 } else
 {
  /* I don't like this solution ... */
  ret = sx330z_get_toc_num_pages(camera, context, &tocpages);	/* number of toc pages */
  if (ret != GP_OK) return(ret);
  for (tcnt = 0;(tcnt < tocpages) && (!found);tcnt++)
  {
   ret = sx330z_get_toc_page(camera, context, &toc, tcnt);
   for (ecnt = 0;ecnt < toc.numEntries;ecnt++)
   {
    if (strncmp(toc.entries[ecnt].name, filename,8) == 0)
     {
      found = 1;
      *size = toc.entries[ecnt].size;
      break;
     }
   } /* */
  } /* load all toc pages */
  /*return(GP_ERROR);*/
  if (!found) return(GP_ERROR);
  if (((*size % 4096) != 0) || (*size == 0)) return(GP_ERROR);  /* sanity check */
  pages = *size / 0x1000;
  id = gp_context_progress_start(context, *size, "Picture %.4s _", &filename[4]);
 } /* real image */
  
 *size = 4096 * pages; 
 *data = malloc(*size);
 dptr = (uint8_t *)*data;
 /* load all parts */
 for (cnt = 0;cnt < pages;cnt++)
 {
  req.always1 = 1;
  req.requesttype = SX330Z_REQUEST_IMAGE;		/* Imagedata */
  req.offset = cnt * 0x1000;
  req.size = 0x1000;
  req.timestamp = 0x0 + cnt * 0x41;			/* timestamp (doesn't matter)?*/
  req.data = 0;
  gp_context_progress_update(context, id, (cnt + 1) * 0x1000);
  sx330z_read_block(camera,context,&req,dptr);  /* read data */
  dptr += 4096;
 }/* download imageparts */     
 gp_context_progress_stop(context,id);
 return(GP_OK);
} /* sx330z_get_data */



/*
 *  delete file 
 */
int 
sx330z_delete_file(Camera *camera, GPContext *context, const char *filename)
{
 struct traveler_req req;
/* struct traveler_ack ack;*/
 uint8_t trxbuf[0x20];
 int ret, id;
 req.always1 = 1;
 req.requesttype = SX330Z_REQUEST_DELETE;					/* Delete */
 req.offset = 0x0;
 req.size = 0x000;
 req.timestamp = 0x0;							/* timestamp (doesn't matter)?*/
 req.data = 0;
 sprintf(req.filename, "%.8s", filename);
 sprintf(&req.filename[8], "jpg");					/* discard . */
 id = gp_context_progress_start(context, 2, "Deleting %s", filename);	/* start context */
 /* send delete request */
 sx330z_fill_req(trxbuf,&req);
 ret = gp_port_usb_msg_write(camera->port,
	USB_REQ_RESERVED, SX330Z_REQUEST_DELETE, 0, (char *)trxbuf, 0x20);
 if (ret != 0x20) return(GP_ERROR);						/* simple error handling */
 gp_context_progress_update(context, id, 1);				/* update context */
 /* read 16 Byte acknowledge packet */
 ret = gp_port_usb_msg_read(camera->port,
	USB_REQ_RESERVED, SX330Z_REQUEST_DELETE, 0, (char *)trxbuf, 0x10);
 if (ret != 0x10) return(GP_ERROR);
 
 gp_context_progress_stop(context, id);					/* stop context */
 return(GP_OK);
} /* sx330z delete file */


