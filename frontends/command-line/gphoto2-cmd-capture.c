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
#include <string.h>

#include <aalib.h>

#include <gphoto2-port-log.h>

#ifdef HAVE_JPEG
#include <jpeglib.h>
#include <unistd.h>
#endif

#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define _(String) (String)
#  define N_(String) (String)
#endif

#ifndef MAX
#define MAX(x, y) (((x)>(y))?(x):(y))
#endif
#ifndef MIN
#define MIN(x, y) (((x)<(y))?(x):(y))
#endif

int
gp_cmd_capture_preview (Camera *camera, CameraFile *file, GPContext *context)
{
	int result, event, contrast, bright;
	aa_context *c;
	aa_renderparams *params;

	c = aa_autoinit (&aa_defparams);
	if (!c)
		return (GP_ERROR);
	aa_autoinitkbd (c, 0);
	params = aa_getrenderparams ();
	contrast = params->contrast;
	bright = params->bright;
	aa_hidecursor (c);

	result = gp_camera_capture_preview (camera, file, context);
	if (result < 0)
		return (result);

	while (1) {
		const char *data, *type;
		long int size;
		unsigned char *bitmap;

		gp_file_get_data_and_size (file, &data, &size);
		gp_file_get_mime_type (file, &type);
		bitmap = aa_image (c);

#ifdef HAVE_JPEG
		if (!strcmp (type, GP_MIME_JPEG)) {
			struct jpeg_decompress_struct cinfo;
			struct jpeg_error_mgr pub;
			int i,fd;
			unsigned char *dptr, **lptr, *lines[4];
			FILE *f;
			char tempname[64];

			/* Create a unique temporary file to download into */
			strcpy(tempname,"/tmp/gphoto.XXXXXX");
			fd = mkstemp(tempname);
			if (fd < 0)
				return (GP_ERROR);
			close(fd);

			gp_file_save (file, tempname);
			f = fopen (tempname, "rb");
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
			unlink(tempname);
		} else
#endif
		{
			/* Silently skip the preview */
			aa_close (c);
			return (GP_OK);
		}

		aa_render (c, params, 0, 0,
			aa_scrwidth (c), aa_scrheight (c));
		aa_flush (c);
		
		event = aa_getevent (c, 0);
		switch (event) {
		case 105:
			/* i */
			params->inversion = 1 - params->inversion;
			break;
		case 300:
			/* Up arrow */
			params->bright = MIN (255, params->bright + 1);
			break;
		case 301:
			/* Down arrow */
			params->bright = MAX (0, params->bright - 1);
			break;
		case 302:
			/* Left arrow */
			params->contrast = MAX (0, params->contrast - 1);
			break;
		case 303:
			/* Right arrow */
			params->contrast = MIN (255, params->contrast + 1);
			break;
		case 114:
			/* r */
			params->bright = bright;
			params->contrast = contrast;
			break;
		case AA_RESIZE:
			aa_resize (c);
			aa_flush (c);
			break;
		case AA_NONE:
		case 32:
			/* Space */
			result = gp_camera_capture_preview (camera, file,
							    context);
			if (result < 0) {
				aa_close (c);
				return (result);
			}
			break;
		case 305:
			/* ESC */
			aa_close (c);
			gp_context_error (context, _("Operation cancelled"));
			return (GP_ERROR_CANCEL);
		default:
			aa_close (c);
			return (GP_OK);
		}
	}

	aa_close (c);

	return (GP_OK);
}
