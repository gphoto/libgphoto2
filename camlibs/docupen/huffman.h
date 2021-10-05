#ifndef CAMLIBS_DOCUPEN_HUFFMAN_H
#define CAMLIBS_DOCUPEN_HUFFMAN_H

struct decoder {
	unsigned char *data;
	int length;
	int bitoff;
	int byteoff;
	int state;
};

void decoder_init(struct decoder *d, void *data, int length);
int  decoder_token(struct decoder *d, int *type, int *len);

enum {
	BLACK = 0,
	WHITE = 1,
	TERM = (1 << 7)
};

enum {
	DECODER_NOOP = 0,
	DECODER_BLACK,
	DECODER_WHITE,
	DECODER_EOL
};

#endif /* !defined(CAMLIBS_DOCUPEN_HUFFMAN_H) */
