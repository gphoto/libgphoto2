/* This code was written by Nathan Stenzel for gphoto */
/* GPL */

/* jpeg_add_marker need to have the data copying code put in. memcpy? */

struct chunk {
long size;
char *data;
};

struct jpeg {
int count;
chunk marker[20]; /* I think this should be big enough */
};

jpeg_init(jpeg *myjpeg)
{
count=0;
}

enum jpegmarker {start, APPO, quantization, huffman, SsSeAhAl};
const char markers[] = {
start, APPO, quantization, huffman, SsSeAhAl
}

char jpeg_findamarker(char *id, long *location, chunk picture, long start) {
    long position;
    char x;
    for (position=start; position<picture.length; position++)
        if (picture.data[position]==0xff)
        {
        *id=picture.data[position+1];
        *location=position;
        return 1;
        }
    return 0;
}

char jpeg_findactivemarker(char *id, long *location, chunk picture, long start) {
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

void jpeg_add_marker(jpeg *myjpeg, chunk picture, long location)
{
    long length;
    long position=location;
    char id=0;
    if (jpeg_findactivemarker(&id, &position, picture, position))
        length=position-location+1;
        else
        length=picture.size-location+1;
    myjpeg.marker[count].size=length;
    myjpeg.marker[count].data = malloc(length);
// copy info to marker chunk here

    myjpeg.count++;
}

char jpeg_parse(jpeg *myjpeg, chunk picture)
{
    long position=0;
    while (jpeg_findactivemarker(&position, picture, position))
        jpeg_add_marker(myjpeg, picture, position);
/* add the last marker now */
    jpeg_add_marker(myjpeg, picture, position);
}
