/* gphoto2-cmd-capture.c
 *
 * Copyright (C) 2001 Lutz Müller <urc8@rz.uni-karlsruhe.de>
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
#include <config.h>
#include "gphoto2-cmd-capture.h"

#include <stdlib.h>
#include <stdio.h>

#include <aalib.h>

#include <gphoto2-port-log.h>

#if HAVE_JPEG
#include <jpeglib.h>
#endif

int
gp_cmd_capture_preview (Camera *camera, CameraFile *file)
{
	int result, event;
	aa_context *c;
	aa_renderparams *params;
	aa_palette palette;

	c = aa_autoinit (&aa_defparams);
	if (!c)
		return (GP_ERROR);
	aa_autoinitkbd (c, 0);
	params = aa_getrenderparams ();
	aa_hidecursor (c);

	result = gp_camera_capture_preview (camera, file);
	if (result < 0)
		return (result);

	while (1) {
		const char *data, *type;
		long int size;
		unsigned char *bitmap;

		gp_file_get_data_and_size (file, &data, &size);
		gp_file_get_mime_type (file, &type);
		bitmap = aa_image (c);

#if HAVE_JPEG
		if (!strcmp (type, GP_MIME_JPEG)) {
			struct jpeg_decompress_struct cinfo;
			struct jpeg_error_mgr pub;
			int i;
			unsigned char *dptr, **lptr, *lines[4];
			FILE *f;

			gp_file_save (file, "/tmp/gphoto.jpg");
			f = fopen ("/tmp/gphoto.jpg", "r");
			if (!f) {
				aa_close (c);
				return (GP_ERROR);
			}

			cinfo.err = jpeg_std_error (&pub);
			gp_log (GP_LOG_DEBUG, "gphoto2", "Preparing "
				"decompression...");
			jpeg_create_decompress (&cinfo);
			jpeg_stdio_src (&cinfo, f);
			jpeg_read_header (&cinfo, TRUE);
			while (cinfo.scale_denom) {
				jpeg_calc_output_dimensions (&cinfo);
				if ((aa_imgwidth (c) >= cinfo.output_width) &&
				    (aa_imgheight (c) >= cinfo.output_height))
					break;
				cinfo.scale_denom *= 2;
			}
			if (!cinfo.scale_denom) {
				gp_log (GP_LOG_DEBUG, "gphoto2", "Screen "
					"too small.");
				jpeg_destroy_decompress (&cinfo);
				aa_close (c);
				return (GP_OK);
			}
			gp_log (GP_LOG_DEBUG, "gphoto2", "AA: (w,h) = (%i,%i)",
				aa_imgwidth (c), aa_imgheight (c));
			jpeg_start_decompress (&cinfo);
			gp_log (GP_LOG_DEBUG, "gphoto2", "JPEG: (w,h) = "
				"(%i,%i)", cinfo.output_width,
				cinfo.output_height);
			cinfo.do_fancy_upsampling = FALSE;
			cinfo.do_block_smoothing = FALSE;
			cinfo.out_color_space = JCS_GRAYSCALE;

			dptr = bitmap;
			while (cinfo.output_scanline < cinfo.output_height) {
				lptr = lines;
				for (i = 0; i < cinfo.rec_outbuf_height; i++) {
					*lptr++ = dptr;
					dptr += aa_imgwidth (c);
				}
				jpeg_read_scanlines (&cinfo, lines,
						cinfo.rec_outbuf_height);
				if (cinfo.output_components == 1)
					printf ("HUH?!?\n");
			}

			jpeg_finish_decompress (&cinfo);
			jpeg_destroy_decompress (&cinfo);

			fclose (f);
		} else
#endif
		{
			/* Silently skip the preview */
			aa_close (c);
			return (GP_OK);
		}

		aa_renderpalette (c, palette, params, 0, 0,
			aa_scrwidth (c), aa_scrheight (c));
		aa_flush (c);
		
		event = aa_getevent (c, 1);
		switch (event) {
		case AA_RESIZE:
			aa_resize (c);
			aa_flush (c);
			break;
		case 32:
			/* Space */
			result = gp_camera_capture_preview (camera, file);
			if (result < 0) {
				aa_close (c);
				return (result);
			}
			break;
		case 305:
			/* ESC */
			aa_close (c);
			return (GP_ERROR);
		default:
			aa_close (c);
			return (GP_OK);
		}
	}

	aa_close (c);

	return (GP_OK);
}
