/* This code was written by Nathan Stenzel for gphoto */
/* GPL */
/* It might be safe enough to put this into the Makefile now */

/* jpeg_add_marker need to have the data copying code put in. memcpy? */

#include <stdio.h>

struct chunk{
long size;
unsigned char *data;
};

chunk_destroy(struct chunk *mychunk)
{
    mychunk->size=0;
    free(mychunk->data);
}

struct jpeg {
int count;
struct chunk marker[20]; /* I think this should be big enough */
};

jpeg_init(struct jpeg *myjpeg)
{
    myjpeg->count=0;
}

jpeg_destroy(struct jpeg *myjpeg)
{
    int count;
    for (count=0; count<myjpeg->count; count++)
        chunk_destroy(&(myjpeg->marker[count]));
    myjpeg->count=0;
}

enum jpegmarker {start, APPO, quantization, huffman, SsSeAhAl};
const char markers[] = {
start, APPO, quantization, huffman, SsSeAhAl
};

char jpeg_findamarker(char *id, long *location, struct chunk picture, long start) {
    long position;
    char x;
    for (position=start; position<picture.size; position++)
        if (picture.data[position]==0xff)
        {
        *id=picture.data[position+1];
        *location=position;
        return 1;
        }
    return 0;
}

char jpeg_findactivemarker(char *id, long *location, struct chunk picture, long start) {
    long position=start;
    char temp=*id;
    while(jpeg_findamarker(&temp, &position, picture, position))
        if (temp) {
            *id=temp;
            *location=position;
            return 1;
        }
    return 0;
}

void jpeg_add_marker(struct jpeg *myjpeg, struct chunk picture, long location)
{
    long length;
    long position = location;
    char id=0;
    if (jpeg_findactivemarker(&id, &position, picture, position))
        length=position-location+1;
        else
        length=picture.size-location+1;
    myjpeg->marker[myjpeg->count].size=length;
    myjpeg->marker[myjpeg->count].data = (char *)malloc(length);
// copy info to marker chunk here
    memcpy(myjpeg->marker[myjpeg->count].data, picture.data+location, length);
//    gp_debug_printf (GP_DEBUG_LOW, "jpeg.c", "JPEG marker 0xFF %c", picture.data[location+1]);
    printf ("JPEG marker 0xFF %c", picture.data[location+1]);
    myjpeg->count++;
}

char jpeg_parse(struct jpeg *myjpeg, struct chunk picture)
{
    long position=0;
    char id;
    while (jpeg_findactivemarker(&id, &position, picture, position))
        jpeg_add_marker(myjpeg, picture, position);
/* add the last marker now */
    jpeg_add_marker(myjpeg, picture, position);
}
