/* jpeg.c
 * This code was written by Nathan Stenzel for gphoto
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gphoto2-library.h>
#include "jpeg.h"

// call example:nullpictureabort(picture,"Picture");
#define nullpointerabort(pointer,name,val) \
    if (pointer==NULL) { printf(name " does not exist\n"); \
        return val; }

#define CHECK_RESULT(result)       {int r = (result); if (r < 0) return (r);}

chunk *chunk_new(int length)
{
chunk *mychunk;
    printf("Entered chunk_new\n");
    mychunk =  (chunk *) malloc(sizeof(chunk));
    mychunk->size=length;
//    printf("New chunk of size %i\n", mychunk->size);
    mychunk->data = (char *)malloc(length);
    if (mychunk==NULL) printf("Failed to allocate new chunk!\n");
    return mychunk;
}

chunk *chunk_new_filled(int length, char *data)
{
    chunk *mychunk;
    printf("Entered chunk_new_filled\n");
    mychunk = chunk_new(length);
    printf("Filling the chunk data via chunk_new_filled\n");
    memcpy(mychunk->data, data, length);
    return mychunk;
}

void chunk_destroy(chunk *mychunk)
{
    nullpointerabort(mychunk, "Chunk",);
    mychunk->size=0;
    free(mychunk->data);
    free(mychunk);
}


void chunk_print(chunk *mychunk)
{
    int x;
//    printf("Size=%i\n", mychunk->size);
    nullpointerabort(mychunk, "Chunk",);
    for (x=0; x<mychunk->size; x++)
        printf("%hX ", mychunk->data[x]);
    printf("\n");
}

char gp_jpeg_findff(int *location, chunk *picture)
{
//printf("Entered jpeg_findamarker!!!!!!\n");
    nullpointerabort(picture, "Picture",0);
    while(*location<picture->size)
    {
//        printf("%hX ",picture->data[*location]);
        if (picture->data[*location]==0xff)
        {
//        printf("return success!\n");
        return 1;
        }
    (*location)++;
    }
//    printf("return failure!\n");
    return 0;
}

char gp_jpeg_findactivemarker(char *id, int *location, chunk *picture)
{
//    printf("Entered gp_jpeg_findactivemarker!!!!!!!!\n");
    nullpointerabort(picture, "Picture",0);
    while(gp_jpeg_findff(location, picture) && ((*location+1)<picture->size))
        if (picture->data[*location+1]) {
            *id=picture->data[*location+1];
            return 1;
        }
    return 0;
}

char *gp_jpeg_markername(int c)
{
    int x;
//    printf("searching for marker %X in list\n",c);
//    printf("%i\n", sizeof(markers));
    for (x=0; x<sizeof(JPEG_MARKERS); x++)
    {
//        printf("checking to see if it is marker %X\n", markers[x]);

        if (c==JPEG_MARKERS[x])
            return (char *)JPEG_MARKERNAMES[x];
    }
    return "Undefined marker";
}

jpeg *gp_jpeg_new()
{
    jpeg *temp;
    temp=malloc(sizeof(jpeg));
    temp->count=0;
    return temp;
}

void gp_jpeg_destroy(jpeg *myjpeg)
{
    int count;
    for (count=0; count<myjpeg->count; count++)
        chunk_destroy(myjpeg->marker[count]);
    myjpeg->count=0;
    free(myjpeg);
}

void gp_jpeg_add_marker(jpeg *myjpeg, chunk *picture, int start, int end)
{
    int length;
    nullpointerabort(picture, "Picture",);
    length=(int)(end-start+1);
//    printf("Add marker #%i starting from %i and ending at %i for a length of %i\n", myjpeg->count, start, end, length);
    myjpeg->marker[myjpeg->count] = chunk_new(length);
//    printf("Read length is: %i\n", myjpeg->marker[myjpeg->count].size);
    memcpy(myjpeg->marker[myjpeg->count]->data, picture->data+start, length);
    chunk_print(myjpeg->marker[myjpeg->count]);
    myjpeg->count++;
}

void gp_jpeg_add_chunk(jpeg *myjpeg, chunk *source)
{ /* Warning! This points to the added chunk instead of deleting it! */
    printf("Entered gp_jpeg_add_chunk\n");
    nullpointerabort(source, "Chunk to add",);
    myjpeg->marker[myjpeg->count]=source;
    myjpeg->count++;
}

void gp_jpeg_parse(jpeg *myjpeg, chunk *picture)
{
    int position=0;
    int lastposition;
    char id;
    nullpointerabort(picture,"Picture",);
    if (picture->data[0]!=0xff)
        {
            gp_jpeg_findactivemarker(&id, &position, picture);
            gp_jpeg_add_marker(myjpeg,picture,0,position-1);
            lastposition=position;
            position=position++;
        }
        else
        {
        lastposition=position;
        position+=2;
        gp_jpeg_findactivemarker(&id, &position, picture);
        gp_jpeg_add_marker(myjpeg,picture,0,position-1);
        lastposition=position;
        position+=2;
        }
    while (position<picture->size)
        {
            gp_jpeg_findactivemarker(&id, &position, picture);
            gp_jpeg_add_marker(myjpeg,picture,lastposition,position-1);
            lastposition=position;
            position+=2;
        }
    position-=2;
    if (position<picture->size)
    gp_jpeg_add_marker(myjpeg,picture,lastposition,picture->size-1);
}

