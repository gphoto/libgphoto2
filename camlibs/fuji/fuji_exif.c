/*
  Thumbnail extraction from EXIF file for GPHOTO package
  Copyright (C) 1998 Matthew G. Martin

  This routine works for DS-7 thumbnail files.  Don't know about any others.
    
    Most of this code was taken from
    GDS7 v0.1 interactive digital image transfer software for DS-7 camera
    Copyright (C) 1998 Matthew G. Martin

    Which was directly derived from get_ds7, a Perl Language library
    Copyright (C) 1997 Mamoru Ohno


    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/times.h>
#include <errno.h>
#include <fcntl.h>
#include <gphoto2.h>
#include "fuji.h"
#include "../../libgphoto2/exif.h"

/***********************************************************************
   EXIF handling functions
 **********************************************************************/

#define APPEND_DATA(src,cnt)   memcpy(curptr,src,cnt);curptr+=cnt;


/* 
   New conversion function, all in memory 
*/
int fuji_exif_convert(exifparser *exifdat,CameraFile *cfile){
  char *tmpstr[32];
  unsigned char *imagedata,*exifimg,*newimg,*curptr;
  unsigned int offset,entry,dataspace;
    long dataptr,dsize,tag,datvec,size,tmp;
  int i,j;

  if (exif_parse_data(exifdat)<0) return(GP_ERROR);

  /* Check out the exif data */
  if (stat_exif(exifdat)) return(GP_ERROR); /* Couldn't parse exif data,quit */

  /* Assume thumb will be no bigger than exif data */
  newimg=malloc(exifdat->exiflen);

  if (newimg==NULL){
    DBG("fuji_exif_convert: could not malloc");
    return(GP_ERROR);
  };

  /* Copy header*/
  curptr=newimg;
  APPEND_DATA(exifdat->data,8);

  /* Get data offset, don't use this for anything... */
  offset=exif_get_lilend(exifdat->data+4,4);
  DBG2("Offset is %d bytes",offset);

  /*gpe_dump_exif(exifdat);*/

  /* Skip to TIFF image data */
  if(exifdat->ifdcnt<2) {
    DBG("Too few ifds, doesn't look right. Giving up");
    return(GP_ERROR); /* Things don't look right...*/
  };

  DBG2("New Offset is %d bytes\n",offset);

  /* Jump to thumbnail image data (2nd set) */
  exifimg=exifdat->ifds[1];

  /* Copy number of entries */
  APPEND_DATA(exifimg,2);

  entry=exif_get_lilend(exifimg,2);

  DBG2("Entry is %d \n",entry);

  /* See if thumb is a JPEG */
  tmp=gpe_getintval(exifimg,EXIF_JPEGInterchangeFormat); /*imagedata start*/

  if (tmp>0) { /* jpeg image */

    DBG("Found jpeg thumb data");

    dsize=gpe_getintval(exifimg,EXIF_JPEGInterchangeFormatLength);

    if (dsize==-1){
      DBG("No Jpeg size tag for thumbnail, skipping");
      return(GP_ERROR);
    };
    imagedata=exifdat->data+tmp;
    memcpy(newimg,imagedata,dsize);
    gp_file_set_data_and_size(cfile,newimg,dsize);
    //cfile->data=newimg;
    //cfile->size=dsize;
    //strcpy(cfile->mime_type,"image/jpeg");
    return(GP_OK);
  };

  /* Try a TIFF */
  //strcpy(cfile->mime_type,"image/tiff");
  tmp=gpe_getintval(exifimg,EXIF_StripOffsets); /*imagedata start*/
  if (tmp==-1) {
    DBG("fuji_exif: Tiff or jpeg data not found, skipping");
    return(GP_ERROR);
  };

  /* imagedata starts at the offset point */
  imagedata=exifdat->data+tmp;

 /* get the imagedata size */
  dataptr=gpe_getintval(exifimg,EXIF_StripByteCounts);
  if (dataptr==-1) {
    DBG("Split two");
    return(GP_OK);
  };

  DBG2("Imagedata size is %ld bytes",dataptr);

  dataspace=12*entry+14; /* Storage area ? */

  for (i=0;i<entry;i++){

    /* Read off individual tags for the thumb */
    dsize=gpe_datsize(exifimg,i);
    tag=gpe_tagnum(exifimg,i);

    DBG4("Datsize %d (tag=%ld) is %ld",i,tag,dsize);

    if (tag==EXIF_StripOffsets) {
      gpe_setval(exifimg,i,dataspace); /* set to end of directory */
    }
    else {
      if (dsize>5){
	/* Copy the referenced data*/
	datvec=gpe_theval(exifimg,i);
	gpe_setval(exifimg,i,dataptr+dataspace); /* Relocate the tag */
	/* move the data */
	for (j=0;j<dsize;j++) 
	  imagedata[dataptr++]=exifdat->data[datvec+j];
      };
    };
    /* Copy the tag over (unmodified) */
    APPEND_DATA(exifimg+12*i+2,12);
  };

  APPEND_DATA(exifimg+12*entry+10,4);/* Write 4 zero bytes */

  memcpy(curptr,imagedata,dataptr);/* write image data after the tags */
  curptr+=dataptr;

  gp_file_set_data_and_size(cfile,newimg,curptr-newimg);
  //cfile->data=newimg;
  //cfile->size=curptr-newimg;//dataptr;
  return GP_OK;
};
