/* file.c
 *
 * Copyright (C) 2000 Scott Fritzinger
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

#include "gphoto2-file.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "gphoto2-result.h"

static int glob_session_file = 0;

#define CHECK_NULL(r)        {if (!(r)) return (GP_ERROR_BAD_PARAMETERS);}
#define CHECK_RESULT(result) {int r = (result); if (r < 0) return (r);}
#define CHECK_MEM(m)         {if (!(m)) return (GP_ERROR_NO_MEMORY);}

int
gp_file_new (CameraFile **file)
{
	CHECK_NULL (file);

	*file = malloc (sizeof (CameraFile));
	if (!*file)
		return (GP_ERROR_NO_MEMORY);

	(*file)->type = GP_FILE_TYPE_NORMAL;
	strcpy ((*file)->mime_type, "unknown/unknown");
	strcpy ((*file)->name, "");
	(*file)->data = NULL;
	(*file)->size = 0;
	(*file)->bytes_read = 0;
	(*file)->session = glob_session_file++;
	(*file)->ref_count = 1;

	(*file)->red_table   = NULL;
	(*file)->green_table = NULL;
	(*file)->blue_table  = NULL;
	(*file)->red_size   = 0;
	(*file)->green_size = 0;
	(*file)->blue_size  = 0;

	return (GP_OK);
}

int gp_file_free (CameraFile *file)
{
	CHECK_NULL (file);
	
	CHECK_RESULT (gp_file_clean (file));
	
	free (file);
	
	return (GP_OK);
}

int
gp_file_ref (CameraFile *file)
{
	CHECK_NULL (file);

	file->ref_count += 1;
	
	return (GP_OK);
}

int
gp_file_unref (CameraFile *file)
{
	CHECK_NULL (file);
	
	file->ref_count -= 1;

	if (file->ref_count == 0)
		CHECK_RESULT (gp_file_free (file));

	return (GP_OK);
}

int gp_file_session (CameraFile *file)
{
	CHECK_NULL (file);

        return (file->session);
}

int
gp_file_append (CameraFile *file, const char *data, int size)
{
        char *t;

	CHECK_NULL (file);

        if (size < 0)
                return (GP_ERROR_BAD_PARAMETERS);

        if (!file->data)
		file->data = malloc (sizeof(char) * (size));
        else {
		t = realloc (file->data, sizeof (char) * (file->size + size));
		if (!t)
			return GP_ERROR_NO_MEMORY;
		file->data = t;
        }
        memcpy (&file->data[file->size], data, size);

        file->bytes_read = size;
        file->size += size;

        return (GP_OK);
}

int
gp_file_set_data_and_size (CameraFile *file, char *data, long int size)
{
	CHECK_NULL (file);

	if (file->data)
		free (file->data);
	file->data = data;
	file->size = size;
	file->bytes_read = size;

	return (GP_OK);
}

int
gp_file_get_last_chunk (CameraFile *file, char **data, long int *size)
{
	CHECK_NULL (file && data && size);

        if (file->bytes_read == 0) {
                /* chunk_add was never called. return safely. */
                *data = NULL;
                *size = 0;
                return (GP_ERROR);
        }

        /* They must free the returned data! */
        *data = malloc(file->bytes_read);
        memcpy (*data, &file->data[file->size - file->bytes_read],
		file->bytes_read);
        *size = file->bytes_read;

        return (GP_OK);
}

int
gp_file_get_data_and_size (CameraFile *file, const char **data, long int *size)
{
	CHECK_NULL (file && data && size);

	*data = file->data;
	*size = file->size;

	return (GP_OK);
}

int
gp_file_save (CameraFile *file, char *filename)
{
        FILE *fp;

	CHECK_NULL (file && filename);

        if (!(fp = fopen (filename, "wb")))
                return (GP_ERROR);
	fwrite (file->data, (size_t)sizeof(char), (size_t)file->size, fp);
        fclose (fp);

        return (GP_OK);
}

