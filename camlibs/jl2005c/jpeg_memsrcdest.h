#ifndef CAMLIBS_JL2005C_JPEG_MEMSRCDEST_H
#define CAMLIBS_JL2005C_JPEG_MEMSRCDEST_H

#include <jpeglib.h>

#if JPEG_LIB_VERSION < 80 && !defined(MEM_SRCDST_SUPPORTED)

void
jpeg_mem_src (j_decompress_ptr cinfo, unsigned char * buffer,
	unsigned long bufsize);

void
jpeg_mem_dest (j_compress_ptr cinfo, unsigned char ** outbuffer,
	unsigned long * outsize);

#endif

#endif /* !defined(CAMLIBS_JL2005C_JPEG_MEMSRCDEST_H) */
