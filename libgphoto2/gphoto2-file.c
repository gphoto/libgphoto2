/** \file
 *
 * \author Copyright 2000 Scott Fritzinger
 *
 * \note
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * \note
 * This library is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details. 
 *
 * \note
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * \note
 * This file contains internal functions. Use of these functions from
 * external software modules is considered <strong>deprecated</strong>.
 */

#include "config.h"
#include <gphoto2/gphoto2-file.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <utime.h>

#include <gphoto2/gphoto2-port-log.h>
#include <gphoto2/gphoto2-port-portability.h>

#include <gphoto2/gphoto2-result.h>

#define CHECK_NULL(r)        {if (!(r)) return (GP_ERROR_BAD_PARAMETERS);}
#define CHECK_RESULT(result) {int r = (result); if (r < 0) return (r);}
#define CHECK_MEM(m)         {if (!(m)) return (GP_ERROR_NO_MEMORY);}

/* lengt of one path component */
#ifndef MAX_PATH
# define MAX_PATH 256
#endif

/*! The internals of the CameraFile struct are private.
 * \internal
 */
struct _CameraFile {
        CameraFileType type;
        char mime_type [64];
        char name [MAX_PATH];
        unsigned long int size;
        unsigned char *data;
        long bytes_read;
        int ref_count;

        time_t mtime;
};


/*! Create new #CameraFile object.
 *
 * \param file a pointer to a #CameraFile
 * \return a gphoto2 error code.
 */
int
gp_file_new (CameraFile **file)
{
	CHECK_NULL (file);

	*file = malloc (sizeof (CameraFile));
	if (!*file)
		return (GP_ERROR_NO_MEMORY);
	memset (*file, 0, sizeof (CameraFile));

	(*file)->type = GP_FILE_TYPE_NORMAL;
	strcpy ((*file)->mime_type, "unknown/unknown");
	(*file)->ref_count = 1;

	return (GP_OK);
}


/*! \brief descruct a #CameraFile object.
 * @param file a #CameraFile
 * @return a gphoto2 error code.
 *
 **/
int gp_file_free (CameraFile *file)
{
	CHECK_NULL (file);
	
	CHECK_RESULT (gp_file_clean (file));
	
	free (file);
	
	return (GP_OK);
}


/*! \brief Increase reference counter for #CameraFile object
 *
 * \param file a #CameraFile
 * \return a gphoto2 error code.
 */
int
gp_file_ref (CameraFile *file)
{
	CHECK_NULL (file);

	file->ref_count += 1;
	
	return (GP_OK);
}


/*! \brief Decrease reference counter for #CameraFile object
 *
 * \param file a #CameraFile
 * \return a gphoto2 error code.
 *
 **/
int
gp_file_unref (CameraFile *file)
{
	CHECK_NULL (file);
	
	file->ref_count -= 1;

	if (file->ref_count == 0)
		CHECK_RESULT (gp_file_free (file));

	return (GP_OK);
}


/**
 * @param file a #CameraFile
 * @param data
 * @param size
 * @return a gphoto2 error code.
 *
 **/
