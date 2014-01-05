/** \file
  \brief EXIF file library for GPHOTO package

  \author Copyright 1999 Matthew G. Martin

  Generic gphoto implementation and extension by Edouard Lafargue.

  Tag descriptions and an ever-increasing number of structural details
  Taken from "exifdump.py" by Thierry Bousch <bousch@topo.math.u-psud.fr>

  Thanks to Paul Wood <pwood@cs.bris.ac.uk> for sub-ifd parsing.

  \par License
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
  
  \par
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
   
  \par
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the 
  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA  02110-1301  USA
*/
#include "exif.h"


/*
 *  Conventions:
 *
 * "tag number" (tagnum) : ID number of a tag (see list just below).
 * "tag index"  (tagind) : index number of a tag inside an EXIF structure
 *                         (for example, tag number one if the first tag, etc..)
 * "tag name" (tagname)  : name of the tag as defined in the EXIF standard.
 */

static int exif_debug=0;

/* Size of various tags */
static const int exif_sizetab[13]={
  0,1,1,2,4,8,1,1,2,4,8,4,8
};

static const struct tagarray{
  int num;
  char* desc;
} tagnames[]=
{
{0xFE, 	"NewSubFileType"},
{0x100,	"ImageWidth"},
{0x101,	"ImageLength"},
{0x102,	"BitsPerSample"},
{0x103,	"Compression"},
{0x106,	"PhotometricInterpretation"},
{0x10A,	"FillOrder"},
{0x10D,	"DocumentName"},
{0x10E,	"ImageDescription"},
{0x10F,	"Make"},
{0x110,	"Model"},
{0x111,	"StripOffsets"},
{0x112,	"Orientation"},
{0x115,	"SamplesPerPixel"},
{0x116,	"RowsPerStrip"},
{0x117,	"StripByteCounts"},
{0x11A,	"XResolution"},
{0x11B,	"YResolution"},
{0x11C,	"PlanarConfiguration"},
{0x128,	"ResolutionUnit"},
{0x12D,	"TransferFunction"},
{0x131,	"Software"},
{0x132,	"DateTime"},
{0x13B,	"Artist"},
{0x13E,	"WhitePoint"},
{0x13F,	"PrimaryChromaticities"},
{0x156,	"TransferRange"},
{0x200,	"JPEGProc"},
{0x201,	"JPEGInterchangeFormat"},
{0x202,	"JPEGInterchangeFormatLength"},
{0x211,	"YCbCrCoefficients"},
{0x212,	"YCbCrSubSampling"},
{0x213,	"YCbCrPositioning"},
{0x214,	"ReferenceBlackWhite"},
{0x828D,	"CFARepeatPatternDim"},
{0x828E,	"CFAPattern"},
{0x828F,	"BatteryLevel"},
{0x8298,	"Copyright"},
{0x829A,	"ExposureTime"},
{0x829D,	"FNumber"},
{0x83BB,	"IPTC/NAA"},
{0x8769,	"ExifOffset"},
{0x8773,	"InterColorProfile"},
{0x8822,	"ExposureProgram"},
{0x8824,	"SpectralSensitivity"},
{0x8825,	"GPSInfo"},
{0x8827,	"ISOSpeedRatings"},
{0x8828,	"OECF"},
{0x9000,	"ExifVersion"},
{0x9003,	"DateTimeOriginal"},
{0x9004,	"DateTimeDigitized"},
{0x9101,	"ComponentsConfiguration"},
{0x9102,	"CompressedBitsPerPixel"},
{0x9201,	"ShutterSpeedValue"},
{0x9202,	"ApertureValue"},
{0x9203,	"BrightnessValue"},
{0x9204,	"ExposureBiasValue"},
{0x9205,	"MaxApertureValue"},
{0x9206,	"SubjectDistance"},
{0x9207,	"MeteringMode"},
{0x9208,	"LightSource"},
{0x9209,	"Flash"},
{0x920A,	"FocalLength"},
{0x927C,	"MakerNote"},
{0x9286,	"UserComment"},
{0x9290,	"SubSecTime"},
{0x9291,	"SubSecTimeOriginal"},
{0x9292,	"SubSecTimeDigitized"},
{0xA000,	"FlashPixVersion"},
{0xA001,	"ColorSpace"},
{0xA002,	"ExifImageWidth"},
{0xA003,	"ExifImageLength"},
{0xA005,	"InteroperabilityOffset"},
{0xA20B,	"FlashEnergy"},			
{0xA20C,	"SpatialFrequencyResponse"},	
{0xA20E,	"FocalPlaneXResolution"},	
{0xA20F,	"FocalPlaneYResolution"},	
{0xA210,	"FocalPlaneResolutionUnit"},	
{0xA214,	"SubjectLocation"},		
{0xA215,	"ExposureIndex"},		
{0xA217,	"SensingMethod"},		
{0xA300,	"FileSource"},
{0xA301,	"Scenetype"},
{0,"end"}
};