int
gp_file_open (CameraFile *file, char *filename)
{
        FILE *fp;
        char *name, *dot;
        long size, size_read;
        int  i;
        /*
         * mime types that cannot be determined by the filename
         * extension. Better hack would be to use library that examine
         * file content instead, like gnome-vfs mime handling, or
         * gnome-mime, whatever.
         */
        static char *mime_table[] = {
            "jpg",  "jpeg",
            "tif",  "tiff",
            "ppm",  "x-portable-pixmap",
            "pgm",  "x-portable-graymap",
            "pbm",  "x-portable-bitmap",
            "png",  "png",
            NULL};

	CHECK_NULL (file && filename);

	CHECK_RESULT (gp_file_clean (file));

        fp = fopen(filename, "r");
        if (!fp)
		return (GP_ERROR);
	fseek (fp, 0, SEEK_END);
        size = ftell (fp);
        rewind (fp);

        file->data = malloc (sizeof(char)*(size + 1));
        if (!file->data)
                return (GP_ERROR_NO_MEMORY);
	size_read = fread (file->data, (size_t)sizeof(char), (size_t)size, fp);
	if (ferror(fp)) {
		gp_file_clean (file);
                return (GP_ERROR);
        }
        fclose(fp);

        file->size = size_read;
        file->data[size_read] = 0;

        name = strrchr (filename, '/');
        if (name)
                strncpy (file->name, name + 1, sizeof (file->name));
           else
                strncpy (file->name, filename, sizeof (file->name));

        /* MIME lookup */
        dot = strrchr (filename, '.');
        if (dot) {
            for (i = 0; mime_table[i] ; i+=2)
#ifdef OS2
                if (!stricmp (mime_table[i], dot+1)) {
#else
                if (!strcasecmp (mime_table[i], dot+1)) {
#endif
                    sprintf (file->mime_type,"image/%s", mime_table[i+1]);
                    break;
                }
            if (!mime_table[i])
                /*
                 * We did not found the type in the lookup table,
                 * so we use the file suffix as mime type.
                 */
                sprintf(file->mime_type, "image/%s", dot + 1);
        } else
            /*
             * Damn, no filename suffix...
             */
            strncpy (file->mime_type, "image/unknown",
		     sizeof (file->mime_type));

        return (GP_OK);
}

int
gp_file_clean (CameraFile *file)
{
        /* 
	 * Frees a CameraFile's components, not the CameraFile itself.
	 * This is used to prep a CameraFile struct to be filled.
         */

	CHECK_NULL (file);

        if (file->data != NULL)
                free(file->data);
        file->data = NULL;

        file->size = 0;
        file->bytes_read = 0;

	strcpy (file->name, "");
	strcpy (file->header, "");

	if (file->red_table) {
		free (file->red_table);
		file->red_table = NULL;
	}
	if (file->green_table) {
		free (file->green_table);
		file->green_table = NULL;
	}
	if (file->blue_table) {
		free (file->blue_table);
		file->blue_table = NULL;
	}
	file->red_size   = 0;
	file->blue_size  = 0;
	file->green_size = 0;

        return (GP_OK);
}

int
gp_file_copy (CameraFile *destination, CameraFile *source)
{
	CHECK_NULL (destination && source);

	if (destination->data)
		free (destination->data);
	destination->data = malloc (sizeof (char) * source->size);
	if (!destination->data)
		return (GP_ERROR_NO_MEMORY);

	memcpy (destination->data, source->data, source->size);
	destination->size = source->size;

	return (GP_OK);
}

int
gp_file_get_name (CameraFile *file, const char **name)
{
	CHECK_NULL (file && name);

	*name = file->name;

	return (GP_OK);
}

int
gp_file_get_mime_type (CameraFile *file, const char **mime_type)
{
	CHECK_NULL (file && mime_type);

	*mime_type = file->mime_type;

	return (GP_OK);
}

int
gp_file_set_name (CameraFile *file, const char *name)
{
	CHECK_NULL (file && name);

	strncpy (file->name, name, sizeof (file->name));

	return (GP_OK);
}

int
gp_file_set_mime_type (CameraFile *file, const char *mime_type)
{
	CHECK_NULL (file && mime_type);

	strncpy (file->mime_type, mime_type, sizeof (file->mime_type));

	return (GP_OK);
}

int
gp_file_set_type (CameraFile *file, CameraFileType type)
{
	CHECK_NULL (file);

	file->type = type;

	return (GP_OK);
}

int
gp_file_get_type (CameraFile *file, CameraFileType *type)
{
	CHECK_NULL (file && type);

	*type = file->type;

	return (GP_OK);
}

int
gp_file_set_color_table (CameraFile *file,
			 const unsigned char *red_table,   int red_size,
			 const unsigned char *green_table, int green_size,
			 const unsigned char *blue_table,  int blue_size)
{
	CHECK_NULL (file && red_table && green_table && blue_table);

	if (red_size) {
		CHECK_MEM (file->red_table = malloc (sizeof (char) * red_size));
		memcpy (file->red_table, red_table, red_size);
	}
	if (green_size) {
		CHECK_MEM (file->green_table = malloc(sizeof(char)*green_size));
		memcpy (file->green_table, green_table, green_size);
	}
	if (blue_size) {
		CHECK_MEM (file->blue_table = malloc (sizeof (char)*blue_size));
		memcpy (file->blue_table, blue_table, blue_size);
	}

	file->red_size   = red_size;
	file->green_size = green_size;
	file->blue_size  = blue_size;

	return (GP_OK);
}

int
gp_file_set_width_and_height (CameraFile *file, int width, int height)
{
	CHECK_NULL (file);

	file->width = width;
	file->height = height;

	return (GP_OK);
}

int
gp_file_set_header (CameraFile *file, const char *header)
{
	CHECK_NULL (file && header);

	strncpy (file->header, header, sizeof (file->header));

	return (GP_OK);
}

int
gp_file_set_conversion_method (CameraFile *file,
			       CameraFileConversionMethod method)
{
	CHECK_NULL (file);

	file->method = method;

	return (GP_OK);
}

static int
gp_file_conversion_chuck (CameraFile *file, unsigned char *data)
{
	int x, y;
	int p1, p2, p3, p4;
	int red, green, blue;

	/*
	 * Added by Chuck -- 12-May-2000
	 * Convert the colors based on location, and my wacky color table:
	 *
	 * Convert the 4 cells into a single RGB pixel.
	 * Colors are encoded as follows:
	 * 
	 * 		000000000000000000........33
	 * 		000000000011111111........11
	 * 		012345678901234567........89
	 * 		----------------------------
	 * 	000     RGRGRGRGRGRGRGRGRG........RG
	 * 	001     GBGBGBGBGBGBGBGBGB........GB
	 * 	002     RGRGRGRGRGRGRGRGRG........RG
	 * 	003     GBGBGBGBGBGBGBGBGB........GB
	 * 	...     ............................
	 * 	238     RGRGRGRGRGRGRGRGRG........RG
	 * 	239     GBGBGBGBGBGBGBGBGB........GB
	 *
	 * NOTE. Above is 320x240 resolution.  Just expand for 640x480.
	 *
	 * Use neighboring cells to generate color values for each pixel.
	 * The neighbors are the (x-1, y-1), (x, y-1), (x-1, y), and (x, y).
	 * The exception is when x or y are zero then the neighors used are
	 * the +1 cells.
	 */
	
	for (y = 0;y < file->height; y++)
		for (x = 0;x < file->width; x++) {
			p1 = ((y==0?y+1:y-1)*file->width) + (x==0?x+1:x-1);
			p2 = ((y==0?y+1:y-1)*file->width) +  x;
			p3 = ( y            *file->width) + (x==0?x+1:x-1);
			p4 = ( y            *file->width) +  x;

			switch (((y & 1) << 1) + (x & 1)) {
			case 0: /* even row, even col, red */
				blue  = file->blue_table [file->data[p1]];
				green = file->green_table[file->data[p2]];
				green+= file->green_table[file->data[p3]];
				red   = file->red_table  [file->data[p4]];
				break;
			case 1: /* even row, odd col, green */
				green = file->green_table[file->data[p1]];
				blue  = file->blue_table [file->data[p2]];
				red   = file->red_table  [file->data[p3]];
				green+= file->green_table[file->data[p4]];
				break;
			case 2: /* odd row, even col, green */
				green = file->green_table[file->data[p1]];
				red   = file->red_table  [file->data[p2]];
				blue  = file->blue_table [file->data[p3]];
				green+=file->green_table [file->data[p4]];
				break;
			case 3: /* odd row, odd col, blue */
			default:
				red   = file->red_table  [file->data[p1]];
				green = file->green_table[file->data[p2]];
				green+= file->green_table[file->data[p3]];
				blue  = file->blue_table [file->data[p4]];
				break;
			}
			*data++ = (unsigned char) red;
			*data++ = (unsigned char) (green/2);
			*data++ = (unsigned char) blue;
		}

	return (GP_OK);
}

static int
gp_file_conversion_johannes (CameraFile *file, unsigned char *data)
{
	unsigned char *ps;
	int x, y;

	/* Skip over the thumbnail header */
	ps = file->data + 16;

	for (y = 0; y < file->height; y++) {
		char *pd =  data + (file->width * y * 3);
		for (x = 0; x < file->width / 2; x++) {
			int _y, u, y1, v, r, g, b;

#define LIMIT(_x) ((((_x)>0xffffff)?0xff0000:(((_x)<=0xffff)?0:(_x)&0xff0000))>>16)
			u =  *ps++ - 128;
			_y =  *ps++ - 16;
			v =  *ps++ - 128;
			y1 = *ps++ - 16;
			r = 104635 * v;
			g = -25690 * u + -53294 * v;
			b = 132278 * u;
			_y  *= 76310;
			y1 *= 76310;
			*pd++ = LIMIT(r + _y);
			*pd++ = LIMIT(g + _y);
			*pd++ = LIMIT(b + _y);
			*pd++ = LIMIT(r + y1);
			*pd++ = LIMIT(g + y1);
			*pd++ = LIMIT(b + y1);
		}
	}

	return (GP_OK);
}

static int
gp_file_raw_to_pnm (CameraFile *file)
{
	unsigned char *new_data, *b;
	long int new_size;
	int result;

	CHECK_NULL (file);

	new_size = (file->width * file->height * 3) + strlen (file->header);
	CHECK_MEM (new_data = malloc (sizeof (char) * new_size));

	strcpy (new_data, file->header);

	b = new_data + strlen (file->header);
	switch (file->method) {
	case GP_FILE_CONVERSION_METHOD_CHUCK:
		result = gp_file_conversion_chuck (file, b);
		break;
	default:
		result = GP_ERROR_NOT_SUPPORTED;
		break;
	}
	if (result != GP_OK) {
		free (new_data);
		return (result);
	}

	CHECK_RESULT (gp_file_set_data_and_size (file, new_data, new_size));
	CHECK_RESULT (gp_file_set_mime_type (file, GP_MIME_PNM));
	
	return (GP_OK);
}

static int
gp_file_raw_to_ppm (CameraFile *file)
{
	unsigned char *new_data, *b;
	long int new_size;
	int result;

	CHECK_NULL (file);

	new_size = (file->width * file->height * 3) + strlen (file->header);
	CHECK_MEM (new_data = malloc (sizeof (char) * new_size));

	strcpy (new_data, file->header);

	b = new_data + strlen (file->header);
	switch (file->method) {
	case GP_FILE_CONVERSION_METHOD_JOHANNES:
		result = gp_file_conversion_johannes (file, b);
		break;
	default:
		result = GP_ERROR_NOT_SUPPORTED;
		break;
	}
	if (result != GP_OK) {
		free (new_data);
		return (result);
	}

	CHECK_RESULT (gp_file_set_data_and_size (file, new_data, new_size));
	CHECK_RESULT (gp_file_set_mime_type (file, GP_MIME_PPM));

	return (GP_OK);
}

int
gp_file_convert (CameraFile *file, const char *mime_type)
{
	CHECK_NULL (file && mime_type);

	if (!strcmp (file->mime_type, GP_MIME_RAW) &&
	    !strcmp (mime_type, GP_MIME_PNM))
		return (gp_file_raw_to_pnm (file));

	else if (!strcmp (file->mime_type, GP_MIME_RAW) &&
		 !strcmp (mime_type, GP_MIME_PPM))
		return (gp_file_raw_to_ppm (file));

	else
		return (GP_ERROR_NOT_SUPPORTED);
}