int
gp_file_append (CameraFile *file, const char *data, 
		unsigned long int size)
{
        char *t;

	CHECK_NULL (file);

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


/**
 * @param file a #CameraFile
 * @param data
 * @param size
 * @return a gphoto2 error code.
 *
 **/
int
gp_file_set_data_and_size (CameraFile *file, char *data,
			   unsigned long int size)
{
	CHECK_NULL (file);

	if (file->data)
		free (file->data);
	file->data = data;
	file->size = size;
	file->bytes_read = size;

	return (GP_OK);
}


/**
 * Get a pointer to the data and the file's size.
 *
 * @param file a #CameraFile
 * @param data
 * @param size
 * @return a gphoto2 error code.
 *
 * Both data and size can be NULL and will then be ignored.
 *
 **/
int
gp_file_get_data_and_size (CameraFile *file, const char **data,
			   unsigned long int *size)
{
	CHECK_NULL (file);

	if (data)
		*data = file->data;
	if (size)
		*size = file->size;

	return (GP_OK);
}


/**
 * @param file a #CameraFile
 * @param filename
 * @return a gphoto2 error code.
 *
 **/
int
gp_file_save (CameraFile *file, const char *filename)
{
	FILE *fp;
	struct utimbuf u;

	CHECK_NULL (file && filename);

	if (!(fp = fopen (filename, "wb")))
		return (GP_ERROR);

	if (fwrite (file->data, (size_t)sizeof(char), (size_t)file->size, fp) != (size_t)file->size) {
		gp_log (GP_LOG_ERROR, "libgphoto2",
			"Not enough space on device in "
			"order to save '%s'.", filename);
		unlink (filename);
		return (GP_ERROR);
	}

	fclose (fp);

	if (file->mtime) {
		u.actime = file->mtime;
		u.modtime = file->mtime;
		utime (filename, &u);
	}

	return (GP_OK);
}


/**
 * @param file a #CameraFile
 * @param filename
 * @return a gphoto2 error code.
 *
 **/
int
gp_file_open (CameraFile *file, const char *filename)
{
        FILE *fp;
        char *name, *dot;
        long size, size_read;
        int  i;
	struct stat s;

        /*
         * mime types that cannot be determined by the filename
         * extension. Better hack would be to use library that examine
         * file content instead, like gnome-vfs mime handling, or
         * gnome-mime, whatever.
         * See also the GP_MIME_* definitions.
         */
        static char *mime_table[] = {
            "bmp",  GP_MIME_BMP,
            "jpg",  GP_MIME_JPEG,
            "tif",  GP_MIME_TIFF,
            "ppm",  GP_MIME_PPM,
            "pgm",  GP_MIME_PGM,
            "pnm",  GP_MIME_PNM,
            "png",  GP_MIME_PNG,
            "wav",  GP_MIME_WAV,
            "avi",  GP_MIME_AVI,
            "mp3",  GP_MIME_MP3,
            "wma",  GP_MIME_WMA,
            "asf",  GP_MIME_ASF,
            "ogg",  GP_MIME_OGG,
            "mpg",  GP_MIME_MPEG,
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
                if (!strcasecmp (mime_table[i], dot+1)) {
                    strncpy (file->mime_type, mime_table[i+1], sizeof(file->mime_type));
                    break;
                }
            if (!mime_table[i])
                /*
                 * We did not found the type in the lookup table,
                 * so we use the file suffix as mime type.
                 * Note: This should probably use GP_MIME_UNKNOWN instead
                 * of returning a non-standard type.
                 */
                sprintf(file->mime_type, "image/%s", dot + 1);
        } else
            /*
             * Damn, no filename suffix...
             */
            strncpy (file->mime_type, GP_MIME_UNKNOWN,
		     sizeof (file->mime_type));

	if (stat (filename, &s) != -1) {
		file->mtime = s.st_mtime;
	} else {
		file->mtime = time (NULL);
	}

        return (GP_OK);
}


/**
 * @param file a #CameraFile
 * @return a gphoto2 error code.
 *
 **/
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
        return (GP_OK);
}

/**
 * @param destination a #CameraFile
 * @param source a #CameraFile
 * @return a gphoto2 error code.
 *
 **/
int
gp_file_copy (CameraFile *destination, CameraFile *source)
{
	int ref_count;

	CHECK_NULL (destination && source);

	gp_log (GP_LOG_DEBUG, "gphoto2-file", "Copying '%s' onto '%s'...",
		source->name, destination->name);

	ref_count = destination->ref_count;
	if (destination->data) {
		free (destination->data);
		destination->data = NULL;
	}

	memcpy (destination, source, sizeof (CameraFile));
	destination->ref_count = ref_count;

	destination->data = malloc (sizeof (char) * source->size);
	if (!destination->data)
		return (GP_ERROR_NO_MEMORY);
	memcpy (destination->data, source->data, source->size);

	return (GP_OK);
}

/**
 * @param file a #CameraFile
 * @param name a pointer to a name string
 * @return a gphoto2 error code.
 *
 **/
int
gp_file_get_name (CameraFile *file, const char **name)
{
	CHECK_NULL (file && name);

	*name = file->name;

	return (GP_OK);
}


/**
 * @param file a #CameraFile
 * @param mime_type a pointer to a MIME type string
 * @return a gphoto2 error code.
 *
 **/
int
gp_file_get_mime_type (CameraFile *file, const char **mime_type)
{
	CHECK_NULL (file && mime_type);

	*mime_type = file->mime_type;

	return (GP_OK);
}