/*
 * Foward declarations. move to exif.h if you want to export.
 */
static int gpi_getvalue(unsigned char *data,int tagind);
static int gpi_datsize(unsigned char *data,int tagind);
static int gpi_tagnum( unsigned char *data,int tagind);
static int gpi_getintval(unsigned char *data, int tagnum);

static long gpi_exif_get_lilend(unsigned char *data, int size);
static int gpe_theval(unsigned char *data,int tagind);
static void gpi_setval(unsigned char *data,int tagind,long newval);

static long exif_next_ifd(unsigned char *exif,int num);
static int gpi_exif_get_comment(exifparser *exifdat, char **comment);
static int gpi_exif_get_field( int tag_number, int ifd, exifparser *exifdata, ExifData *tag_data);
/*
 *  Utility functions: get fields in little-endian format,
 * initialize data structures, etc.
 */

/*
 * Convert to Intel-Endian number.
 *
 * data : pointer to the data to convert
 * size : size of the data to convert
 *
 * Returns : the value
 */
static long gpi_exif_get_lilend(unsigned char *data, int size){
  long total;

  total=0;
  for (--size;size>=0;size--){
    total<<=8;
    total+=(unsigned char)(data[size]);
  }

  return(total);
}

/*
 *  Return "value" of directory entry at tagind
 */
static int gpe_theval(unsigned char *data,int tagind){
 return(gpi_exif_get_lilend(data+tagind*12+10,4));
}


/*
 *  Set the "value" of directory entry at tagind.
 */
static void gpi_setval(unsigned char *data,int tagind,long newval){
  int i;
  for (i=0;i<4;i++) data[tagind*12+10+i]=0xff&(newval>>(i*8));
  if (gpi_getvalue(data,tagind)!=newval) 
    printf("Setptr: error %d inst %ld\n",gpe_theval(data,tagind),newval);
}


/*
 * Returns the offset of the next IFD entry after
 * offset "num".
 *
 * exif : exif data
 * num  : start offset
 *
 * returns: offset of next IFD.
 */
static long exif_next_ifd(unsigned char *exif,int num){

  int offset=(exif[num]+(exif[num+1]<<8))*12+num+2;
  if (exif_debug) printf("next_ifd,offset=%d\n",offset);
  return(gpi_exif_get_lilend(exif+offset,4));

}


/*
 * Parse an exif data structure and initialise the "exifparser".
 *
 * exifdat : the exif data
 *
 * returns : size of the rest of the file.
 *           -1 if failed.
 */
