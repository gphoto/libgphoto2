/* This code was written by Nathan Stenzel for gphoto */
/* GPL */

enum jpegmarker {start=0xD8, APPO=0xE0, quantization=0xDB, huffman=0xC4, SsSeAhAl=0xDA, SOFC0=0xC0};

const markers[] = {
    start, APPO, quantization, huffman, SsSeAhAl, SOFC0
};

const char *markernames[] = {
    "Start", "APPO", "Quantization table", "Huffman table", "SsSeAhAl", "SOFC0" };

struct chunk{
    int size;
    unsigned char *data;
};

void chunk_new(struct chunk *mychunk, int length);

void chunk_print(struct chunk *mychunk);

void chunk_destroy(struct chunk *mychunk);

struct jpeg {
    int count;
    struct chunk marker[20]; /* I think this should be big enough */
};

void jpeg_init(struct jpeg *myjpeg);

void jpeg_destroy(struct jpeg *myjpeg);

char jpeg_findff(int *location, struct chunk *picture);

char jpeg_findactivemarker(char *id, int *location, struct chunk *picture);

void jpeg_add_marker(struct jpeg *myjpeg, struct chunk *picture, int start, int end);

void jpeg_parse(struct jpeg *myjpeg, struct chunk *picture);

void jpeg_print(struct jpeg *myjpeg);

char *jpeg_markername(int c);