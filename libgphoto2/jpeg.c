/** \file
 * \author This code was written by Nathan Stenzel for gphoto
 *
 * \par License
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * \par
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * \par
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "jpeg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gphoto2/gphoto2-file.h>

/* call example:nullpictureabort(picture,"Picture",0); */
#define nullpointerabort(pointer,name,val) \
    if (pointer==NULL) { printf(name " does not exist\n"); return val; }

/* call example:nullpictureabortvoid(picture,"Picture"); */
#define nullpointerabortvoid(pointer,name) \
    if (pointer==NULL) { printf(name " does not exist\n"); return; }

#define CHECK_RESULT(result)       {int r = (result); if (r < 0) return (r);}

/* Determine the number of elements in an array. */
#define countof(array) (sizeof(array) / sizeof((array)[0]))

const jpegmarker JPEG_MARKERS[] = {
    JPEG_START,             JPEG_COMMENT,           JPEG_APPO,
    JPEG_QUANTIZATION,      JPEG_HUFFMAN,           JPEG_SOFC0,
    JPEG_SSSEAHAL,          JPEG_EOI
};

const char *JPEG_MARKERNAMES[] = {
    "Start",                "Comment",              "APPO",
    "Quantization table",   "Huffman table",        "SOFC0",
    "SsSeAhAl",             "End of image"
};

chunk *gpi_jpeg_chunk_new(int length)
{
	chunk *mychunk;
	printf("Entered gpi_jpeg_chunk_new\n");
	mychunk =  (chunk *) malloc(sizeof(chunk));
	if (mychunk==NULL) {
		printf("Failed to allocate new chunk!\n");
		return NULL;
	}
	mychunk->size=length;
/*    printf("New chunk of size %i\n", mychunk->size); */
	mychunk->data = malloc(length);
	return mychunk;
}

chunk *gpi_jpeg_chunk_new_filled(int length, char *data)
{
    chunk *mychunk;
    printf("Entered gpi_jpeg_chunk_new_filled\n");
    mychunk = gpi_jpeg_chunk_new(length);
    if (!mychunk)
	return NULL;
    printf("Filling the chunk data via chunk_new_filled\n");
    memcpy(mychunk->data, data, length);
    return mychunk;
}

void gpi_jpeg_chunk_destroy(chunk *mychunk)
{
    nullpointerabortvoid(mychunk, "Chunk");
    mychunk->size=0;
    free(mychunk->data);
    free(mychunk);
}


void gpi_jpeg_chunk_print(chunk *mychunk)
{
    int x;
/*    printf("Size=%i\n", mychunk->size); */
    nullpointerabortvoid(mychunk, "Chunk");
    for (x=0; x<mychunk->size; x++)
        printf("%hX ", mychunk->data[x]);
    printf("\n");
}

char gpi_jpeg_findff(int *location, chunk *picture)
{
/* printf("Entered jpeg_findamarker!!!!!!\n"); */
    nullpointerabort(picture, "Picture",0);
    while(*location<picture->size)
    {
/*        printf("%hX ",picture->data[*location]); */
        if (picture->data[*location]==0xff)
        {
/*        printf("return success!\n"); */
        return 1;
        }
    (*location)++;
    }
/*    printf("return failure!\n"); */
    return 0;
}

char gpi_jpeg_findactivemarker(char *id, int *location, chunk *picture)
{
/*    printf("Entered gpi_jpeg_findactivemarker!!!!!!!!\n"); */
    nullpointerabort(picture, "Picture",0);
    while(gpi_jpeg_findff(location, picture) && ((*location+1)<picture->size))
        if (picture->data[*location+1]) {
            *id=picture->data[*location+1];
            return 1;
        }
    return 0;
}

char *gpi_jpeg_markername(unsigned int c)
{
    unsigned int x;
/*    printf("searching for marker %X in list\n",c); */
/*    printf("%i\n", sizeof(markers)); */
    for (x=0; x<countof(JPEG_MARKERS); x++)
    {
/*        printf("checking to see if it is marker %X\n", markers[x]); */

        if (c==JPEG_MARKERS[x])
            return (char *)JPEG_MARKERNAMES[x];
    }
    return "Undefined marker";
}

jpeg *gpi_jpeg_new()
{
    jpeg *temp;
    temp=malloc(sizeof(jpeg));
    temp->count=0;
    return temp;
}

void gpi_jpeg_destroy(jpeg *myjpeg)
{
    int count;
    for (count=0; count<myjpeg->count; count++)
        gpi_jpeg_chunk_destroy(myjpeg->marker[count]);
    myjpeg->count=0;
    free(myjpeg);
}

void gpi_jpeg_add_marker(jpeg *myjpeg, chunk *picture, int start, int end)
{
    int length;
    nullpointerabortvoid(picture, "Picture");
    length=(int)(end-start+1);
/*  printf("Add marker #%i starting from %i and ending at %i for a length of %i\n", myjpeg->count, start, end, length); */
    myjpeg->marker[myjpeg->count] = gpi_jpeg_chunk_new(length);
    if (!myjpeg->marker[myjpeg->count])
	return;
/*  printf("Read length is: %i\n", myjpeg->marker[myjpeg->count].size); */
    memcpy(myjpeg->marker[myjpeg->count]->data, picture->data+start, length);
    gpi_jpeg_chunk_print(myjpeg->marker[myjpeg->count]);
    myjpeg->count++;
}