static int exif_parse_data(exifparser *exifdat) {
  long offset=0;
  ExifData tagdat;

  exif_debug=1;
  /* Check that it's not already preparsed */
  /* Doesn't work if we don't initialize this first... */
  /*  if (exifdat->preparsed) return 0; */
  if (exif_debug) printf("Parsing exif structure\n");

  /* First, verify that we really have an EXIF file */
  /* Note: maybe we could also generalize to TIFF... */
  if ((memcmp("Exif",exifdat->header+6,4)) ||
      memcmp("\377\330\377\341",exifdat->header,4)){
    fprintf(stderr,"Not exif data\n");
    return(-1);
  }

  exifdat->exiflen = exifdat->header[5] + (exifdat->header[4]<<8) - 8;
  if (exif_debug) printf("Exif length is %i\n",exifdat->exiflen);

  /* Count the number of IFD's and register their pointers. */
  exifdat->endian=0;
  if (exifdat->data[0]!='I') {
    exifdat->endian=1;
    printf("%c,Intel-Endian format only supported right now!\n",
	   exifdat->data[0]);
    return(-1);
  }

  offset=gpi_exif_get_lilend(exifdat->data+4,4); /*Get offset of first IFD */
  exifdat->ifdcnt=-1;

  /*Step through each IFD */
  do {
    exifdat->ifdcnt++;
    exifdat->ifds[exifdat->ifdcnt]     = exifdat->data+offset;
    exifdat->ifdtags[exifdat->ifdcnt]  = gpi_exif_get_lilend(exifdat->data+offset,2);
  } while ((offset=exif_next_ifd(exifdat->data,offset)));

  exifdat->ifdcnt++;

  /* Look for the EXIF IFD */
  exifdat->preparsed=1; /* Indicate that our exifparser is initialized */
  /* Has to be done before calling gpi_exif_get_field */
  if ( ! gpi_exif_get_field( EXIF_ExifOffset, 0, exifdat, &tagdat)) {
    printf("No EXIF information stored in this image\n");
  } else {
    if (exif_debug) printf("Offset to the Exif subIFD is %d\n",tagdat.intval);
    exifdat->ifds[exifdat->ifdcnt] = exifdat->data + tagdat.intval;
    exifdat->ifdtags[exifdat->ifdcnt]  = gpi_exif_get_lilend(exifdat->data+tagdat.intval,2);
    exifdat->ifdcnt++;
  }

  if (exif_debug) printf("Finished parsing exif structure\n");
  return exifdat->exiflen;
}

/*
 * tag-level functions: get/set tags in the EXIF structure
 */

/**
 * Returns the value of a field, identified by its tag and the IFD.
 *
 * Specifying an IFD of '-1' will return the first occurence of
 * the tag in the first IFD where it finds it.
 *
 * tag_number: tag that identifies the field
 * ifd       : image file directory where to look for that tag
 * exifdata  : pointer to the EXIF data
 * tag_data  : filled by this function, contains the actual data
 *
 * returns   : 1 on success, 0 on failure
 */
