#include <jpeglib.h>

void
jpeg_mem_src (j_decompress_ptr cinfo, unsigned char * buffer,
	unsigned long bufsize);

void
jpeg_mem_dest (j_compress_ptr cinfo, unsigned char ** outbuffer,
	unsigned long * outsize);
