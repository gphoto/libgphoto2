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

#if HAVE_JPEG
#include <jpeglib.h>
#endif

int
gp_cmd_capture_preview (Camera *camera, CameraFile *file)
{
	int result, event;
	aa_context *context;
	aa_renderparams *params;
	aa_palette palette;
	unsigned char *bitmap;

	context = aa_autoinit (&aa_defparams);
	if (!context)
		return (GP_ERROR);
	aa_autoinitkbd (context, 0);
	params = aa_getrenderparams ();
	bitmap = aa_image (context);
	aa_hidecursor (context);

	result = gp_camera_capture_preview (camera, file);
	if (result < 0)
		return (result);

	while (1) {
		const char *data, *type;
		long int size;

		gp_file_get_data_and_size (file, &data, &size);
		gp_file_get_mime_type (file, &type);

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
				aa_close (context);
				return (GP_ERROR);
			}

			cinfo.err = jpeg_std_error (&pub);
			jpeg_create_decompress (&cinfo);
			jpeg_stdio_src (&cinfo, f);
			jpeg_read_header (&cinfo, TRUE);
			while (aa_imgwidth (context) <
				(cinfo.image_width / cinfo.scale_denom))
				cinfo.scale_denom *= 2;
			while (aa_imgheight (context) <
				(cinfo.image_height / cinfo.scale_denom))
				cinfo.scale_denom *= 2;

			jpeg_start_decompress (&cinfo);

			cinfo.do_fancy_upsampling = FALSE;
			cinfo.do_block_smoothing = FALSE;
			cinfo.out_color_space = JCS_GRAYSCALE;

			dptr = bitmap;
			while (cinfo.output_scanline < cinfo.output_height) {
				lptr = lines;
				for (i = 0; i < cinfo.rec_outbuf_height; i++) {
					*lptr++ = dptr;
					dptr += aa_imgwidth (context);
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
			aa_close (context);
			return (GP_OK);
		}

		aa_renderpalette (context, palette, params, 0, 0,
			aa_scrwidth (context), aa_scrheight (context));
		aa_flush (context);
		
		event = aa_getevent (context, 1);
		switch (event) {
		case 32:
			/* Space */
			break;
		case 305:
			/* ESC */
			aa_close (context);
			return (GP_ERROR);
		default:
			aa_close (context);
			return (GP_OK);
		}
	}

	aa_close (context);

	return (GP_OK);
}
