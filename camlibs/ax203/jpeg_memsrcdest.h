#include <jpeglib.h>

void
jpeg_mem_src (j_decompress_ptr cinfo, const JOCTET * buffer, size_t bufsize);

void jpeg_mem_dest (j_compress_ptr cinfo);
int jpeg_mem_dest_get_final_image_size (j_compress_ptr cinfo);
JOCTET * jpeg_mem_dest_get_buffer (j_compress_ptr cinfo);
void jpeg_mem_dest_free_buffer (j_compress_ptr cinfo);