void gpi_jpeg_add_chunk(jpeg *myjpeg, chunk *source)
{ /* Warning! This points to the added chunk instead of deleting it! */
    printf("Entered gpi_jpeg_add_chunk\n");
    nullpointerabortvoid(source, "Chunk to add");
    myjpeg->marker[myjpeg->count]=source;
    myjpeg->count++;
}

void gpi_jpeg_parse(jpeg *myjpeg, chunk *picture)
{
    int position=0;
    int lastposition;
    char id;
    nullpointerabortvoid(picture,"Picture");
    if (picture->data[0]!=0xff)
        {
            gpi_jpeg_findactivemarker(&id, &position, picture);
            gpi_jpeg_add_marker(myjpeg,picture,0,position-1);
            lastposition=position;
            position++;
        }
        else
        {
        lastposition=position;
        position+=2;
        gpi_jpeg_findactivemarker(&id, &position, picture);
        gpi_jpeg_add_marker(myjpeg,picture,0,position-1);
        lastposition=position;
        position+=2;
        }
    while (position<picture->size)
        {
            gpi_jpeg_findactivemarker(&id, &position, picture);
            gpi_jpeg_add_marker(myjpeg,picture,lastposition,position-1);
            lastposition=position;
            position+=2;
        }
    position-=2;
    if (position<picture->size)
    gpi_jpeg_add_marker(myjpeg,picture,lastposition,picture->size-1);
}

void gpi_jpeg_print(jpeg *myjpeg)
{
    int c;
    printf("There are %i markers\n", myjpeg->count);
    for (c=0; c < myjpeg->count; c++)
    {
        printf("%s:\n",gpi_jpeg_markername(myjpeg->marker[c]->data[1]));
        gpi_jpeg_chunk_print(myjpeg->marker[c]);
    }
}

chunk *gpi_jpeg_make_start() /* also makes the JFIF marker */
{
    chunk *temp;
    /* Start marker and
       JFIF APPO Marker:Version 1.01, density 1x2 (the 00 01 00 02) */
    temp = gpi_jpeg_chunk_new_filled(20,  "\xFF\xD8"
        "\xFF\xE0\x00\x10\x4A\x46\x49\x46" "\x00\x01\x01\x00\x00\x01\x00\x02" "\x00\x00");
    return temp;
}

chunk *gpi_jpeg_make_SOFC (int width, int height, char vh1, char vh2, char vh3, char q1, char q2, char q3)
{
    chunk *target;

    char sofc_data[]={
        0xFF, 0xC0, 0x00, 0x11,  0x08, 0x00, 0x80, 0x01,
        0x40, 0x03, 0x01, 0x11,  0x00, 0x02, 0x21, 0x01,  0x03, 0x11, 0x00
        };
    target = gpi_jpeg_chunk_new(sizeof(sofc_data));
    if (target==NULL) { printf("New SOFC failed allocation\n"); return target; }
    memcpy(target->data, sofc_data, sizeof(sofc_data));
    target->data[5] = (height&0xff00) >> 8;
    target->data[6] = height&0xff;
    target->data[7] = (width&0xff00) >> 8;
    target->data[8] = width&0xff;
    target->data[11]= vh1;
    target->data[12]= q1;
    target->data[14]= vh2;
    target->data[15]= q2;
    target->data[17]= vh3;
    target->data[18]= q3;
    return target;
}

chunk *gpi_jpeg_makeSsSeAhAl(int huffset1, int huffset2, int huffset3)
{
    chunk *target;
    printf("About to call gpi_jpeg_chunk_new_filled\n");
    target= gpi_jpeg_chunk_new_filled(14, "\xFF\xDA\x00\x0C\x03\x01\x00\x02"
        "\x00\x03\x00\x00\x3F\x00");
    if (!target) return NULL;
    target->data[6] = huffset1;
    target->data[8] = huffset2;
    target->data[10] = huffset3;
    return target;
}

void gpi_jpeg_print_quantization_table(jpeg_quantization_table *table)
{
    int x;
    nullpointerabortvoid(table, "Quantization table");
    for (x=0; x<64; x++)
    {
        if (x && ((x%8)==0))
        {
            printf("\n");
        }
        printf("%3i ", (*table)[x]);
    }
    printf("\n");
}


chunk *gpi_jpeg_make_quantization(const jpeg_quantization_table *table, char number)
{
    chunk *temp;
    char x,y,z,c;
    temp = gpi_jpeg_chunk_new(5+64);
    if (!temp) return NULL;
    memcpy(temp->data, "\xFF\xDB\x00\x43\x01", 5);
    temp->data[4]=number;
    for (c=z=0; z<8; z++)
        if (z%2)
            for (y=0,x=z; y<=z; x--,y++)
                {
                temp->data[5+c] = (*table)[x+y*8];
                temp->data[5+63-c++] = (*table)[63-x-y*8];
                }
        else
            for (x=0,y=z; x<=z; x++,y--)
                {
                temp->data[5+c] = (*table)[x+y*8];
                temp->data[5+63-c++] = (*table)[63-x-y*8];
                }
    return temp;
}