static int gpi_exif_get_field( int tag_number, int ifd, exifparser *exifdata, ExifData *tag_data) {
  int numtags,i, tag;
  unsigned char *ifdp, *data;

  /* Sanity check first: */
  exif_debug=1;
  if ( !exifdata->preparsed ) {
    if(exif_parse_data(exifdata) < 0)
      return 0;
  }

  if (ifd == -1) {
    for (i = 0; i < exifdata->ifdcnt+1; i++){
      if (exif_debug) printf("Searching in IFD %d\n",i);
      if ( gpi_exif_get_field(tag_number, i, exifdata, tag_data) == 1)
	return 1;
    }
    return 0; /* We did not find it. */
  } else {

    /* Find tag in a specific IFD */
    ifdp = exifdata->ifds[ifd];
    numtags=gpi_exif_get_lilend(ifdp,2); /* How many tags in the IFD */
    if (exif_debug)  printf("gpi_exif_get_field: %d tags in ifd %d\n",numtags, ifd);
    i=-1;
    do {
      i++;
      tag = gpi_exif_get_lilend(ifdp+i*12+2, 2); /* Get the tag ID */
      /* if (exif_debug) fprintf(stderr,"Found tag %d \n",tag); */
    } while ((i < numtags) && (tag != tag_number));
    
    if(tag != tag_number) {
      if (exif_debug) fprintf(stderr,"Tag %d not found\n",tag_number);
      return 0;
    }
    
    ifdp = ifdp+i*12+2; /* Place pointer at TAG type position */
    tag_data->tag = tag;
    tag_data->type = gpi_exif_get_lilend(ifdp+2,2);    /* tag type */
    tag_data->size = exif_sizetab[tag_data->type]*gpi_exif_get_lilend(ifdp+4,4);
    if (exif_debug) printf("(%d bytes) ",tag_data->size);

    ifdp += 8; /* place pointer at beginning of the contents of the field */

    /* Data types smaller than 4 bytes are stored directly in
       the IFD field value. Otherwise, that value is an offset
       to the real data */
    if (tag_data->size > 4) {
	ifdp = exifdata->data + gpi_exif_get_lilend(ifdp,4);
    }

    /* Last operation, get the data itself: */
    /* Allocate memory, and copy the data */
      data=malloc(tag_data->size);
      if (data==NULL){
	fprintf(stderr,"gpi_exif_get_field: could not malloc\n");
	return 0;
      }

    if (tag_data->type == EXIF_ASCII) {
	memcpy(data,ifdp,tag_data->size);  /* Normally, the exif data includes a terminating 0 */
	tag_data->data = data;
	if(exif_debug) printf("\"%s\"",data);
    } else {
      for (i = 0; i < tag_data->size; i += exif_sizetab[tag_data->type]) {
	/*if (exif_debug) fprintf(stderr,"."); */
	if ( tag_data->type % 5 ) {
	  memcpy(data+i,ifdp+i,exif_sizetab[tag_data->type]);	  
	} else { /* Fraction */
	    tag_data->num =gpi_exif_get_lilend(ifdp+i,4);
	    tag_data->den =gpi_exif_get_lilend(ifdp+i+4,4);
	  
	  if(exif_debug)  printf("%d/%d=%.3g ",tag_data->num,
		                  tag_data->den,
				 (tag_data->den==0)?0:(1.0*tag_data->num/tag_data->den));
	}	
      }
      /* If the value can be put into an int, save the trouble and store */
      /* it into "intval" right away... */
      if(tag_data->type != EXIF_ASCII && tag_data->type != EXIF_RATIONAL &&
	 tag_data->type != EXIF_UNDEFINED && tag_data->type != EXIF_SRATIONAL) {
	tag_data->intval = gpi_exif_get_lilend(data,exif_sizetab[tag_data->type]);
	if(exif_debug) printf("'%d'",tag_data->intval);
      }
      tag_data->data = data; /* Save the data field */
    }
    if (exif_debug) printf("\n"); /*end of this tag */
    return 1;
  }
}

/*
 * Higher-level functions: get/set logical entities, such as
 * comments, thumbnail, etc.
 */

/*
 * Gets the comment field if it exists.
 */
static int gpi_exif_get_comment(exifparser *exifdat, char **comment) {
  ExifData tagdat;

  if (exif_parse_data(exifdat)<0) return 0;

  /* User Comment is in IFD number 2 */
  if ( ! gpi_exif_get_field( EXIF_UserComment, 2, exifdat, &tagdat)) {
    if (exif_debug) printf("No comment field in this image\n");
    return 0;
  } else {
    *comment = malloc (tagdat.size+1);
    memcpy(*comment,tagdat.data,tagdat.size);
    return tagdat.size;
  }  
}

/*
 * Gets the thumbnail of an EXIF image.
 * The thumbnail size is provided
 *
 * The returned thumbnail might be in JPEG or TIFF format.
 */