/**
 * @param file a #CameraFile
 * @param name a pointer to a MIME type string
 * @return a gphoto2 error code.
 *
 **/
int
gp_file_set_name (CameraFile *file, const char *name)
{
	CHECK_NULL (file && name);

	strncpy (file->name, name, sizeof (file->name));

	return (GP_OK);
}


/**
 * @param file a #CameraFile
 * @param mime_type a MIME type string
 * @return a gphoto2 error code.
 *
 **/
int
gp_file_set_mime_type (CameraFile *file, const char *mime_type)
{
	CHECK_NULL (file && mime_type);

	strncpy (file->mime_type, mime_type, sizeof (file->mime_type));

	return (GP_OK);
}


/**
 * @param file a #CameraFile
 * @return a gphoto2 error code.
 *
 **/
int
gp_file_detect_mime_type (CameraFile *file)
{
	const char TIFF_SOI_MARKER[] = {(char) 0x49, (char) 0x49, (char) 0x2A,
				        (char) 0x00, (char) 0x08, '\0' };
	const char JPEG_SOI_MARKER[] = {(char) 0xFF, (char) 0xD8, '\0' };

	CHECK_NULL (file);

	/* image/tiff */
	if ((file->size >= 5) && !memcmp (file->data, TIFF_SOI_MARKER, 5))
		CHECK_RESULT (gp_file_set_mime_type (file, GP_MIME_TIFF))

	/* image/jpeg */
	else if ((file->size >= 2) && !memcmp (file->data, JPEG_SOI_MARKER, 2))
		CHECK_RESULT (gp_file_set_mime_type (file, GP_MIME_JPEG))

	else
		CHECK_RESULT (gp_file_set_mime_type (file, GP_MIME_RAW));
	
	return (GP_OK);
}


/**
 * @param file a #CameraFile
 * @return a gphoto2 error code.
 *
 **/
int
gp_file_adjust_name_for_mime_type (CameraFile *file)
{
	int x;
	char *suffix;
	const char *table[] = {
		GP_MIME_RAW,  "raw",
		GP_MIME_JPEG, "jpg",
		GP_MIME_PNG,  "png",
		GP_MIME_PPM,  "ppm",
		GP_MIME_PGM,  "pgm",
		GP_MIME_PNM,  "pnm",
		GP_MIME_TIFF, "tif",
		GP_MIME_WAV,  "wav",
		GP_MIME_BMP,  "bmp",
		GP_MIME_AVI,  "avi",
		NULL};

	CHECK_NULL (file);

	gp_log (GP_LOG_DEBUG, "gphoto2-file", "Adjusting file name for "
		"mime type '%s'...", file->mime_type);
	for (x = 0; table[x]; x += 2)
                if (!strcmp (file->mime_type, table[x])) {

			/* Search the current suffix and erase it */
#ifdef HAVE_STRCHR
			suffix = strrchr (file->name, '.');
#else
			suffix = rindex (file->name, '.');
#endif
			if (suffix++)
				*suffix = '\0';
			strcat (file->name, table[x + 1]);
			break;
		}
	gp_log (GP_LOG_DEBUG, "gphoto2-file", "Name adjusted to '%s'.",
		file->name);
	
	return (GP_OK);
}


/**
 * @param file a #CameraFile
 * @param type a #CameraFileType
 * @return a gphoto2 error code.
 *
 **/
int
gp_file_set_type (CameraFile *file, CameraFileType type)
{
	CHECK_NULL (file);

	file->type = type;

	return (GP_OK);
}


/**
 * @param file a #CameraFile
 * @param type a #CameraFileType
 * @return a gphoto2 error code.
 *
 **/
int
gp_file_get_type (CameraFile *file, CameraFileType *type)
{
	CHECK_NULL (file && type);

	*type = file->type;

	return (GP_OK);
}


/**
 * @param file a #CameraFile
 * @param mtime
 * @return a gphoto2 error code.
 *
 **/
int
gp_file_get_mtime (CameraFile *file, time_t *mtime)
{
	CHECK_NULL (file && mtime);

	*mtime = file->mtime;

	return (GP_OK);
}


/**
 * @param file a #CameraFile
 * @param mtime
 * @return a gphoto2 error code.
 *
 **/
int
gp_file_set_mtime (CameraFile *file, time_t mtime)
{
	CHECK_NULL (file);

	file->mtime = mtime;

	return (GP_OK);
}