jpeg_quantization_table *gpi_jpeg_quantization2table(chunk *qmarker)
{
    char x,y,z,c;
    jpeg_quantization_table *table;
    table = (jpeg_quantization_table *)malloc(64);
    for (c=z=0; z<8; z++)
        if (z%2)
            for (y=0,x=z; y<=z; x--,y++)
                {
                (*table)[63-x-y*8] = qmarker->data[5+63-c];
                (*table)[x+y*8] = qmarker->data[5+c++];
                }
        else
            for (x=0,y=z; x<=z; x++,y--)
                {
                (*table)[63-x-y*8] = qmarker->data[5+63-c];
                (*table)[x+y*8] = qmarker->data[5+c++];
                }
    return table;
}


jpeg *gpi_jpeg_header(int width, int height,
    char vh1, char vh2, char vh3,
    char q1, char q2, char q3,
    const jpeg_quantization_table *quant1, const jpeg_quantization_table *quant2,
    char huffset1, char huffset2, char huffset3,
    chunk *huff1, chunk *huff2, chunk *huff3, chunk *huff4)
{
    jpeg *temp;
    temp = gpi_jpeg_new();
    gpi_jpeg_add_chunk(temp, gpi_jpeg_make_start());
    gpi_jpeg_add_chunk(temp, gpi_jpeg_make_quantization(quant1, 0));
    gpi_jpeg_add_chunk(temp, gpi_jpeg_make_quantization(quant2, 1));
    gpi_jpeg_add_chunk(temp, gpi_jpeg_make_SOFC(width,height, vh1,vh2,vh3, q1,q2,q3));
    gpi_jpeg_add_chunk(temp, huff1);
    gpi_jpeg_add_chunk(temp, huff2);
    gpi_jpeg_add_chunk(temp, huff3);
    gpi_jpeg_add_chunk(temp, huff4);
    printf("About to make and add the SsSeAhAl marker\n");
    gpi_jpeg_add_chunk(temp, gpi_jpeg_makeSsSeAhAl(huffset1, huffset2, huffset3));
    return temp;
}

/*#define TESTING_JPEG_C */

#ifndef TESTING_JPEG_C
char gpi_jpeg_write(CameraFile *file, const char *filename, jpeg *myjpeg)
{
    int x;
    CHECK_RESULT (gp_file_set_name (file, filename));
    CHECK_RESULT (gp_file_set_mime_type(file, GP_MIME_JPEG));
    for (x=0; x<myjpeg->count; x++)
        CHECK_RESULT (gp_file_append(file, (char*)myjpeg->marker[x]->data, myjpeg->marker[x]->size));
    return 1;
}
#endif

#ifdef TESTING_JPEG_C
/* TEST CODE SECTION */
char testdata[] ={
    0xFF,0xD8, 0xFF,0xE0, 0xff,0xDB, 0xFF,0xC4, 0xFF,0xDA,
    0xFF,0xC0, 0xff,0xff};

jpeg_quantization_table mytable = {
      2,  3,  4,  5,  6,  7,  8,  9,
      3,  4,  5,  6,  7,  8,  9, 10,
      4,  5,  6,  7,  8,  9, 10, 11,
      5,  6,  7,  8,  9, 10, 11, 12,
      6,  7,  8,  9, 10, 11, 12, 13,
      7,  8,  9, 10, 11, 12, 13, 14,
      8,  9, 10, 11, 12, 13, 14, 15,
      9, 10, 11, 12, 13, 14, 15, 16};

int main()
{
chunk *picture;
jpeg *myjpeg;
char id;
int location=0,x;
printf("Print the test quantization table\n");
gpi_jpeg_print_quantization_table(&mytable);
myjpeg = gpi_jpeg_new();
picture = gpi_jpeg_chunk_new(sizeof(testdata));
picture->data=testdata;
picture->size=sizeof(testdata);
printf("testdata size is %i\n",picture->size);
gpi_jpeg_chunk_print(picture);
printf("Call jpeg_parse!!!!!!!!!!!!!!!!!!!!!!!\n");
gpi_jpeg_parse(myjpeg,picture);

printf("\nPrint the jpeg table\n");
gpi_jpeg_print(myjpeg);
printf("\nCall gpi_jpeg_destroy\n");
gpi_jpeg_destroy(myjpeg);
/* You can't call gpi_jpeg_chunk_destroy because testdata is a constant.
 * Since picture->data points to it, it would segfault.
 */
free(picture);
printf("gpi_jpeg_chunk_new and gpi_jpeg_chunk_destroy tests\n");
picture = gpi_jpeg_chunk_new(10);
for (x=0; x<10; x++) picture->data[x]=x;
for (x=0; x<10; x++) printf("%hX ",picture->data[x]);
gpi_jpeg_chunk_destroy(picture);
printf("\n");
}
#endif