unsigned char *gpi_exif_get_thumbnail_and_size(exifparser *exifdat, long *size) {
  unsigned char *imagedata,*exifimg,*newimg,*curptr;
  unsigned int entry;
  long dataptr,dsize,tag,datvec,tmp;
  unsigned int i,j;

  exif_debug=1;
  if (exif_parse_data(exifdat)<0) return(NULL);

  *size = 0;
  newimg=malloc(exifdat->exiflen);
  if (newimg==NULL){
    fprintf(stderr,"gpi_exif_get_thumbnail: could not malloc\n");
    return(NULL);
  }

  /* Copy header*/
  memcpy(newimg,exifdat->data,8);
  curptr=newimg+8;
  *size += 8;

  if (exif_debug) {    
      ExifData owner;
      char *comment=NULL;
      printf("Decoding EXIF fields in thumbnail\n");
      gpi_exif_get_field( EXIF_Model, -1, exifdat, &owner);
      printf("Camera model: %s\n",owner.data);
      printf("Comment for this picture (%d chars)",gpi_exif_get_comment( exifdat, &comment));
      if (comment) { 
          printf(" -> %s\n",comment);
          free(comment);
      }
      gpi_exif_get_field( EXIF_SubjectDistance, 2, exifdat, &owner);
      /*      dump_exif(exifdat);       */
  }

  /* Skip to Thumbnail image data */
  if(exifdat->ifdcnt<2) {
    if (exif_debug) {
      fprintf(stderr,"Too few ifds, doesn't look right. Giving up\n");
    }
    *size = 0;
    free(newimg);
    return(NULL); /* Things don't look right...*/
  }

  /* Jump to thumbnail image data */
  exifimg=exifdat->ifds[1];

  /* Copy number of entries */
  memcpy(curptr,exifimg,2);
  curptr+=2;
  *size += 2;

  entry=gpi_exif_get_lilend(exifimg,2);

  if (exif_debug) printf("Entry is %d \n",entry);

  /* See if thumb is a JPEG */
  tmp=gpi_getintval(exifimg,EXIF_JPEGInterchangeFormat); /*imagedata start*/
  if (tmp>0) { /* jpeg image */
    if (exif_debug) fprintf(stderr,"Found jpeg thumb data\n");
    dsize=gpi_getintval(exifimg,EXIF_JPEGInterchangeFormatLength);
    if (dsize==-1){
      fprintf(stderr,"No Jpeg size tag for thumbnail, skipping\n");
      *size = 0;
      free(newimg);
      return(NULL);
    }
    imagedata=exifdat->data+tmp;
    memcpy(newimg,imagedata,dsize);
    *size += dsize;
    return(newimg);
  }

  /* Try a TIFF */
  tmp=gpi_getintval(exifimg,EXIF_StripOffsets); /*imagedata start*/
  if (tmp==-1) {
    fprintf(stderr,"gpe_get_thumbnail: Tiff or jpeg data not found, skipping\n");
    *size = 0;
    free(newimg);
    return(NULL);
  }
  imagedata=exifdat->data+tmp;

  dataptr=gpi_getintval(exifimg,EXIF_StripByteCounts);        /* imagedata size */
  if (dataptr==-1) {
    printf("Split two\n");
    *size = 0;
    free(newimg);
    return(NULL);
  }

  if (exif_debug) printf("Imagedata size is %ld bytes\n",dataptr);

  for (i=0;i<entry;i++){
    dsize=gpi_datsize(exifimg,i);
    tag=gpi_tagnum(exifimg,i);

    /*
      if (exif_debug) printf("Datsize %d (tag=%ld) is %ld\n",i,tag,dsize);
    */

    if (tag==EXIF_StripOffsets) {
      gpi_setval(exifimg,i,12*entry+14); /* set to end of directory */
      memcpy(curptr,exifimg+12*i+2,12);
      curptr+=12;
      *size += 12;
    }
    else {
      if (dsize<5){
	/* Just copy the field if small */
        memcpy(curptr,exifimg+12*i+2,12);
	curptr+=12;
	*size += 12;
      }
      else {
	datvec=gpi_getvalue(exifimg,i);
	gpi_setval(exifimg,i,dataptr+12*entry+14);
	for (j=0;j<dsize;j++) imagedata[dataptr++]=exifdat->data[datvec+j];
        memcpy(curptr,exifimg+12*i+2,12);
	curptr+=12;
	*size += 12;
      }
    }
  }
  memcpy(curptr,exifimg+12*entry+10,4); /* Write 4 zero bytes */
  curptr+=4;
  memcpy(curptr,imagedata,dataptr);/* ? */
  curptr+=dataptr;
  *size += 4+dataptr;
  return newimg;
}

