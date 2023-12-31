/*
* memsrc.c
*
* Copyright (C) 1994-1996, Thomas G. Lane.
* This file is part of the Independent JPEG Group's software.
* For conditions of distribution and use, see the accompanying README file.
*
* This file contains decompression data source routines for the case of
* reading JPEG data from a memory buffer that is preloaded with the entire
* JPEG file. This would not seem especially useful at first sight, but
* a number of people have asked for it.
* This is really just a stripped-down version of jdatasrc.c. Comparison
* of this code with jdatasrc.c may be helpful in seeing how to make
* custom source managers for other purposes.
*/

/* this is not a core library module, so it doesn't define JPEG_INTERNALS */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>

#ifdef HAVE_LIBJPEG
#include <jpeglib.h>
#include <jerror.h>
#include "jpeg_memsrcdest.h"

/* libjpeg8 and later come with their own (API compatible) memory source
   and dest, and older versions may have it backported */
#if JPEG_LIB_VERSION < 80 && !defined(MEM_SRCDST_SUPPORTED)

/* Expanded data source object for memory input */

typedef struct {
	struct jpeg_source_mgr pub; /* public fields */

	JOCTET eoi_buffer[2]; /* a place to put a dummy EOI */
} my_source_mgr;

typedef my_source_mgr * my_src_ptr;


/*
* Initialize source --- called by jpeg_read_header
* before any data is actually read.
*/

METHODDEF(void)
init_source (j_decompress_ptr cinfo)
{
	/* No work, since jpeg_mem_src set up the buffer pointer and count.
	* Indeed, if we want to read multiple JPEG images from one buffer,
	* this *must* not do anything to the pointer.
	*/
}


/*
* Fill the input buffer --- called whenever buffer is emptied.
*
* In this application, this routine should never be called; if it is called,
* the decompressor has overrun the end of the input buffer, implying we
* supplied an incomplete or corrupt JPEG datastream. A simple error exit
* might be the most appropriate response.
*
* But what we choose to do in this code is to supply dummy EOI markers
* in order to force the decompressor to finish processing and supply
* some sort of output image, no matter how corrupted.
*/

METHODDEF(boolean)
fill_input_buffer (j_decompress_ptr cinfo)
{
	my_src_ptr src = (my_src_ptr) cinfo->src;

	WARNMS(cinfo, JWRN_JPEG_EOF);

	/* Create a fake EOI marker */
	src->eoi_buffer[0] = (JOCTET) 0xFF;
	src->eoi_buffer[1] = (JOCTET) JPEG_EOI;
	src->pub.next_input_byte = src->eoi_buffer;
	src->pub.bytes_in_buffer = 2;

	return TRUE;
}


/*
* Skip data --- used to skip over a potentially large amount of
* uninteresting data (such as an APPn marker).
*
* If we overrun the end of the buffer, we let fill_input_buffer deal with
* it. An extremely large skip could cause some time-wasting here, but
* it really isn't supposed to happen ... and the decompressor will never
* skip more than 64K anyway.
*/

METHODDEF(void)
skip_input_data (j_decompress_ptr cinfo, long num_bytes)
{
	my_src_ptr src = (my_src_ptr) cinfo->src;

	if (num_bytes > 0) {
		while (num_bytes > (long) src->pub.bytes_in_buffer) {
			num_bytes -= (long) src->pub.bytes_in_buffer;
			(void) fill_input_buffer(cinfo);
			/* note we assume that fill_input_buffer will never
			* return FALSE, so suspension need not be handled.
			*/
		}
		src->pub.next_input_byte += (size_t) num_bytes;
		src->pub.bytes_in_buffer -= (size_t) num_bytes;
	}
}


/*
* An additional method that can be provided by data source modules is the
* resync_to_restart method for error recovery in the presence of RST markers.
* For the moment, this source module just uses the default resync method
* provided by the JPEG library. That method assumes that no backtracking
* is possible.
*/


/*
* Terminate source --- called by jpeg_finish_decompress
* after all data has been read. Often a no-op.
*
* NB: *not* called by jpeg_abort or jpeg_destroy; surrounding
* application must deal with any cleanup that should happen even
* for error exit.
*/

METHODDEF(void)
term_source (j_decompress_ptr cinfo)
{
	/* no work necessary here */
}


/*
* Prepare for input from a memory buffer.
*/

GLOBAL(void)
jpeg_mem_src (j_decompress_ptr cinfo, unsigned char * buffer,
	unsigned long bufsize)
{
	my_src_ptr src;

	/* The source object is made permanent so that a series of JPEG images
	* can be read from a single buffer by calling jpeg_mem_src
	* only before the first one.
	* This makes it unsafe to use this manager and a different source
	* manager serially with the same JPEG object. Caveat programmer.
	*/
	if (cinfo->src == NULL) { /* first time for this JPEG object? */
		cinfo->src = (struct jpeg_source_mgr *)
			(*cinfo->mem->alloc_small) ((j_common_ptr) cinfo,
						    JPOOL_PERMANENT,
						    sizeof(my_source_mgr));
	}

	src = (my_src_ptr) cinfo->src;
	src->pub.init_source = init_source;
	src->pub.fill_input_buffer = fill_input_buffer;
	src->pub.skip_input_data = skip_input_data;
	/* use default method */
	src->pub.resync_to_restart = jpeg_resync_to_restart;
	src->pub.term_source = term_source;

	src->pub.next_input_byte = buffer;
	src->pub.bytes_in_buffer = bufsize;
}