void gp_jpeg_print(jpeg *myjpeg)
{
    int c;
    printf("There are %i markers\n", myjpeg->count);
    for (c=0; c < myjpeg->count; c++)
    {
        printf("%s:\n",gp_jpeg_markername(myjpeg->marker[c]->data[1]));
        chunk_print(myjpeg->marker[c]);
    }
}

chunk *gp_jpeg_make_start() //also makes the JFIF marker
{
    chunk *temp;
    // Start marker and
    // JFIF APPO Marker:Version 1.01, density 1x2 (the 00 01 00 02)
    temp = chunk_new_filled(20,  "\xFF\xD8"
        "\xFF\xE0\x00\x10\x4A\x46\x49\x46" "\x00\x01\x01\x00\x00\x01\x00\x02" "\x00\x00");
    return temp;
}

chunk *gp_jpeg_make_SOFC (int width, int height, char vh1, char vh2, char vh3, char q1, char q2, char q3)
{
    chunk *target;

    char sofc_data[]={
        0xFF, 0xC0, 0x00, 0x11,  0x08, 0x00, 0x80, 0x01,
        0x40, 0x03, 0x01, 0x11,  0x00, 0x02, 0x21, 0x01,  0x03, 0x11, 0x00
        };
    target = chunk_new(sizeof(sofc_data));
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

chunk *gp_jpeg_makeSsSeAhAl(int huffset1, int huffset2, int huffset3)
{
    chunk *target;
    printf("About to call chunk_new_filled\n");
    target= chunk_new_filled(14, "\xFF\xDA\x00\x0C\x03\x01\x00\x02"
        "\x00\x03\x00\x00\x3F\x00");
    target->data[6] = huffset1;
    target->data[8] = huffset2;
    target->data[10] = huffset3;
    return target;
}

void gp_jpeg_print_quantization_table(jpeg_quantization_table *table)
{
    int x;
    nullpointerabort(table, "Quantization table",);
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


chunk *gp_jpeg_make_quantization(jpeg_quantization_table *table, char number)
{
    chunk *temp;
    char x,y,z,c;
    temp = chunk_new(5+64);
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

jpeg_quantization_table *gp_jpeg_quantization2table(chunk *qmarker)
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


jpeg *gp_jpeg_header(int width, int height,
    char vh1, char vh2, char vh3,
    char q1, char q2, char q3,
    jpeg_quantization_table *quant1, jpeg_quantization_table *quant2,
    char huffset1, char huffset2, char huffset3,
    chunk *huff1, chunk *huff2, chunk *huff3, chunk *huff4)
{
    jpeg *temp;
    temp = gp_jpeg_new();
    gp_jpeg_add_chunk(temp, gp_jpeg_make_start());
    gp_jpeg_add_chunk(temp, gp_jpeg_make_quantization(quant1, 0));
    gp_jpeg_add_chunk(temp, gp_jpeg_make_quantization(quant2, 1));
    gp_jpeg_add_chunk(temp, gp_jpeg_make_SOFC(width,height, vh1,vh2,vh3, q1,q2,q3));
    gp_jpeg_add_chunk(temp, huff1);
    gp_jpeg_add_chunk(temp, huff2);
    gp_jpeg_add_chunk(temp, huff3);
    gp_jpeg_add_chunk(temp, huff4);
    printf("About to make and add the SsSeAhAl marker\n");
    gp_jpeg_add_chunk(temp, gp_jpeg_makeSsSeAhAl(huffset1, huffset2, huffset3));
    return temp;
}

//#define TESTING_JPEG_C

#ifndef TESTING_JPEG_C
char gp_jpeg_write(CameraFile *file, const char *filename, jpeg *myjpeg)
{
    int x;
    CHECK_RESULT (gp_file_set_name (file, filename));
    CHECK_RESULT (gp_file_set_mime_type(file, GP_MIME_JPEG));
    for (x=0; x<myjpeg->count; x++)
        CHECK_RESULT (gp_file_append(file, myjpeg->marker[x]->data, myjpeg->marker[x]->size));
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
gp_jpeg_print_quantization_table(&mytable);
myjpeg = gp_jpeg_new();
picture = chunk_new(sizeof(testdata));
picture->data=testdata;
picture->size=sizeof(testdata);
printf("testdata size is %i\n",picture->size);
chunk_print(picture);
printf("Call jpeg_parse!!!!!!!!!!!!!!!!!!!!!!!\n");
gp_jpeg_parse(myjpeg,picture);

printf("\nPrint the jpeg table\n");
gp_jpeg_print(myjpeg);
printf("\nCall gp_jpeg_destroy\n");
gp_jpeg_destroy(myjpeg);
/* You can't call chunk_destroy because testdata is a constant.
 * Since picture->data points to it, it would segfault.
 */
free(picture);
printf("chunk_new and chunk_destroy tests\n");
picture = chunk_new(10);
for (x=0; x<10; x++) picture->data[x]=x;
for (x=0; x<10; x++) printf("%hX ",picture->data[x]);
chunk_destroy(picture);
printf("\n");
}
#endif