/**
 * Return tag number of directory entry at tag index
 *
 * data   : pointer to EXIF data, aligned to the beginning of an IFD
 * tagind : index of the tag within the IFD
 *
 * returns: tag ID number
 */
static int gpi_tagnum( unsigned char *data,int tagind){
 return(gpi_exif_get_lilend(data+tagind*12+2,2));
}

/*
 * Get the value of a tag as an integer.
 * only works for 4 byte values.
 *
 * data : exif data, aligned to the beginning of an IFD.
 * tagnum : tag number.
 *
 * returns: -1 if the tag is not found.
 */
static int gpi_getintval(unsigned char *data, int tagnum) {
  int numtags,i,tag,tagtype;

  numtags=gpi_exif_get_lilend(data,2);

  if (exif_debug)  printf("getval:%d tags\n",numtags);

 i=-1;
 do {
   i++;
   tag=gpi_exif_get_lilend(data+i*12+2,2);
 } while ((i<numtags) && (tag!=tagnum));

 if(tag!=tagnum) {
   if (exif_debug) fprintf(stderr,"Tag %d not found\n",tagnum);
   return(-1);
 }

 tagtype=gpi_exif_get_lilend(data+i*12+4,2);    /* tag type */

 return(gpi_exif_get_lilend(data+i*12+10,exif_sizetab[tagtype-1]));
}

/**
 *  Return "value" of directory entry at tagind
 *
 * data  : exif data
 * tagind: tag index
 */
static int gpi_getvalue(unsigned char *data,int tagind){
 return(gpi_exif_get_lilend(data+tagind*12+10,4));
}


/**
 * Add all tag names and values in an ifd to an array of strings for gphoto
 */
#if 0
int gpe_exif_add_all(exifparser *exifdata,int ifdnum,char **datastrings){
  int i;
  for (i=0;i<exifdata->ifdtags[ifdnum];i++){
    togphotostr(exifdata,ifdnum,i,datastrings+i*2,datastrings+i*2+1);
    /*    printf("%s = %s\n",datastrings[i*2],datastrings[i*2+1]);*/
  }
}
#endif


/**
 * Return data size of directory entry at tagind
 */
static int gpi_datsize(unsigned char *data,int tagind){
  return(exif_sizetab[gpi_exif_get_lilend(data+tagind*12+4,2)]*gpi_exif_get_lilend(data+tagind*12+6,4));
}

int gpi_exif_stat(exifparser *exifdata) {
  long offset=0;

  exifdata->endian=0;
  if (exifdata->data[0]!='I') {
    exifdata->endian=1;
    printf("%c,Intel-Endian format only supported right now!\n",
           exifdata->data[0]);
    return(-1);
  }

  offset=gpi_exif_get_lilend(exifdata->data+4,4); /*Get offset of first IFD*/

  exifdata->ifdcnt=-1;

  /*Step through each IFD (looks like there should be 2 or 3)*/
  do{
    exifdata->ifdcnt++;
    exifdata->ifds[exifdata->ifdcnt]     = exifdata->data+offset;
    exifdata->ifdtags[exifdata->ifdcnt]  = 
    gpi_exif_get_lilend(exifdata->data+offset,2);

  }while((offset=exif_next_ifd(exifdata->data,offset)));
  exifdata->ifdcnt++;

  exifdata->preparsed=1;
  return(0);
}
