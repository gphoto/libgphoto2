#ifndef _exif_
#define _exif_ 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/times.h>
#include <fcntl.h>
#include "exif_tags.h"

typedef struct exif_parser{
  char *header,*data,*ifds[10];
  int ifdtags[10];  /* How many tags in each ifd */
  int ifdcnt; /* Number of IFD's, assumed to be < 10  */
  unsigned int exiflen;
  int preparsed,endian;
} exifparser;

extern int fuji_exif_debug; /* Non-zero for debug messages */

/*
Parse an exif header, return size of the rest of the file
*/
unsigned int exif_header_parse(exifparser *exifdat);

/* 
   Count the number of IFD's and log their pointers 
*/
int stat_exif(exifparser *exifdata);

/*
  Dump out all tag descriptions and values (vals not yet implemented)
*/
int dump_exif(exifparser *exifdat);

/* 
   Return tag number of directory entry at tagind
 */
int tagnum(unsigned char *data,int tagind);

/* 
   Return a character description of the tag
*/
char *tagname(int tagnum);

/* 
   Pull value of tag tagnum from data
*/
int getintval(unsigned char *data, int tagnum);


long lilend(unsigned char *data, int size);

/* 
   Return data size of directory entry at tagind
 */
int datsize(unsigned char *data,int tagind);

/* 
   Return "value" of directory entry at tagind
 */
int theval(unsigned char *data,int tagind);

/* 
   Set the "value" of directory entry at tagind
*/

void setval(unsigned char *data,int tagind,long newval);

/* directly from Ohno's perl script (IFD=Image Format Descriptor ?)*/

long next_ifd(unsigned char *exif,int num);

#endif /* _exif_ */
