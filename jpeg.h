/* This code was written by Nathan Stenzel for gphoto */
/* GPL */


struct chunk{
    int size;
    unsigned char *data;
};

chunk_new(struct chunk *mychunk, int length);

chunk_print(struct chunk *mychunk);

chunk_destroy(struct chunk *mychunk);

struct jpeg {
    int count;
    struct chunk marker[20]; /* I think this should be big enough */
};

jpeg_init(struct jpeg *myjpeg);

jpeg_destroy(struct jpeg *myjpeg);

enum jpegmarker {start, APPO, quantization, huffman, SsSeAhAl};

const char markers[] = {
    start, APPO, quantization, huffman, SsSeAhAl
};

char jpeg_findff(int *location, struct chunk *picture);

char jpeg_findactivemarker(char *id, int *location, struct chunk *picture);

void jpeg_add_marker(struct jpeg *myjpeg, struct chunk *picture, int start, int end);

char jpeg_parse(struct jpeg *myjpeg, struct chunk *picture);

char jpeg_print(struct jpeg *myjpeg);
