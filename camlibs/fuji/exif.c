#include "exif.h"
/*
  EXIF file library for GPHOTO package
  Copyright (C) 1999 Matthew G. Martin

  This routine works for DS-7 thumbnail files.  Don't know about any others.

  Tag descriptions and an ever-increasing number of structural details
  Taken from "exifdump.py" by Thierry Bousch <bousch@topo.math.u-psud.fr>

  Thanks to Paul Wood <pwood@cs.bris.ac.uk> for sub-ifd parsing.

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

/* 
   Watch out, this code is very formative...
              nowhere near everything is parsed correctly
*/

int fuji_exif_debug=0;

extern unsigned char *fuji_exif_mem_convert(exifparser *exifdat);

static struct tagarray{
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

/* Size of various tags */
static int exif_sizetab[13]={
  0,1,1,2,4,8,1,1,2,4,8,4,8
};

/* 
   Return tag number of directory entry at tagind
 */
int tagnum(unsigned char *data,int tagind){
 return(lilend(data+tagind*12+2,2));
};

/* Convert Intel-Endian number, this should be generalized for both types*/
long lilend(unsigned char *data, int size){
  long total;

  total=0;
  for (--size;size>=0;size--){
    total<<=8;
    total+=data[size];
  };

  return(total);
};

/* Convert signed Intel-Endian number, 
   this should be generalized for both types*/
long slilend(unsigned char *data, int size){
  long total;
  long unsigned mask=1<<(size*8-1);

  total=0;
  for (--size;size>=0;size--){
    total<<=8;
    total+=data[size];
  };

  if (total&mask) total-=mask;

  return(total);
};

/*
  Point tagnum and data to the tag name and value strings of
  IFD ifdnum, TAG tagnum1
*/
int togphotostr(exifparser *exifdata,int ifdnum,int tagnum, char** tagnam,char** data){
  unsigned char* thistag;
  unsigned char* thedata;
  int i,j,tag,numtags,tagtype,count,typelen,tmp1,tmp2;
  char tmpstr[256];

  tmpstr[0]=0; /* clear the string, just to be sure */
  *tagnam=*data=NULL;
  thistag=exifdata->ifds[ifdnum]+tagnum*12+2;
  tag=lilend(thistag,2);          /* tag identifier */
  *tagnam=strdup(tagname(tag));
  tagtype=lilend(thistag+2,2);    /* tag type */
  count = lilend(thistag+4, 4);   /* how many */
  typelen=exif_sizetab[tagtype];/* length of this type */
  
  thedata=thistag+8;
  if (count*typelen > 4)   /* find it in a data block elsewhere */
    thedata = exifdata->data+lilend(thedata, 4);
  
  if (tagtype==2) {
    /* Do an ASCII tag */
    strncpy(tmpstr,thedata,count);
    tmpstr[count]='\0';
  }
  else for (j=0;j<count;j++) {

    if ((tagtype==5)||(tagtype==10)) {/* Fractional */
      tmp1=slilend(thedata+j*typelen,4);
      tmp2=slilend(thedata+4+j*typelen,4);
      sprintf(tmpstr+strlen(tmpstr),"%.3g ",(tmp2==0)?0:(1.0*tmp1/tmp2));
    }
    else
      sprintf(tmpstr+strlen(tmpstr),"%d ",lilend(thedata+j*typelen,typelen));
  };
  *data=strdup(tmpstr);
  
  if (fuji_exif_debug) printf("Got %s = %s\n",*tagnam,*data);

  return(0);
};

/* 
   Pull value of tag tagnum from data
      only works for 4 byte values (for now)
*/
int getintval(unsigned char *data, int tagnum){
  int numtags,i,tag,tagtype;

  numtags=lilend(data,2);

  if (fuji_exif_debug)  printf("getval:%d tags\n",numtags);

 i=-1;
 do{
   i++;
   tag=lilend(data+i*12+2,2);
 }while((i<numtags)&&(tag!=tagnum));

 if(tag!=tagnum) {
   if (fuji_exif_debug) fprintf(stderr,"Tag %d not found\n",tagnum);
   return(-1);
 };

 tagtype=lilend(data+i*12+4,2);    /* tag type */

 return(lilend(data+i*12+10,exif_sizetab[tagtype-1]));
};

/*
  Add all tag names and values in an ifd to an array of strings for gphoto
 */
int exif_add_all(exifparser *exifdata,int ifdnum,char **datastrings){
  int i;
  for (i=0;i<exifdata->ifdtags[ifdnum];i++){
    togphotostr(exifdata,ifdnum,i,datastrings+i*2,datastrings+i*2+1);
    /*    printf("%s = %s\n",datastrings[i*2],datastrings[i*2+1]);*/
  };
};


char *tagname(int tagnum){
  int i=-1;
  do{
    i++;
    if (tagnames[i].num==tagnum) return (tagnames[i].desc);
  }while(tagnames[i].num);
  return("Unknown");
};

int dump_ifd(int ifdnum,exifparser *exifdata,char **allpars){
  int i,j,tag,numtags,tagtype,count,typelen, value,tmp1,tmp2;
  char tmpstr[256];
  unsigned char* thistag;
  unsigned char* thedata;
  unsigned char* thisisd;
  char tmpdat[200];
  char** name;
  
  
    thisisd=exifdata->ifds[ifdnum];
    numtags=lilend(thisisd,2);
    printf("has %d tags ----------------------\n",numtags);
    for (i=0;i<numtags;i++){

      thistag=thisisd+i*12+2;   /* pointer to data for this tag */

      tag=lilend(thistag,2);          /* tag identifier */
      tagtype=lilend(thistag+2,2);    /* tag type */
      count = lilend(thistag+4, 4);   /* how many */
      typelen=exif_sizetab[tagtype];/* length of this type */

      if (fuji_exif_debug) printf("(%dX) ",count);

      thedata=thistag+8;
      if (count*typelen > 4)   /* find it in a data block elsewhere */
	thedata = exifdata->data+lilend(thistag+8, 4);

      printf("Tag 0x%X %s = ",tag,tagname(tag));

      if (tagtype==2) {
	/* Do an ASCII tag */
	  strncpy(tmpstr,thedata,count+1);
	  tmpstr[count+1]='\0';
	  printf("'%s'",tmpstr);
	} 
      else for (j=0;j<count;j++) {

	if ((tagtype==5)||(tagtype==10)) {/* Fractional */
	  tmp1=slilend(thedata+j*typelen,4);
	  tmp2=slilend(thedata+4+j*typelen,4);
	      printf("%d/%d=%.3g ",tmp1,tmp2,(tmp2==0)?0:(1.0*tmp1/tmp2));
	}
	else {
	  value =lilend(thedata+j*typelen,typelen);
	  printf("%d ",value);
	};
	
      };

      printf("\n"); /*end of this tag */

/* Print SubIfd tags */
      if ( tag == 0x8769 ) {
         printf("Exif SubIFD at offset %d\n", value );
         exifdata->ifds[exifdata->ifdcnt]     = exifdata->data+value;  
         exifdata->ifds[exifdata->ifdcnt];
         exifdata->ifdtags[exifdata->ifdcnt]=lilend(exifdata->data+value,2);
         exifdata->ifdcnt++;

}
/***/

    };
};

/* Count the number of IFD's and log their pointers */
int stat_exif(exifparser *exifdata){
  long offset=0;

  exifdata->endian=0;
  if (exifdata->data[0]!='I') {
    exifdata->endian=1;
    printf("%c,Intel-Endian format only supported right now!\n",
	   exifdata->data[0]);
    return(-1);
  };

  offset=lilend(exifdata->data+4,4); /*Get offset of first IFD */

  exifdata->ifdcnt=-1;

  /*Step through each IFD (looks like there should be 2 or 3)*/
  do{
    exifdata->ifdcnt++;
    exifdata->ifds[exifdata->ifdcnt]     = exifdata->data+offset;
    exifdata->ifdtags[exifdata->ifdcnt]  = lilend(exifdata->data+offset,2);

  }while(offset=next_ifd(exifdata->data,offset));
  exifdata->ifdcnt++;

  exifdata->preparsed=1;
  return(0);
};

/* Walk through EXIF structure, printing values */
int dump_exif(exifparser *exifdata){ 
  int i,tag;
  unsigned char* thisifd;

  if (!exifdata->preparsed) 
    if (stat_exif(exifdata)) return(-1);

  for (i=0;i<exifdata->ifdcnt;i++){
    switch (i) {
       case 0:      
          printf("IFD %d, %s ",i,"Main Image");
          break;
       case 1:
          printf("IFD %d, %s ",i,"Thumbnail");
         break;
       case 2:
          printf("IFD %d, %s ",i,"Sub IFD");
          break;
   
   }       
       

    dump_ifd(i,exifdata,NULL);
  };
};

/* 
   Return data size of directory entry at tagind
 */
int datsize(unsigned char *data,int tagind){
  return(exif_sizetab[lilend(data+tagind*12+4,2)]*lilend(data+tagind*12+6,4));
};

/* 
   Return "value" of directory entry at tagind
 */
int theval(unsigned char *data,int tagind){
 return(lilend(data+tagind*12+10,4));
};

/* 
   Set the "value" of directory entry at tagind
*/

void setval(unsigned char *data,int tagind,long newval){
  int i;
  for (i=0;i<4;i++) data[tagind*12+10+i]=0xff&(newval>>(i*8));
  if (theval(data,tagind)!=newval) 
    printf("Setptr: error %d inst %ld\n",theval(data,tagind),newval);
};

/* directly from Ohno's perl script (IFD=Image Format Descriptor ?)*/

long next_ifd(unsigned char *exif,int num){
  int offset=(exif[num]+(exif[num+1]<<8))*12+num+2;

  if (fuji_exif_debug) printf("next_ifd,offset=%d\n",offset);

  return(lilend(exif+offset,4));
};

/*
Parse an exif header, return size of the rest of the file
*/
unsigned int exif_header_parse(exifparser *exifdat){
  if (strncmp("Exif",exifdat->header+6,4)){
    fprintf(stderr,"Not exif data\n");
    return(-1);
  };
  exifdat->exiflen=exifdat->header[5]+(exifdat->header[4]<<8)-8;
  if (fuji_exif_debug) fprintf(stderr,"Exif length is %ld\n",exifdat->exiflen);
  return exifdat->exiflen;
};
