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

int
gp_file_new (CameraFile **file)
{
	CHECK_NULL (file);

	*file = malloc (sizeof (CameraFile));
	if (!*file)
		return (GP_ERROR_NO_MEMORY);

	strcpy ((*file)->type, "unknown/unknown");
	strcpy ((*file)->name, "");
	(*file)->data = NULL;
	(*file)->size = 0;
	(*file)->bytes_read = 0;
	(*file)->session = glob_session_file++;
	(*file)->ref_count = 1;

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
gp_file_append (CameraFile *file, char *data, int size)
{
        char *t;

	CHECK_NULL (file);

        if (size < 0)
                return (GP_ERROR_BAD_PARAMETERS);

        if (!file->data)
		file->data = malloc(sizeof(char) * (size));
        else {
		t = realloc (file->data, sizeof(char) * (file->size + size));
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
gp_file_get_last_chunk (CameraFile *file, char **data, int *size)
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
            "png",  "x-png",
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
                strcpy(file->name, name + 1);
           else
                strcpy(file->name, filename);

        /* MIME lookup */
        dot = strrchr (filename, '.');
        if (dot) {
            for (i = 0; mime_table[i] ; i+=2)
#ifdef OS2
                if (!stricmp (mime_table[i], dot+1)) {
#else
                if (!strcasecmp (mime_table[i], dot+1)) {
#endif
                    sprintf (file->type,"image/%s", mime_table[i+1]);
                    break;
                }
            if (!mime_table[i])
                /*
                 * We did not found the type in the lookup table,
                 * so we use the file suffix as mime type.
                 */
                sprintf(file->type, "image/%s", dot + 1);
        } else
            /*
             * Damn, no filename suffix...
             */
            strcpy(file->type, "image/unknown");

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
        strcpy (file->name, "");
        file->data = NULL;
        file->size = 0;
        file->bytes_read = 0;

        return(GP_OK);
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
gp_file_set_name (CameraFile *file, const char *name)
{
	CHECK_NULL (file && name);

	strcpy (file->name, name);

	return (GP_OK);
}

int
gp_file_set_type (CameraFile *file, const char *type)
{
	CHECK_NULL (file && type);

	strcpy (file->type, type);

	return (GP_OK);
}