/* Memory destination source modelled after Thomas G. Lane's memory source
 * support and jdatadst.c
 *
 * Copyright (C) 2010, Hans de Goede
 *
 * This code may be used under the same conditions as Thomas G. Lane's memory
 * source (see the copyright header at the top of this file).
 */

typedef struct {
	struct jpeg_destination_mgr pub; /* public fields */

	JOCTET **buffer;              /* start of buffer */
	unsigned long buf_size, *outsize;
} my_destination_mgr;

typedef my_destination_mgr * my_dest_ptr;

#define OUTPUT_BUF_SIZE 32768   /* choose an efficiently fwrite'able size */


/*
 * Initialize destination --- called by jpeg_start_compress
 * before any data is actually written.
 */

METHODDEF(void)
init_destination (j_compress_ptr cinfo)
{
	/* No work, since jpeg_mem_dest set up the buffer pointer and count.
	* Indeed, if we want to write multiple JPEG images to one buffer,
	* this *must* not do anything to the pointer.
	*/
}

/*
 * Empty the output buffer --- called whenever buffer fills up.
 *
 * In typical applications, this should write the entire output buffer
 * (ignoring the current state of next_output_byte & free_in_buffer),
 * reset the pointer & count to the start of the buffer, and return TRUE
 * indicating that the buffer has been dumped.
 *
 * In applications that need to be able to suspend compression due to output
 * overrun, a FALSE return indicates that the buffer cannot be emptied now.
 * In this situation, the compressor will return to its caller (possibly with
 * an indication that it has not accepted all the supplied scanlines).  The
 * application should resume compression after it has made more room in the
 * output buffer.  Note that there are substantial restrictions on the use of
 * suspension --- see the documentation.
 *
 * When suspending, the compressor will back up to a convenient restart point
 * (typically the start of the current MCU). next_output_byte & free_in_buffer
 * indicate where the restart point will be if the current call returns FALSE.
 * Data beyond this point will be regenerated after resumption, so do not
 * write it out when emptying the buffer externally.
 */

METHODDEF(boolean)
empty_output_buffer (j_compress_ptr cinfo)
{
	my_dest_ptr dest = (my_dest_ptr) cinfo->dest;

	*dest->buffer = realloc (*dest->buffer,
					dest->buf_size + OUTPUT_BUF_SIZE);
	if (!*dest->buffer)
		ERREXIT1(cinfo, JERR_OUT_OF_MEMORY, 0);

	dest->pub.next_output_byte = *dest->buffer + dest->buf_size;
	dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
	dest->buf_size += OUTPUT_BUF_SIZE;

	return TRUE;
}

/*
 * Terminate destination --- called by jpeg_finish_compress
 * after all data has been written.  Usually needs to flush buffer.
 *
 * NB: *not* called by jpeg_abort or jpeg_destroy; surrounding
 * application must deal with any cleanup that should happen even
 * for error exit.
 */

METHODDEF(void)
term_destination (j_compress_ptr cinfo)
{
	my_dest_ptr dest = (my_dest_ptr) cinfo->dest;

	*dest->outsize = dest->buf_size - dest->pub.free_in_buffer;
}

GLOBAL(void)
jpeg_mem_dest (j_compress_ptr cinfo, unsigned char ** outbuffer,
	unsigned long * outsize)
{
	my_dest_ptr dest;

	/* The destination object is made permanent so that multiple JPEG
	 * images can be written to the same file without re-executing
	 * jpeg_stdio_dest.
	 * This makes it dangerous to use this manager and a different
	 * destination manager serially with the same JPEG object, because
	 * their private object sizes may be different.
	 *
	 * Caveat programmer.
	 */
	if (cinfo->dest == NULL) {  /* first time for this JPEG object? */
		cinfo->dest = (struct jpeg_destination_mgr *)
			(*cinfo->mem->alloc_small) ((j_common_ptr) cinfo,
						    JPOOL_PERMANENT,
						    sizeof(my_destination_mgr));
	}

	dest = (my_dest_ptr) cinfo->dest;
	dest->pub.init_destination = init_destination;
	dest->pub.empty_output_buffer = empty_output_buffer;
	dest->pub.term_destination = term_destination;
	dest->buffer = outbuffer;
	dest->buf_size = *outsize;
	dest->outsize = outsize;

	if (*dest->buffer == NULL || dest->buf_size == 0) {
		/* Allocate initial buffer */
		*dest->buffer = malloc(OUTPUT_BUF_SIZE);
		if (*dest->buffer == NULL)
			ERREXIT1(cinfo, JERR_OUT_OF_MEMORY, 10);
		dest->buf_size = OUTPUT_BUF_SIZE;
	}

	dest->pub.next_output_byte = *dest->buffer;
	dest->pub.free_in_buffer = dest->buf_size;
}

#endif
#endif
